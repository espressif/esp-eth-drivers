/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
// Standard C library headers
#include <string.h>
#include <stdlib.h>
#include <sys/cdefs.h>
#include <inttypes.h>
#include <endian.h>

// ESP-IDF headers
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_cpu.h"
#include "esp_timer.h"
#include "sdkconfig.h"

// Driver headers
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_private/gpio.h"
#include "soc/io_mux_reg.h"

// FreeRTOS headers
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

// Component headers
#include "esp_eth_mac_spi.h"
#include "esp_eth_mac_lan865x.h"
#include "lan865x_reg.h"

static const char *TAG = "lan865x.mac";

#define LAN865X_READ_REG                (0)
#define LAN865X_WRITE_REG               (1)

#define LAN865X_DUMMY_OFFSET            (4)
#define LAN865X_HEADER_FOOTER_SIZE      (4)
#define LAN865X_DATA_BLOCK_SIZE         (64)

#define LAN865X_HASH_FILTER_TABLE_SIZE  (64)

#define LAN865X_RX_BUFFER_SIZE (((ETH_MAX_PACKET_SIZE + LAN865X_DATA_BLOCK_SIZE - 1) / LAN865X_DATA_BLOCK_SIZE) * LAN865X_DATA_BLOCK_SIZE + LAN865X_HEADER_FOOTER_SIZE)
#define LAN865X_SPI_BUFFER_SIZE (LAN865X_HEADER_FOOTER_SIZE + LAN865X_DATA_BLOCK_SIZE + LAN865X_DUMMY_OFFSET)
// use same size as for data block so we can use the same buffer for both data and control blocks
#define LAN865X_SPI_MAX_CTRL_BLOCK_SIZE (LAN865X_DATA_BLOCK_SIZE)

#define LAN865X_SPI_LOCK_TIMEOUT_MS     (500)
#define LAN865X_SW_RESET_TIMEOUT_MS     (100)

typedef struct {
    union {
        struct {
            uint32_t parity: 1;        /* Parity bit over bits 31:1 */
            uint32_t reserved: 5;       /* Reserved, set to 0 */
            uint32_t tsc: 2;           /* Time Stamp Capture */
            uint32_t ebo: 6;           /* End Byte Offset */
            uint32_t ev: 1;            /* End Valid */
            uint32_t reserved2: 1;     /* Reserved, set to 0 */
            uint32_t swo: 4;           /* Start Word Offset */
            uint32_t sv: 1;            /* Start Valid */
            uint32_t dv: 1;            /* Data Valid */
            uint32_t vs: 2;            /* Vendor Specific, set to 0 */
            uint32_t reserved3: 5;     /* Reserved, set to 0 */
            uint32_t norx: 1;          /* No Receive */
            uint32_t seq: 1;           /* Data Block Sequence */
            uint32_t dnc: 1;           /* Data Not Control, always 1 for data blocks */
        };
        uint32_t val;
    };
} lan865x_tx_header_t;

typedef struct {
    union {
        struct {
            uint32_t parity: 1;        /* Parity bit over bits 31:1 */
            uint32_t txc: 5;           /* Transmit Credits */
            uint32_t rtsp: 1;          /* Receive Timestamp Parity */
            uint32_t rtsa: 1;          /* Receive Timestamp Added */
            uint32_t ebo: 6;           /* End Byte Offset */
            uint32_t ev: 1;            /* End Valid */
            uint32_t fd: 1;            /* Frame Drop */
            uint32_t swo: 4;           /* Start Word Offset */
            uint32_t sv: 1;            /* Start Valid */
            uint32_t dv: 1;            /* Data Valid */
            uint32_t vs: 2;            /* Vendor Specific */
            uint32_t rba: 5;           /* Receive Blocks Available */
            uint32_t sync: 1;          /* Configuration Synchronized */
            uint32_t hdrb: 1;          /* Header Bad */
            uint32_t exst: 1;          /* Extended Status */
        };
        uint32_t val;
    };
} lan865x_rx_footer_t;

typedef struct {
    union {
        struct {
            uint32_t parity: 1;        /* Parity bit over bits 31:1 */
            uint32_t len: 7;           /* Length - number of registers minus one */
            uint32_t addr: 16;         /* Address of first register to access */
            uint32_t mms: 4;           /* Memory Map Selector */
            uint32_t aid: 1;            /* Address Increment disable */
            uint32_t rw: 1;            /* Read/Write */
            uint32_t hdrb: 1;          /* Header Bad */
            uint32_t dnc: 1;           /* Data Not Control, always 0 for control */
        };
        uint32_t val;
    };
} lan865x_control_header_t;

typedef struct {
    lan865x_tx_header_t header;
    uint8_t data[LAN865X_DATA_BLOCK_SIZE];
} __attribute__((packed)) lan865x_tx_block_t;

typedef struct {
    uint8_t data[LAN865X_DATA_BLOCK_SIZE];
    lan865x_rx_footer_t footer;
} __attribute__((packed))lan865x_rx_block_t;

typedef struct {
    lan865x_control_header_t header;
    uint32_t data[];
} __attribute__((packed))lan865x_control_block_t;

typedef struct {
    uint32_t dummy;
    lan865x_control_header_t header;
    uint32_t data[];
} __attribute__((packed))lan865x_control_resp_t;

typedef struct {
    spi_device_handle_t hdl;
} eth_spi_info_t;

typedef struct {
    void *ctx;
    void *(*init)(const void *spi_config);
    esp_err_t (*deinit)(void *spi_ctx);
    esp_err_t (*read)(void *spi_ctx, uint32_t cmd, uint32_t addr, void *data, uint32_t data_len);
    esp_err_t (*write)(void *spi_ctx, uint32_t cmd, uint32_t addr, const void *data, uint32_t data_len);
} eth_spi_custom_driver_t;

typedef struct {
    esp_eth_mac_t parent;
    esp_eth_mediator_t *eth;
    eth_spi_custom_driver_t spi;
    SemaphoreHandle_t spi_lock;
    TaskHandle_t rx_task_hdl;
    uint32_t sw_reset_timeout_ms;
    int int_gpio_num;
    esp_timer_handle_t poll_timer;
    uint32_t poll_period_ms;
    uint8_t *rx_buffer;
    uint8_t *spi_buffer;
    int8_t hash_filter_cnt[LAN865X_HASH_FILTER_TABLE_SIZE];
} emac_lan865x_t;

static inline bool lan865x_spi_lock(emac_lan865x_t *emac)
{
    return xSemaphoreTake(emac->spi_lock, pdMS_TO_TICKS(LAN865X_SPI_LOCK_TIMEOUT_MS)) == pdTRUE;
}

static inline bool lan865x_spi_unlock(emac_lan865x_t *emac)
{
    return xSemaphoreGive(emac->spi_lock) == pdTRUE;
}

static void *lan865x_spi_init(const void *spi_config)
{
    void *ret = NULL;
    ESP_GOTO_ON_FALSE(spi_config, NULL, err, TAG, "invalid spi device configuration");
    eth_lan865x_config_t *lan865x_config = (eth_lan865x_config_t *)spi_config;
    spi_device_interface_config_t devcfg = *(lan865x_config->spi_devcfg);

    ESP_GOTO_ON_FALSE(devcfg.command_bits == 0 && devcfg.address_bits == 0,
                      NULL, err, TAG, "incorrect SPI frame format (command_bits/address_bits)");

    // Allocate SPI info
    eth_spi_info_t *spi_info = NULL;
    spi_info = heap_caps_calloc(1, sizeof(eth_spi_info_t), MALLOC_CAP_DEFAULT);
    ESP_GOTO_ON_FALSE(spi_info, NULL, err, TAG, "no mem for SPI info");
    // Initialize SPI device
    ESP_GOTO_ON_FALSE(spi_bus_add_device(lan865x_config->spi_host_id, &devcfg, &spi_info->hdl) == ESP_OK,
                      NULL, err_spi, TAG, "failed to add SPI device");

    ret = spi_info;
    return ret;

err_spi:
    free(spi_info);
err:
    return NULL;
}

static esp_err_t lan865x_spi_deinit(void *spi_ctx)
{
    esp_err_t ret = ESP_OK;
    eth_spi_info_t *spi = (eth_spi_info_t *)spi_ctx;

    spi_bus_remove_device(spi->hdl);
    free(spi);
    return ret;
}

static esp_err_t lan865x_spi_write(void *spi_ctx, uint32_t cmd, uint32_t addr, const void *value, uint32_t len)
{
    esp_err_t ret = ESP_OK;
    eth_spi_info_t *spi = (eth_spi_info_t *)spi_ctx;

    spi_transaction_t trans = {
        .length = 8 * len,
        .tx_buffer = value,
    };

    ESP_RETURN_ON_ERROR(spi_device_polling_transmit(spi->hdl, &trans), TAG, "spi write failed");

    return ret;
}

static esp_err_t lan865x_spi_read(void *spi_ctx, uint32_t cmd, uint32_t addr, void *value, uint32_t len)
{
    esp_err_t ret = ESP_OK;
    eth_spi_info_t *spi = (eth_spi_info_t *)spi_ctx;

    spi_transaction_t trans = {
        .length = 8 * len,
        .rx_buffer = value,
        .tx_buffer = value,
    };

    ESP_RETURN_ON_ERROR(spi_device_polling_transmit(spi->hdl, &trans), TAG, "spi write-read failed");

    return ret;
}

static bool lan865x_parity(uint32_t value)
{
    value >>= 1; // Skip bit 0

    for (int shift = 16; shift >= 1; shift /= 2) {
        value ^= value >> shift;
    }
    return !(value & 0x1);
}

static esp_err_t lan865x_frame_transmit(emac_lan865x_t *emac, uint8_t *frame, uint32_t length)
{
    esp_err_t ret = ESP_OK;
    // the same memory is used for tx and rx blocks
    lan865x_tx_block_t *tx_block = (lan865x_tx_block_t *)emac->spi_buffer;
    lan865x_rx_block_t *rx_block = (lan865x_rx_block_t *)emac->spi_buffer;

    if (!lan865x_spi_lock(emac)) {
        ESP_LOGE(TAG, "%s: timeout", __func__);
        return ESP_ERR_TIMEOUT;
    }
    int chunks = (length + LAN865X_DATA_BLOCK_SIZE - 1) / LAN865X_DATA_BLOCK_SIZE;
    for (int i = 0; i < chunks; i++) {
        tx_block->header.val = 0; // clear to be reused

        tx_block->header.dnc = 1; // Data, not control
        tx_block->header.dv = 1; // Data valid
        tx_block->header.norx = 1; // No data receive
        if (i == 0) {
            tx_block->header.sv = 1;
            tx_block->header.swo = 0;
        }
        if (i == chunks - 1) {
            tx_block->header.ev = 1;
            tx_block->header.ebo = (length - 1) % LAN865X_DATA_BLOCK_SIZE;
            memcpy(tx_block->data, frame + i * LAN865X_DATA_BLOCK_SIZE, length % LAN865X_DATA_BLOCK_SIZE);
        } else {
            memcpy(tx_block->data, frame + i * LAN865X_DATA_BLOCK_SIZE, LAN865X_DATA_BLOCK_SIZE);
        }
        tx_block->header.parity = lan865x_parity(tx_block->header.val);
        tx_block->header.val = htobe32(tx_block->header.val);

        ESP_GOTO_ON_ERROR(emac->spi.read(emac->spi.ctx, 0, 0, tx_block, sizeof(lan865x_tx_block_t)), err_inv_data, TAG, "spi failed");

        // compute footer parity - should have odd number of 1s
        bool footer_parity = lan865x_parity(rx_block->footer.val);
        // Invalid footer parity indicates potential data corruption at SPI from LAN865X
        ESP_GOTO_ON_FALSE(footer_parity == rx_block->footer.parity, ESP_ERR_INVALID_CRC, err_inv_data, TAG, "footer parity mismatch");
        // Header bad indicates potential data corruption at SPI to LAN865X
        ESP_GOTO_ON_FALSE(rx_block->footer.hdrb == 0, ESP_ERR_INVALID_CRC, err, TAG, "header bad");
    }

err:
    // if there is data to receive, notify the rx_task
    if (rx_block->footer.rba > 0) {
        xTaskNotifyGive(emac->rx_task_hdl);
    }
err_inv_data:
    lan865x_spi_unlock(emac);
    return ret;
}

static esp_err_t lan865x_frame_receive(emac_lan865x_t *emac, uint8_t *frame, uint32_t *length, uint8_t *remain)
{
    esp_err_t ret = ESP_OK;
    uint8_t *rx_buffer_p = frame;
    lan865x_tx_block_t *tx_block;
    lan865x_rx_block_t *rx_block;
    uint8_t blocks_available = 0;
    uint32_t actual_length = 0;
    bool start_found = false;
    if (!lan865x_spi_lock(emac)) {
        ESP_LOGE(TAG, "%s: timeout", __func__);
        return ESP_ERR_TIMEOUT;
    }
    do {
        // check if input buffer is able to fit the next chunk
        if (actual_length + LAN865X_DATA_BLOCK_SIZE + LAN865X_HEADER_FOOTER_SIZE > *length) {
            ESP_LOGW(TAG, "frame truncated");
            break;
        }
        // the same memory is used for tx and rx blocks (tx_block is just used to initiate receive)
        tx_block = (lan865x_tx_block_t *)rx_buffer_p;
        rx_block = (lan865x_rx_block_t *)rx_buffer_p;

        // prepare empty tx block header to initiate receive data
        memset(&tx_block->header.val, 0, sizeof(lan865x_tx_header_t));
        tx_block->header.dnc = 1; // Data, not control
        tx_block->header.parity = lan865x_parity(tx_block->header.val);
        tx_block->header.val = be32toh(tx_block->header.val);

        // initiate SPI transaction
        ESP_GOTO_ON_ERROR(emac->spi.read(emac->spi.ctx, 0, 0, tx_block, sizeof(lan865x_tx_block_t)), err_inv_data, TAG, "spi failed");

        rx_block->footer.val = be32toh(rx_block->footer.val);
        // compute footer parity - should have odd number of 1s
        bool footer_parity = lan865x_parity(rx_block->footer.val);
        // Invalid footer parity indicates potential data corruption at SPI from LAN865X
        ESP_GOTO_ON_FALSE(footer_parity == rx_block->footer.parity, ESP_ERR_INVALID_CRC, err_inv_data, TAG, "footer parity mismatch");
        // Header bad indicates potential data corruption at SPI to LAN865X
        ESP_GOTO_ON_FALSE(rx_block->footer.hdrb == 0, ESP_ERR_INVALID_CRC, err, TAG, "header bad");
        blocks_available = rx_block->footer.rba;

        // check if data is valid
        if (rx_block->footer.dv == 1) {
            // in case previous block was partially received, find a new start
            if (!start_found) {
                if (rx_block->footer.sv == 1) {
                    // Data should be always aligned to zero due to LAN865X_OA_CONFIG0_RECV_FRAME_ALIGN_ZERO
                    ESP_GOTO_ON_FALSE(rx_block->footer.swo == 0, ESP_ERR_INVALID_STATE, err, TAG, "partial block received");
                    start_found = true;
                } else {
                    continue;
                }
            }

            uint8_t copy_len;
            if (rx_block->footer.ev) {
                copy_len = rx_block->footer.ebo + 1; // +1 because it's offset, not length
            } else {
                copy_len = LAN865X_DATA_BLOCK_SIZE;
            }
            actual_length += copy_len;
            // move pointer in rx_buffer to next block
            rx_buffer_p += LAN865X_DATA_BLOCK_SIZE;
        }
    } while (rx_block->footer.dv == 1 && !rx_block->footer.ev);

err:
    if (remain != NULL) {
        *remain = blocks_available;
    }
err_inv_data:
    lan865x_spi_unlock(emac);
    *length = actual_length;
    return ret;
}

static esp_err_t lan865x_control_transaction(emac_lan865x_t *emac, bool write, uint8_t mms, uint16_t addr, uint8_t len, uint32_t *data)
{
    esp_err_t ret = ESP_OK;
    ESP_LOGD(TAG, "ctrl_translen: %" PRIu8 ", addr: 0x%04" PRIx16 ", mms: %" PRIu8 ", write: %d", len, addr, mms, write);
    ESP_RETURN_ON_FALSE(len * 4 <= LAN865X_SPI_MAX_CTRL_BLOCK_SIZE, ESP_ERR_INVALID_ARG, TAG, "invalid length");
    if (!lan865x_spi_lock(emac)) {
        ESP_LOGE(TAG, "%s: timeout", __func__);
        return ESP_ERR_TIMEOUT;
    }
    uint32_t trans_len = LAN865X_DUMMY_OFFSET + LAN865X_HEADER_FOOTER_SIZE + len * 4;

    // Prepare control header
    lan865x_control_header_t ctrl_hdr = {
        .len = len - 1,  // Number of registers - 1
        .addr = addr,    // Register address
        .mms = mms,      // Memory map selector
        .rw = write,     // Write or read
        .dnc = 0,        // Control transaction
        .aid = 1,        // Address increment disable
    };

    // Calculate header parity - odd number of 1s
    ctrl_hdr.parity = lan865x_parity(ctrl_hdr.val);

    lan865x_control_block_t *control_block = (lan865x_control_block_t *)emac->spi_buffer;
    control_block->header.val = htobe32(ctrl_hdr.val);
    if (write) {
        for (int i = 0; i < len; i++) {
            control_block->data[i] = htobe32(*data);
            data++;
        }
    }
    ESP_GOTO_ON_ERROR(emac->spi.read(emac->spi.ctx, 0, 0, emac->spi_buffer, trans_len), err, TAG, "spi failed");
    lan865x_control_resp_t *control_resp = (lan865x_control_resp_t *)emac->spi_buffer;
    control_resp->header.val = be32toh(control_resp->header.val);
    bool footer_parity = lan865x_parity(control_resp->header.val);
    // Invalid footer parity indicates potential data corruption at SPI from LAN865X
    ESP_GOTO_ON_FALSE(footer_parity == control_resp->header.parity, ESP_ERR_INVALID_CRC, err, TAG, "footer parity mismatch");
    // Header bad indicates potential data corruption at SPI to LAN865X
    ESP_GOTO_ON_FALSE(control_resp->header.hdrb == 0, ESP_ERR_INVALID_CRC, err, TAG, "control header bad");
    if (!write) {
        for (int i = 0; i < len; i++) {
            *data = be32toh(control_resp->data[i]);
            data++;
        }
    }

err:
    lan865x_spi_unlock(emac);
    return ret;
}

static esp_err_t lan865x_read_reg(emac_lan865x_t *emac, uint8_t mms, uint16_t addr, uint32_t *value)
{
    return lan865x_control_transaction(emac, LAN865X_READ_REG, mms, addr, 1, value);
}

static esp_err_t lan865x_write_reg(emac_lan865x_t *emac, uint8_t mms, uint16_t addr, uint32_t value)
{
    return lan865x_control_transaction(emac, LAN865X_WRITE_REG, mms, addr, 1, &value);
}

static esp_err_t lan865x_set_reg_bits(emac_lan865x_t *emac, uint8_t mms, uint16_t addr, uint32_t bitmask)
{
    esp_err_t ret = ESP_OK;
    uint32_t reg_value;

    ESP_RETURN_ON_ERROR(lan865x_read_reg(emac, mms, addr, &reg_value), TAG, "Failed to read register MMS: %" PRIu8 ", ADDR: 0x%04" PRIx16, mms, addr);
    reg_value |= bitmask;
    ESP_RETURN_ON_ERROR(lan865x_write_reg(emac, mms, addr, reg_value), TAG, "Failed to write register MMS: %" PRIu8 ", ADDR: 0x%04" PRIx16, mms, addr);
    return ret;
}

static esp_err_t lan865x_clear_reg_bits(emac_lan865x_t *emac, uint8_t mms, uint16_t addr, uint32_t bitmask)
{
    esp_err_t ret = ESP_OK;
    uint32_t reg_value;
    ESP_RETURN_ON_ERROR(lan865x_read_reg(emac, mms, addr, &reg_value), TAG, "Failed to read register MMS: %" PRIu8 ", ADDR: 0x%04" PRIx16, mms, addr);
    reg_value &= ~bitmask;
    ESP_RETURN_ON_ERROR(lan865x_write_reg(emac, mms, addr, reg_value), TAG, "Failed to write register MMS: %" PRIu8 ", ADDR: 0x%04" PRIx16, mms, addr);
    return ret;
}

// !! Proprietary access mechanism. Do not confuse this with the Clause 22 indirect access to Clause 45 registers. !!
static esp_err_t lan865x_indirect_read(emac_lan865x_t *emac, uint8_t addr, uint8_t mask, uint8_t *value)
{
    esp_err_t ret = ESP_OK;
    uint32_t reg_value;
    ESP_GOTO_ON_ERROR(lan865x_write_reg(emac, 0x04, 0x00D8, addr), err, TAG, "Failed to write register MMS: 0x04, ADDR: 0x00D8");
    ESP_GOTO_ON_ERROR(lan865x_write_reg(emac, 0x04, 0x00DA, 0x2), err, TAG, "Failed to write register MMS: 0x04, ADDR: 0x00DA");
    ESP_GOTO_ON_ERROR(lan865x_read_reg(emac, 0x04, 0x00D9, &reg_value), err, TAG, "Failed to read register MMS: 0x04, ADDR: 0x00D9");
    *value = reg_value & mask;
err:
    return ret;
}

// Configuration recommended by manufacturer (http://www.microchip.com/DS60001760)
static esp_err_t lan865x_default_config(emac_lan865x_t *emac)
{
    esp_err_t ret = ESP_OK;

    // Read configuration parameters
    uint8_t value1, value2;
    int8_t offset1, offset2;
    uint16_t cfgparam1, cfgparam2;

    // Read value1 from address 0x04
    ESP_GOTO_ON_ERROR(lan865x_indirect_read(emac, 0x04, 0x1F, &value1), err, TAG, "Failed to read value1");

    // Calculate offset1
    if ((value1 & 0x10) != 0) {
        offset1 = (int8_t)((uint8_t)value1 - 0x20);
    } else {
        offset1 = (int8_t)value1;
    }

    // Read value2 from address 0x0008
    ESP_GOTO_ON_ERROR(lan865x_indirect_read(emac, 0x08, 0x1F, &value2), err, TAG, "Failed to read value2");

    // Calculate offset2
    if ((value2 & 0x10) != 0) {
        offset2 = (int8_t)((uint8_t)value2 - 0x20);
    } else {
        offset2 = (int8_t)value2;
    }

    // Calculate configuration parameters
    cfgparam1 = (uint16_t)(((9 + offset1) & 0x3F) << 10) | (uint16_t)(((14 + offset1) & 0x3F) << 4) | 0x03;
    cfgparam2 = (uint16_t)(((40 + offset2) & 0x3F) << 10);

    // Write configuration registers according to Table 1 in AN1760
    ESP_GOTO_ON_ERROR(lan865x_write_reg(emac, 0x4, 0x00D0, 0x3F31), err, TAG, "Failed to write 0x00D0");
    ESP_GOTO_ON_ERROR(lan865x_write_reg(emac, 0x4, 0x00E0, 0xC000), err, TAG, "Failed to write 0x00E0");
    ESP_GOTO_ON_ERROR(lan865x_write_reg(emac, 0x4, 0x0084, cfgparam1), err, TAG, "Failed to write 0x0084");
    ESP_GOTO_ON_ERROR(lan865x_write_reg(emac, 0x4, 0x008A, cfgparam2), err, TAG, "Failed to write 0x008A");
    ESP_GOTO_ON_ERROR(lan865x_write_reg(emac, 0x4, 0x00E9, 0x9E50), err, TAG, "Failed to write 0x00E9");
    ESP_GOTO_ON_ERROR(lan865x_write_reg(emac, 0x4, 0x00F5, 0x1CF8), err, TAG, "Failed to write 0x00F5");
    ESP_GOTO_ON_ERROR(lan865x_write_reg(emac, 0x4, 0x00F4, 0xC020), err, TAG, "Failed to write 0x00F4");
    ESP_GOTO_ON_ERROR(lan865x_write_reg(emac, 0x4, 0x00F8, 0xB900), err, TAG, "Failed to write 0x00F8");
    ESP_GOTO_ON_ERROR(lan865x_write_reg(emac, 0x4, 0x00F9, 0x4E53), err, TAG, "Failed to write 0x00F9");
    ESP_GOTO_ON_ERROR(lan865x_write_reg(emac, 0x4, 0x0081, 0x0080), err, TAG, "Failed to write 0x0081");
    ESP_GOTO_ON_ERROR(lan865x_write_reg(emac, 0x4, 0x0091, 0x9660), err, TAG, "Failed to write 0x0091");
    ESP_GOTO_ON_ERROR(lan865x_write_reg(emac, 0x1, 0x0077, 0x0028), err, TAG, "Failed to write 0x0077");
    ESP_GOTO_ON_ERROR(lan865x_write_reg(emac, 0x4, 0x0043, 0x00FF), err, TAG, "Failed to write 0x0043");
    ESP_GOTO_ON_ERROR(lan865x_write_reg(emac, 0x4, 0x0044, 0xFFFF), err, TAG, "Failed to write 0x0044");
    ESP_GOTO_ON_ERROR(lan865x_write_reg(emac, 0x4, 0x0045, 0x0000), err, TAG, "Failed to write 0x0045");
    ESP_GOTO_ON_ERROR(lan865x_write_reg(emac, 0x4, 0x0053, 0x00FF), err, TAG, "Failed to write 0x0053");
    ESP_GOTO_ON_ERROR(lan865x_write_reg(emac, 0x4, 0x0054, 0xFFFF), err, TAG, "Failed to write 0x0054");
    ESP_GOTO_ON_ERROR(lan865x_write_reg(emac, 0x4, 0x0055, 0x0000), err, TAG, "Failed to write 0x0055");
    ESP_GOTO_ON_ERROR(lan865x_write_reg(emac, 0x4, 0x0040, 0x0002), err, TAG, "Failed to write 0x0040");
    ESP_GOTO_ON_ERROR(lan865x_write_reg(emac, 0x4, 0x0050, 0x0002), err, TAG, "Failed to write 0x0050");
err:
    return ret;
}

static esp_err_t lan865x_reset(emac_lan865x_t *emac)
{
    esp_err_t ret = ESP_OK;

    lan865x_oa_reset_reg_t oa_reset = {
        .swreset = 1,
    };
    ESP_GOTO_ON_ERROR(lan865x_write_reg(emac, LAN865X_MMS_OA, LAN865X_OA_RESET_REG_ADDR, oa_reset.val), err, TAG, "OA_RESET configuration failed");
    // wait for reset complete
    uint32_t to = 0;
    for (; to < LAN865X_SW_RESET_TIMEOUT_MS; to += 10) {
        ESP_GOTO_ON_ERROR(lan865x_read_reg(emac, LAN865X_MMS_OA, LAN865X_OA_RESET_REG_ADDR, &oa_reset.val), err, TAG, "OA_RESET read failed");
        if (oa_reset.swreset == 0) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    // wait for reset complete is indicated in the status register
    lan865x_oa_status0_reg_t oa_status0;
    for (; to < LAN865X_SW_RESET_TIMEOUT_MS; to += 10) {
        ESP_GOTO_ON_ERROR(lan865x_read_reg(emac, LAN865X_MMS_OA, LAN865X_OA_STATUS0_REG_ADDR, &oa_status0.val), err, TAG, "OA_STATUS0 read failed");
        if (oa_status0.resetc == 1) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    ESP_GOTO_ON_FALSE(to < LAN865X_SW_RESET_TIMEOUT_MS, ESP_ERR_TIMEOUT, err, TAG, "reset timeout");
    // clear reset complete by writing 1 to resetc bit
    oa_status0.resetc = 1;
    ESP_GOTO_ON_ERROR(lan865x_write_reg(emac, LAN865X_MMS_OA, LAN865X_OA_STATUS0_REG_ADDR, oa_status0.val), err, TAG, "OA_STATUS0 configuration failed");

err:
    return ret;
}

static esp_err_t lan865x_verify_id(emac_lan865x_t *emac)
{
    esp_err_t ret = ESP_OK;

    lan865x_devid_reg_t devid;
    ESP_GOTO_ON_ERROR(lan865x_read_reg(emac, LAN865X_MMS_MISC, LAN865X_DEVID_REG_ADDR, &devid.val), err, TAG, "Failed to read devid");
    ESP_GOTO_ON_FALSE((devid.model == 0x8650 || devid.model == 0x8651), ESP_ERR_INVALID_VERSION, err, TAG, "Invalid chip ID: 0x%04x", devid.model);
    ESP_LOGI(TAG, "Chip ID verified: LAN%04x", devid.model);
err:
    return ret;
}

static esp_err_t emac_lan865x_start(esp_eth_mac_t *mac)
{
    esp_err_t ret = ESP_OK;
    emac_lan865x_t *emac = __containerof(mac, emac_lan865x_t, parent);

    // Configure MAC Network Control Register mask to enable Rx/Tx
    lan865x_mac_ncr_reg_t mac_ncr_mask = {
        .rxen = 1,         // Enable receive
        .txen = 1,         // Enable transmit
    };
    ESP_GOTO_ON_ERROR(lan865x_set_reg_bits(emac, LAN865X_MMS_MAC, LAN865X_MAC_NCR_REG_ADDR, mac_ncr_mask.val), err, TAG, "MAC_NCR configuration failed");
err:
    return ret;
}

static esp_err_t emac_lan865x_stop(esp_eth_mac_t *mac)
{
    esp_err_t ret = ESP_OK;
    emac_lan865x_t *emac = __containerof(mac, emac_lan865x_t, parent);

    // Configure MAC Network Control Register mask to disable Rx/Tx
    lan865x_mac_ncr_reg_t mac_ncr_mask = {
        .rxen = 1,         // Disable receive
        .txen = 1,         // Disable transmit
    };
    ESP_GOTO_ON_ERROR(lan865x_clear_reg_bits(emac, LAN865X_MMS_MAC, LAN865X_MAC_NCR_REG_ADDR, mac_ncr_mask.val), err, TAG, "MAC_NCR configuration failed");
err:
    return ret;
}

static esp_err_t emac_lan865x_set_mediator(esp_eth_mac_t *mac, esp_eth_mediator_t *eth)
{
    esp_err_t ret = ESP_OK;
    ESP_GOTO_ON_FALSE(eth, ESP_ERR_INVALID_ARG, err, TAG, "can't set mac's mediator to null");
    emac_lan865x_t *emac = __containerof(mac, emac_lan865x_t, parent);
    emac->eth = eth;
    return ESP_OK;
err:
    return ret;
}

static esp_err_t emac_lan865x_write_phy_reg(esp_eth_mac_t *mac, uint32_t phy_addr, uint32_t phy_reg, uint32_t reg_value)
{
    esp_err_t ret = ESP_OK;
    emac_lan865x_t *emac = __containerof(mac, emac_lan865x_t, parent);
    ret = lan865x_write_reg(emac, LAN865X_MMS_OA, phy_reg | LAN865X_OA_PHY_REG_OFFSET, reg_value);
    return ret;
}

static esp_err_t emac_lan865x_read_phy_reg(esp_eth_mac_t *mac, uint32_t phy_addr, uint32_t phy_reg, uint32_t *reg_value)
{
    esp_err_t ret = ESP_OK;
    emac_lan865x_t *emac = __containerof(mac, emac_lan865x_t, parent);
    ret = lan865x_read_reg(emac, LAN865X_MMS_OA, phy_reg | LAN865X_OA_PHY_REG_OFFSET, reg_value);
    return ret;
}

static esp_err_t emac_lan865x_set_addr(esp_eth_mac_t *mac, uint8_t *addr)
{
    esp_err_t ret = ESP_OK;
    emac_lan865x_t *emac = __containerof(mac, emac_lan865x_t, parent);

    lan865x_mac_sab1_reg_t mac_sab1 = {
        .val = addr[0] | addr[1] << 8 | addr[2] << 16 | addr[3] << 24,
    };
    ESP_GOTO_ON_ERROR(lan865x_write_reg(emac, LAN865X_MMS_MAC, LAN865X_MAC_SAB1_REG_ADDR, mac_sab1.val), err, TAG, "MAC_SAB1 configuration failed");
    lan865x_mac_sat1_reg_t mac_sat1 = {
        .val = addr[4] | addr[5] << 8,
    };
    ESP_GOTO_ON_ERROR(lan865x_write_reg(emac, LAN865X_MMS_MAC, LAN865X_MAC_SAT1_REG_ADDR, mac_sat1.val), err, TAG, "MAC_SAT1 configuration failed");
err:
    return ret;
}

static esp_err_t emac_lan865x_get_addr(esp_eth_mac_t *mac, uint8_t *addr)
{
    esp_err_t ret = ESP_OK;
    emac_lan865x_t *emac = __containerof(mac, emac_lan865x_t, parent);

    lan865x_mac_sab1_reg_t mac_sab1;
    ESP_GOTO_ON_ERROR(lan865x_read_reg(emac, LAN865X_MMS_MAC, LAN865X_MAC_SAB1_REG_ADDR, &mac_sab1.val), err, TAG, "MAC_SAB1 read failed");
    addr[0] = mac_sab1.val & 0xFF;
    addr[1] = (mac_sab1.val >> 8) & 0xFF;
    addr[2] = (mac_sab1.val >> 16) & 0xFF;
    addr[3] = (mac_sab1.val >> 24) & 0xFF;
    lan865x_mac_sat1_reg_t mac_sat1;
    ESP_GOTO_ON_ERROR(lan865x_read_reg(emac, LAN865X_MMS_MAC, LAN865X_MAC_SAT1_REG_ADDR, &mac_sat1.val), err, TAG, "MAC_SAT1 read failed");
    addr[4] = mac_sat1.val & 0xFF;
    addr[5] = (mac_sat1.val >> 8) & 0xFF;
err:
    return ret;
}

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
static esp_err_t lan865x_hash_filter_modify(emac_lan865x_t *emac, uint8_t *addr, bool add)
{
    esp_err_t ret = ESP_OK;

    uint32_t k = 0;
    //Apply the hash function
    k = (addr[0] >> 6) ^ addr[0];
    k ^= (addr[1] >> 4) ^ (addr[1] << 2);
    k ^= (addr[2] >> 2) ^ (addr[2] << 4);
    k ^= (addr[3] >> 6) ^ addr[3];
    k ^= (addr[4] >> 4) ^ (addr[4] << 2);
    k ^= (addr[5] >> 2) ^ (addr[5] << 4);

    uint8_t hash_value = k & 0x3F;
    uint8_t hash_group = hash_value / 32;
    uint8_t hash_bit = hash_value % 32;

    lan865x_mac_hrb_reg_t mac_hrb;
    lan865x_mac_hrt_reg_t mac_hrt;
    ESP_GOTO_ON_ERROR(lan865x_read_reg(emac, LAN865X_MMS_MAC, LAN865X_MAC_HRB_REG_ADDR, &mac_hrb.val), err, TAG, "read MAC_HRB register failed");
    ESP_GOTO_ON_ERROR(lan865x_read_reg(emac, LAN865X_MMS_MAC, LAN865X_MAC_HRT_REG_ADDR, &mac_hrt.val), err, TAG, "read MAC_HRT register failed");

    uint32_t *hash_addr;
    if (hash_group == 0) {
        hash_addr = &mac_hrb.val;
    } else {
        hash_addr = &mac_hrt.val;
    }

    if (add) {
        // add address to hash table
        *hash_addr |= 1 << hash_bit;

        emac->hash_filter_cnt[hash_value]++;
    } else {
        emac->hash_filter_cnt[hash_value]--;
        if (emac->hash_filter_cnt[hash_value] == 0) {
            // remove address from hash table
            *hash_addr &= ~(1 << hash_bit);
        }
    }
    // Write order matters
    ESP_GOTO_ON_ERROR(lan865x_write_reg(emac, LAN865X_MMS_MAC, LAN865X_MAC_HRB_REG_ADDR, mac_hrb.val), err, TAG, "write MAC_HRB register failed");
    ESP_GOTO_ON_ERROR(lan865x_write_reg(emac, LAN865X_MMS_MAC, LAN865X_MAC_HRT_REG_ADDR, mac_hrt.val), err, TAG, "write MAC_HRT register failed");

err:
    return ret;
}

static esp_err_t emac_lan865x_add_mac_filter(esp_eth_mac_t *mac, uint8_t *addr)
{
    esp_err_t ret = ESP_OK;
    emac_lan865x_t *emac = __containerof(mac, emac_lan865x_t, parent);
    ESP_RETURN_ON_ERROR(lan865x_hash_filter_modify(emac, addr, true), TAG, "modify multicast table failed");
    return ret;
}

static esp_err_t emac_lan865x_rm_mac_filter(esp_eth_mac_t *mac, uint8_t *addr)
{
    esp_err_t ret = ESP_OK;
    emac_lan865x_t *emac = __containerof(mac, emac_lan865x_t, parent);
    ESP_RETURN_ON_ERROR(lan865x_hash_filter_modify(emac, addr, false), TAG, "modify multicast table failed");
    return ret;
}
#endif // ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)

static esp_err_t emac_lan865x_set_link(esp_eth_mac_t *mac, eth_link_t link)
{
    esp_err_t ret = ESP_OK;
    emac_lan865x_t *emac = __containerof(mac, emac_lan865x_t, parent);
    switch (link) {
    case ETH_LINK_UP:
        ESP_GOTO_ON_ERROR(mac->start(mac), err, TAG, "lan865x start failed");
        if (emac->poll_timer) {
            ESP_GOTO_ON_ERROR(esp_timer_start_periodic(emac->poll_timer, emac->poll_period_ms * 1000),
                              err, TAG, "start poll timer failed");
        }
        break;
    case ETH_LINK_DOWN:
        ESP_GOTO_ON_ERROR(mac->stop(mac), err, TAG, "lan865x stop failed");
        if (emac->poll_timer) {
            ESP_GOTO_ON_ERROR(esp_timer_stop(emac->poll_timer),
                              err, TAG, "stop poll timer failed");
        }
        break;
    default:
        ESP_GOTO_ON_FALSE(false, ESP_ERR_INVALID_ARG, err, TAG, "unknown link status");
        break;
    }
    return ESP_OK;
err:
    return ret;
}

static esp_err_t emac_lan865x_set_speed(esp_eth_mac_t *mac, eth_speed_t speed)
{
    if (speed != ETH_SPEED_10M) {
        ESP_LOGW(TAG, "Speed setting other than 10Mbps is not supported");
        return ESP_ERR_NOT_SUPPORTED;
    }
    return ESP_OK;
}

static esp_err_t emac_lan865x_set_duplex(esp_eth_mac_t *mac, eth_duplex_t duplex)
{
    if (duplex != ETH_DUPLEX_HALF) {
        ESP_LOGW(TAG, "Full-Duplex setting is not supported");
        return ESP_ERR_NOT_SUPPORTED;
    }
    return ESP_OK;
}

static esp_err_t emac_lan865x_set_promiscuous(esp_eth_mac_t *mac, bool enable)
{
    esp_err_t ret = ESP_OK;
    emac_lan865x_t *emac = __containerof(mac, emac_lan865x_t, parent);

    lan865x_mac_ncfgr_reg_t mac_ncfgr_mask = {
        .calf = 1,
    };
    if (enable) {
        ESP_GOTO_ON_ERROR(lan865x_set_reg_bits(emac, LAN865X_MMS_MAC, LAN865X_MAC_NCFGR_REG_ADDR, mac_ncfgr_mask.val), err, TAG, "MAC_NCFGR configuration failed");
    } else {
        ESP_GOTO_ON_ERROR(lan865x_clear_reg_bits(emac, LAN865X_MMS_MAC, LAN865X_MAC_NCFGR_REG_ADDR, mac_ncfgr_mask.val), err, TAG, "MAC_NCFGR configuration failed");
    }
err:
    return ret;
}

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
static esp_err_t emac_lan865x_set_all_multicast(esp_eth_mac_t *mac, bool enable)
{
    esp_err_t ret = ESP_OK;
    emac_lan865x_t *emac = __containerof(mac, emac_lan865x_t, parent);
    lan865x_mac_hrb_reg_t mac_hrb;
    lan865x_mac_hrt_reg_t mac_hrt;

    if (enable) {
        mac_hrb.val = 0xFFFFFFFF;
        mac_hrt.val = 0xFFFFFFFF;
    } else {
        mac_hrb.val = 0x0;
        mac_hrt.val = 0x0;
    }
    ESP_GOTO_ON_ERROR(lan865x_write_reg(emac, LAN865X_MMS_MAC, LAN865X_MAC_HRB_REG_ADDR, mac_hrb.val), err, TAG, "write MAC_HRB register failed");
    ESP_GOTO_ON_ERROR(lan865x_write_reg(emac, LAN865X_MMS_MAC, LAN865X_MAC_HRT_REG_ADDR, mac_hrt.val), err, TAG, "write MAC_HRT register failed");
err:
    return ret;
}
#endif // ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)

static esp_err_t emac_lan865x_enable_flow_ctrl(esp_eth_mac_t *mac, bool enable)
{
    ESP_LOGW(TAG, "Flow control setting is not supported");
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t emac_lan865x_set_peer_pause_ability(esp_eth_mac_t *mac, uint32_t ability)
{
    ESP_LOGW(TAG, "Peer pause ability setting is not supported");
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t emac_lan865x_transmit(esp_eth_mac_t *mac, uint8_t *buf, uint32_t length)
{
    esp_err_t ret = ESP_OK;
    emac_lan865x_t *emac = __containerof(mac, emac_lan865x_t, parent);
    ESP_LOGD(TAG, "Transmitting %" PRIu32 " bytes", length);

    lan865x_oa_bufsts_reg_t oa_bufsts;
    ESP_GOTO_ON_ERROR(lan865x_read_reg(emac, LAN865X_MMS_OA, LAN865X_OA_BUFSTS_REG_ADDR, &oa_bufsts.val), err,  TAG, "OA_BUFSTS read failed");
    if (oa_bufsts.txc < (length + LAN865X_DATA_BLOCK_SIZE - 1) / LAN865X_DATA_BLOCK_SIZE) {
        ESP_LOGD(TAG, "Not enough transmit credits available");
        return ESP_ERR_NO_MEM;
    }
    ESP_GOTO_ON_FALSE(lan865x_frame_transmit(emac, buf, length) == ESP_OK, ESP_FAIL, err, TAG, "frame transmit failed at SPI");
err:
    return ret;
}

static esp_err_t emac_lan865x_receive(esp_eth_mac_t *mac, uint8_t *buf, uint32_t *length)
{
    esp_err_t ret = ESP_OK;
    emac_lan865x_t *emac = __containerof(mac, emac_lan865x_t, parent);
    lan865x_oa_bufsts_reg_t oa_bufsts;
    ESP_GOTO_ON_ERROR(lan865x_read_reg(emac, LAN865X_MMS_OA, LAN865X_OA_BUFSTS_REG_ADDR, &oa_bufsts.val), err,  TAG, "OA_BUFSTS read failed");
    if (oa_bufsts.rba < 1) {
        ESP_LOGD(TAG, "No receive blocks available");
        return ESP_ERR_NO_MEM;
    }
    uint32_t frame_len = ETH_MAX_PACKET_SIZE;
    ESP_GOTO_ON_ERROR(lan865x_frame_receive(emac, emac->rx_buffer, &frame_len, NULL), err, TAG, "frame receive failed at SPI");
    if (frame_len > 0) {
        uint32_t copy_len = frame_len;
        if (frame_len > *length) {
            ret = ESP_ERR_INVALID_SIZE;
            copy_len = *length;
        }
        memcpy(buf, emac->rx_buffer, copy_len);
        *length = frame_len;
    }
err:
    return ret;
}

IRAM_ATTR static void lan865x_isr_handler(void *arg)
{
    emac_lan865x_t *emac = (emac_lan865x_t *)arg;
    BaseType_t high_task_wakeup = pdFALSE;
    /* notify lan865x task */
    vTaskNotifyGiveFromISR(emac->rx_task_hdl, &high_task_wakeup);
    if (high_task_wakeup != pdFALSE) {
        portYIELD_FROM_ISR();
    }
}

static void lan865x_poll_timer(void *arg)
{
    emac_lan865x_t *emac = (emac_lan865x_t *)arg;
    xTaskNotifyGive(emac->rx_task_hdl);
}

static void emac_lan865x_task(void *arg)
{
    emac_lan865x_t *emac = (emac_lan865x_t *)arg;

    while (1) {
        // check if the task receives any notification
        if (emac->int_gpio_num >= 0) {                                   // if in interrupt mode
            if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000)) == 0 &&    // if no notification ...
                    gpio_get_level(emac->int_gpio_num) == 1) {           // ...and no interrupt asserted
                continue;                                                // -> just continue to check again
            }
        } else {
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        }

        uint8_t remain = 0;
        do {
            uint32_t frame_len = ETH_MAX_PACKET_SIZE;
            if (lan865x_frame_receive(emac, emac->rx_buffer, &frame_len, &remain) == ESP_OK) {
                if (frame_len > 0) {
                    uint8_t *buffer = malloc(frame_len);
                    if (buffer == NULL) {
                        ESP_LOGE(TAG, "no mem for receive buffer");
                    } else {
                        memcpy(buffer, emac->rx_buffer, frame_len);
                        ESP_LOGD(TAG, "receive len=%" PRIu32, frame_len);
                        /* pass the buffer to stack (e.g. TCP/IP layer) */
                        emac->eth->stack_input(emac->eth, buffer, frame_len);
                    }
                }
            } else {
                ESP_LOGE(TAG, "frame receive failed");
            }
        } while (remain > 0);
    }

    vTaskDelete(NULL);
}

static esp_err_t emac_lan865x_init(esp_eth_mac_t *mac)
{
    esp_err_t ret = ESP_OK;
    emac_lan865x_t *emac = __containerof(mac, emac_lan865x_t, parent);
    esp_eth_mediator_t *eth = emac->eth;

    ESP_GOTO_ON_ERROR(eth->on_state_changed(eth, ETH_STATE_LLINIT, NULL), err, TAG, "lowlevel init failed");

    // Reset the device
    ESP_GOTO_ON_ERROR(lan865x_reset(emac), err, TAG, "reset failed");

    // Verify device ID
    ESP_GOTO_ON_ERROR(lan865x_verify_id(emac), err, TAG, "device ID verification failed");

    // Set device default configuration as recommended by manufacturer
    ESP_GOTO_ON_ERROR(lan865x_default_config(emac), err, TAG, "default configuration failed");

    // Configure MAC registers
    lan865x_mac_ncfgr_reg_t mac_ncfgr_mask = {
        .mtihen = 1,       // Enable multicast hash table
        .rfcs = 1,         // Remove rx frame FCS
    };
    ESP_GOTO_ON_ERROR(lan865x_set_reg_bits(emac, LAN865X_MMS_MAC, LAN865X_MAC_NCFGR_REG_ADDR, mac_ncfgr_mask.val), err, TAG, "MAC_NCFGR configuration failed");

    // Configure MAC Network Control Register (but don't enable Rx/Tx yet)
    lan865x_mac_ncr_reg_t mac_ncr_mask = {
        .lbl = 0,          // Disable loopback
        .rxen = 0,         // Disable receive (will be enabled in start())
        .txen = 0,         // Disable transmit (will be enabled in start())
    };
    ESP_GOTO_ON_ERROR(lan865x_set_reg_bits(emac, LAN865X_MMS_MAC, LAN865X_MAC_NCR_REG_ADDR, mac_ncr_mask.val), err, TAG, "MAC_NCR configuration failed");

    // Configure OA_CONFIG0
    lan865x_oa_config0_reg_t oa_config0 = {
        .bps = LAN865X_OA_CONFIG0_BLOCK_PAYLOAD_SIZE_64,
        .rfa = LAN865X_OA_CONFIG0_RECV_FRAME_ALIGN_ZERO,
        .prote = 0,        // Disable control data protection
        .ftse = 0,         // Disable frame timestamp
        .rxcte = 0,        // Disable receive cut-through
        .txcte = 0,        // Disable transmit cut-through
        .txfcsve = 0,      // Disable transmit FCS validation
        .sync = 1,         // Confirm configuration synchronization
    };
    ESP_GOTO_ON_ERROR(lan865x_write_reg(emac, LAN865X_MMS_OA, LAN865X_OA_CONFIG0_REG_ADDR, oa_config0.val), err, TAG, "OA_CONFIG0 configuration failed");

    // Clean reset status
    lan865x_oa_status0_reg_t oa_status0_mask = {
        .resetc = 1,
    };
    ESP_GOTO_ON_ERROR(lan865x_clear_reg_bits(emac, LAN865X_MMS_OA, LAN865X_OA_STATUS0_REG_ADDR, oa_status0_mask.val), err, TAG, "OA_STATUS0 configuration failed");

    if (emac->int_gpio_num >= 0) {
        gpio_func_sel(emac->int_gpio_num, PIN_FUNC_GPIO);
        gpio_input_enable(emac->int_gpio_num);
        gpio_pulldown_en(emac->int_gpio_num);
        gpio_set_intr_type(emac->int_gpio_num, GPIO_INTR_NEGEDGE);
        gpio_intr_enable(emac->int_gpio_num);
        gpio_isr_handler_add(emac->int_gpio_num, lan865x_isr_handler, emac);
    }

    return ESP_OK;
err:
    return ret;
}

static esp_err_t emac_lan865x_deinit(esp_eth_mac_t *mac)
{
    emac_lan865x_t *emac = __containerof(mac, emac_lan865x_t, parent);

    esp_eth_mediator_t *eth = emac->eth;
    mac->stop(mac);
    if (emac->int_gpio_num >= 0) {
        gpio_isr_handler_remove(emac->int_gpio_num);
    }
    if (emac->poll_timer && esp_timer_is_active(emac->poll_timer)) {
        esp_timer_stop(emac->poll_timer);
    }
    eth->on_state_changed(eth, ETH_STATE_DEINIT, NULL);
    return ESP_OK;
}

static esp_err_t emac_lan865x_del(esp_eth_mac_t *mac)
{
    emac_lan865x_t *emac = __containerof(mac, emac_lan865x_t, parent);
    if (emac->poll_timer) {
        esp_timer_delete(emac->poll_timer);
    }
    vTaskDelete(emac->rx_task_hdl);
    emac->spi.deinit(emac->spi.ctx);
    vSemaphoreDelete(emac->spi_lock);
    heap_caps_free(emac->rx_buffer);
    heap_caps_free(emac->spi_buffer);
    free(emac);
    return ESP_OK;
}

esp_eth_mac_t *esp_eth_mac_new_lan865x(const eth_lan865x_config_t *lan865x_config, const eth_mac_config_t *mac_config)
{
    esp_eth_mac_t *ret = NULL;
    emac_lan865x_t *emac = NULL;
    ESP_GOTO_ON_FALSE(lan865x_config, NULL, err, TAG, "can't set lan865x specific config to null");
    ESP_GOTO_ON_FALSE(mac_config, NULL, err, TAG, "can't set mac config to null");
    ESP_GOTO_ON_FALSE((lan865x_config->int_gpio_num >= 0) != (lan865x_config->poll_period_ms > 0), NULL, err, TAG, "invalid configuration argument combination");
    emac = calloc(1, sizeof(emac_lan865x_t));
    ESP_GOTO_ON_FALSE(emac, NULL, err, TAG, "calloc emac failed");
    /* bind methods and attributes */
    emac->sw_reset_timeout_ms = mac_config->sw_reset_timeout_ms;
    emac->int_gpio_num = lan865x_config->int_gpio_num;
    emac->poll_period_ms = lan865x_config->poll_period_ms;
    emac->parent.set_mediator = emac_lan865x_set_mediator;
    emac->parent.init = emac_lan865x_init;
    emac->parent.deinit = emac_lan865x_deinit;
    emac->parent.start = emac_lan865x_start;
    emac->parent.stop = emac_lan865x_stop;
    emac->parent.del = emac_lan865x_del;
    emac->parent.write_phy_reg = emac_lan865x_write_phy_reg;
    emac->parent.read_phy_reg = emac_lan865x_read_phy_reg;
    emac->parent.set_addr = emac_lan865x_set_addr;
    emac->parent.get_addr = emac_lan865x_get_addr;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
    emac->parent.add_mac_filter = emac_lan865x_add_mac_filter;
    emac->parent.rm_mac_filter = emac_lan865x_rm_mac_filter;
    emac->parent.set_all_multicast = emac_lan865x_set_all_multicast;
#endif
    emac->parent.set_speed = emac_lan865x_set_speed;
    emac->parent.set_duplex = emac_lan865x_set_duplex;
    emac->parent.set_link = emac_lan865x_set_link;
    emac->parent.set_promiscuous = emac_lan865x_set_promiscuous;
    emac->parent.set_peer_pause_ability = emac_lan865x_set_peer_pause_ability;
    emac->parent.enable_flow_ctrl = emac_lan865x_enable_flow_ctrl;
    emac->parent.transmit = emac_lan865x_transmit;
    emac->parent.receive = emac_lan865x_receive;

    if (lan865x_config->custom_spi_driver.init != NULL && lan865x_config->custom_spi_driver.deinit != NULL
            && lan865x_config->custom_spi_driver.read != NULL && lan865x_config->custom_spi_driver.write != NULL) {
        ESP_LOGD(TAG, "Using user's custom SPI Driver");
        emac->spi.init = lan865x_config->custom_spi_driver.init;
        emac->spi.deinit = lan865x_config->custom_spi_driver.deinit;
        emac->spi.read = lan865x_config->custom_spi_driver.read;
        emac->spi.write = lan865x_config->custom_spi_driver.write;
        /* Custom SPI driver device init */
        ESP_GOTO_ON_FALSE((emac->spi.ctx = emac->spi.init(lan865x_config->custom_spi_driver.config)) != NULL, NULL, err, TAG, "SPI initialization failed");
    } else {
        ESP_LOGD(TAG, "Using default SPI Driver");
        emac->spi.init = lan865x_spi_init;
        emac->spi.deinit = lan865x_spi_deinit;
        emac->spi.read = lan865x_spi_read;
        emac->spi.write = lan865x_spi_write;
        /* SPI device init */
        ESP_GOTO_ON_FALSE((emac->spi.ctx = emac->spi.init(lan865x_config)) != NULL, NULL, err, TAG, "SPI initialization failed");
    }

    // Create mutex
    emac->spi_lock = xSemaphoreCreateMutex();
    ESP_GOTO_ON_FALSE(emac->spi_lock, NULL, err, TAG, "create lock failed");

    /* create lan865x task */
    BaseType_t core_num = tskNO_AFFINITY;
    if (mac_config->flags & ETH_MAC_FLAG_PIN_TO_CORE) {
        core_num = esp_cpu_get_core_id();
    }
    BaseType_t xReturned = xTaskCreatePinnedToCore(emac_lan865x_task, "lan865x_tsk", mac_config->rx_task_stack_size, emac,
                                                   mac_config->rx_task_prio, &emac->rx_task_hdl, core_num);
    ESP_GOTO_ON_FALSE(xReturned == pdPASS, NULL, err, TAG, "create lan865x task failed");

    emac->rx_buffer = heap_caps_malloc(LAN865X_RX_BUFFER_SIZE, MALLOC_CAP_DMA);
    ESP_GOTO_ON_FALSE(emac->rx_buffer, NULL, err, TAG, "RX buffer allocation failed");

    emac->spi_buffer = heap_caps_malloc(LAN865X_SPI_BUFFER_SIZE, MALLOC_CAP_DMA);
    ESP_GOTO_ON_FALSE(emac->spi_buffer, NULL, err, TAG, "SPI buffer allocation failed");

    if (emac->int_gpio_num < 0) {
        const esp_timer_create_args_t poll_timer_args = {
            .callback = lan865x_poll_timer,
            .name = "emac_spi_poll_timer",
            .arg = emac,
            .skip_unhandled_events = true
        };
        ESP_GOTO_ON_FALSE(esp_timer_create(&poll_timer_args, &emac->poll_timer) == ESP_OK, NULL, err, TAG, "create poll timer failed");
    }

    return &(emac->parent);

err:
    if (emac) {
        if (emac->spi_lock) {
            vSemaphoreDelete(emac->spi_lock);
        }
        if (emac->poll_timer) {
            esp_timer_delete(emac->poll_timer);
        }
        if (emac->rx_task_hdl) {
            vTaskDelete(emac->rx_task_hdl);
        }
        if (emac->spi.ctx) {
            emac->spi.deinit(emac->spi.ctx);
        }
        heap_caps_free(emac->rx_buffer);
        heap_caps_free(emac->spi_buffer);
        free(emac);
    }
    return ret;
}
