/*
 * SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include "string.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "esp_log.h"
#include "esp_check.h"

#include "ksz8863_ctrl_internal.h"
#include "ksz8863.h" // registers

#define KSZ8863_I2C_TIMEOUT_MS 500
#define KSZ8863_I2C_LOCK_TIMEOUT_MS (KSZ8863_I2C_TIMEOUT_MS + 50)
#define KSZ8863_SPI_LOCK_TIMEOUT_MS 500

typedef struct {
    ksz8863_intf_mode_t mode;
    SemaphoreHandle_t bus_lock;
    esp_err_t (*ksz8863_reg_read)(uint8_t reg_addr, uint8_t *data, size_t len);
    esp_err_t (*ksz8863_reg_write)(uint8_t reg_addr, uint8_t *data, size_t len);
    union {
        spi_device_handle_t spi_handle;
        i2c_master_dev_handle_t i2c_handle;
    };
} ksz8863_ctrl_intf_t;

static ksz8863_ctrl_intf_t *s_ksz8863_ctrl_intf = NULL;

static const char *TAG = "ksz8863_ctrl_intf";

static inline bool bus_lock(uint32_t lock_timeout_ms)
{
    return xSemaphoreTake(s_ksz8863_ctrl_intf->bus_lock, pdMS_TO_TICKS(lock_timeout_ms)) == pdTRUE;
}

static inline bool bus_unlock(void)
{
    return xSemaphoreGive(s_ksz8863_ctrl_intf->bus_lock) == pdTRUE;
}

static esp_err_t ksz8863_i2c_write(uint8_t reg_addr, uint8_t *data, size_t len)
{
    esp_err_t ret = ESP_OK;
    uint8_t *reg_addr_and_data = malloc(len + 1);
    // Create a packet containing the register and data to be transmitted
    reg_addr_and_data[0] = reg_addr;
    memcpy(reg_addr_and_data + 1, data, len);
    // When performing a soft reset, the KSZ8863 doesn't produce an ACK.
    if unlikely(reg_addr == KSZ8863_RESET_ADDR) {
        i2c_operation_job_t i2c_tx_no_ack_check_ops[3] = {
            {.command = I2C_MASTER_CMD_START },
            {.command = I2C_MASTER_CMD_WRITE, .write = {.ack_check = false, .data = reg_addr_and_data, .total_bytes = len + 1}},
            {.command = I2C_MASTER_CMD_STOP }
        };
        i2c_master_execute_defined_operations(s_ksz8863_ctrl_intf->i2c_handle, i2c_tx_no_ack_check_ops, 3, KSZ8863_I2C_TIMEOUT_MS);
    } else {
        ESP_GOTO_ON_ERROR(i2c_master_transmit(s_ksz8863_ctrl_intf->i2c_handle, reg_addr_and_data, len + 1, KSZ8863_I2C_TIMEOUT_MS), err, TAG, "Error during i2c write operation");
    }
err:
    free(reg_addr_and_data);
    return ret;
}

static esp_err_t ksz8863_i2c_read(uint8_t reg_addr, uint8_t *data, size_t len)
{
    esp_err_t ret = ESP_OK;
    ESP_GOTO_ON_ERROR(i2c_master_transmit_receive(s_ksz8863_ctrl_intf->i2c_handle, &reg_addr, 1, data, len, KSZ8863_I2C_TIMEOUT_MS), err, TAG, "Error during i2c read operation");
err:
    return ret;
}

static esp_err_t ksz8863_spi_write(uint8_t reg_addr, uint8_t *data, size_t len)
{
    esp_err_t ret = ESP_OK;

    spi_transaction_t trans = {
        .cmd = KSZ8863_SPI_WRITE_CMD,
        .addr = reg_addr,
        .length = 8 * len,
        .tx_buffer = data
    };
    ESP_GOTO_ON_FALSE(bus_lock(KSZ8863_SPI_LOCK_TIMEOUT_MS), ESP_ERR_TIMEOUT, err, TAG, "SPI bus lock timeout");
    ESP_GOTO_ON_ERROR(spi_device_polling_transmit(s_ksz8863_ctrl_intf->spi_handle, &trans), err_release, TAG, "SPI transmit fail");
err_release:
    bus_unlock();
err:
    return ret;
}

static esp_err_t ksz8863_spi_read(uint8_t reg_addr, uint8_t *data, size_t len)
{
    esp_err_t ret = ESP_OK;

    spi_transaction_t trans = {
        .flags = len <= 4 ? SPI_TRANS_USE_RXDATA : 0, // use direct reads for registers to prevent overwrites by 4-byte boundary writes
        .cmd = KSZ8863_SPI_READ_CMD,
        .addr = reg_addr,
        .length = 8 * len,
        .rx_buffer = data
    };
    ESP_GOTO_ON_FALSE(bus_lock(KSZ8863_SPI_LOCK_TIMEOUT_MS), ESP_ERR_TIMEOUT, err, TAG, "SPI bus lock timeout");
    ESP_GOTO_ON_ERROR(spi_device_polling_transmit(s_ksz8863_ctrl_intf->spi_handle, &trans), err_release, TAG, "SPI transmit fail");
    bus_unlock();

    if ((trans.flags & SPI_TRANS_USE_RXDATA) && len <= 4) {
        memcpy(data, trans.rx_data, len);  // copy register values to output
    }
err:
    return ret;
err_release:
    bus_unlock();
    return ret;
}

esp_err_t ksz8863_phy_reg_write(esp_eth_handle_t eth_handle, uint32_t phy_addr, uint32_t phy_reg, uint32_t reg_value)
{
    return s_ksz8863_ctrl_intf->ksz8863_reg_write(phy_reg, (uint8_t *)&reg_value, 1);
}

esp_err_t ksz8863_phy_reg_read(esp_eth_handle_t eth_handle, uint32_t phy_addr, uint32_t phy_reg, uint32_t *reg_value)
{
    return s_ksz8863_ctrl_intf->ksz8863_reg_read(phy_reg, (uint8_t *)reg_value, 1);
}

static void ksz8863_swap_to_mac_tbl(uint8_t *swap_data, void *mac_table_out, bool static_tbl)
{
    size_t tbl_size;
    uint8_t *mac_tbl_data_p;
    uint8_t *mac_tbl_addr_p;

    if (static_tbl) {
        ksz8863_sta_mac_table_t *mac_table = (ksz8863_sta_mac_table_t *) mac_table_out;
        tbl_size = sizeof(ksz8863_sta_mac_table_t);
        mac_tbl_data_p = mac_table->data;
        mac_tbl_addr_p = mac_table->mac_addr;
    } else {
        ksz8863_dyn_mac_table_t *mac_table = (ksz8863_dyn_mac_table_t *) mac_table_out;
        tbl_size = sizeof(ksz8863_dyn_mac_table_t);
        mac_tbl_data_p = mac_table->data;
        mac_tbl_addr_p = mac_table->mac_addr;
    }
    // MAC address byte order does not need to be swapped, just store it at correct byte position
    memcpy(mac_tbl_addr_p, &swap_data[tbl_size - ETH_ADDR_LEN], ETH_ADDR_LEN);
    for (int i = ETH_ADDR_LEN; i < tbl_size; i++) {
        mac_tbl_data_p[i] = swap_data[tbl_size - 1 - i];
    }
}

static void ksz8863_swap_from_mac_tbl(void *mac_table_in, uint8_t *swap_data, bool static_tbl)
{
    size_t tbl_size;
    uint8_t *mac_tbl_data_p;
    uint8_t *mac_tbl_addr_p;

    if (static_tbl) {
        ksz8863_sta_mac_table_t *mac_table = (ksz8863_sta_mac_table_t *) mac_table_in;
        tbl_size = sizeof(ksz8863_sta_mac_table_t);
        mac_tbl_data_p = mac_table->data;
        mac_tbl_addr_p = mac_table->mac_addr;
    } else {
        ksz8863_dyn_mac_table_t *mac_table = (ksz8863_dyn_mac_table_t *) mac_table_in;
        tbl_size = sizeof(ksz8863_dyn_mac_table_t);
        mac_tbl_data_p = mac_table->data;
        mac_tbl_addr_p = mac_table->mac_addr;
    }
    // MAC address byte order does not need to be swapped, just store it at correct byte position
    memcpy(&swap_data[tbl_size - ETH_ADDR_LEN], mac_tbl_addr_p, ETH_ADDR_LEN);
    for (int i = ETH_ADDR_LEN; i < tbl_size; i++) {
        swap_data[tbl_size - 1 - i] = mac_tbl_data_p[i];
    }
}

esp_err_t ksz8863_indirect_read(ksz8863_indir_access_tbls_t tbl, uint8_t ind_addr, void *data, size_t len)
{
    if (!(s_ksz8863_ctrl_intf->mode == KSZ8863_I2C_MODE || s_ksz8863_ctrl_intf->mode == KSZ8863_SPI_MODE)) {
        ESP_LOGD(TAG, "Indirect read is accessible only in I2C or SPI mode");
        return ESP_ERR_INVALID_STATE;
    }

    if (len > KSZ8863_INDIR_DATA_MAX_SIZE) {
        ESP_LOGD(TAG, "maximally %d bytes can be indirectly accessed at a time", KSZ8863_INDIR_DATA_MAX_SIZE);
        return ESP_ERR_INVALID_SIZE;
    }

    ksz8863_iacr0_1_reg_t req_hdr;
    req_hdr.read_write = KSZ8863_INDIR_ACCESS_READ;
    req_hdr.table_sel = tbl;
    req_hdr.addr = ind_addr;

    // Indirect Access header is stored in opposite order in KSZ
    uint16_t swap_hdr = __builtin_bswap16(req_hdr.val); // TODO: this maybe too GCC specific
    s_ksz8863_ctrl_intf->ksz8863_reg_write(KSZ8863_IACR0_ADDR, (uint8_t *)&swap_hdr, sizeof(req_hdr));

    // Indirect Access data is stored in opposite order in KSZ
    uint8_t read_data[KSZ8863_INDIR_DATA_MAX_SIZE];
    s_ksz8863_ctrl_intf->ksz8863_reg_read(KSZ8863_IDR0_ADDR - len + 1, read_data, len);
    if (tbl == KSZ8863_STA_MAC_TABLE) {
        ksz8863_swap_to_mac_tbl(read_data, data, true);
    } else if (tbl == KSZ8863_DYN_MAC_TABLE) {
        ksz8863_swap_to_mac_tbl(read_data, data, false);
    }

    return ESP_OK;
}

esp_err_t ksz8863_indirect_write(ksz8863_indir_access_tbls_t tbl, uint8_t ind_addr, void *data, size_t len)
{
    if (!(s_ksz8863_ctrl_intf->mode == KSZ8863_I2C_MODE || s_ksz8863_ctrl_intf->mode == KSZ8863_SPI_MODE)) {
        ESP_LOGD(TAG, "Indirect write is accessible only in I2C or SPI mode");
        return ESP_ERR_INVALID_STATE;
    }

    if (len > KSZ8863_INDIR_DATA_MAX_SIZE) {
        ESP_LOGD(TAG, "maximal %d bytes can be indirectly accessed at a time", KSZ8863_INDIR_DATA_MAX_SIZE);
        return ESP_ERR_INVALID_SIZE;
    }

    ksz8863_iacr0_1_reg_t req_hdr;
    req_hdr.read_write = KSZ8863_INDIR_ACCESS_WRITE;
    req_hdr.table_sel = tbl;
    req_hdr.addr = ind_addr;

    // Indirect Access data is stored in opposite order in KSZ
    uint16_t swap_hdr = __builtin_bswap16(req_hdr.val);
    uint8_t swap_data[KSZ8863_INDIR_DATA_MAX_SIZE];
    if (tbl == KSZ8863_STA_MAC_TABLE) {
        ksz8863_swap_from_mac_tbl(data, swap_data, true);
    } else if (tbl == KSZ8863_DYN_MAC_TABLE) {
        ksz8863_swap_from_mac_tbl(data, swap_data, false);
    }

    s_ksz8863_ctrl_intf->ksz8863_reg_write(KSZ8863_IDR0_ADDR - len + 1, swap_data, len);
    s_ksz8863_ctrl_intf->ksz8863_reg_write(KSZ8863_IACR0_ADDR, (uint8_t *)&swap_hdr, sizeof(req_hdr));

    return ESP_OK;
}

esp_err_t ksz8863_ctrl_intf_init(ksz8863_ctrl_intf_config_t *config)
{
    esp_err_t ret = ESP_OK;

    if (s_ksz8863_ctrl_intf != NULL) {
        ESP_LOGW(TAG, "Control Interface has been already initialized");
        return ESP_ERR_INVALID_STATE;
    }
    s_ksz8863_ctrl_intf = calloc(1, sizeof(ksz8863_ctrl_intf_t));
    ESP_RETURN_ON_FALSE(s_ksz8863_ctrl_intf, ESP_ERR_NO_MEM, TAG, "no memory");

    s_ksz8863_ctrl_intf->mode = config->host_mode;

    switch (config->host_mode) {
    case KSZ8863_I2C_MODE:
        i2c_device_config_t dev_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .scl_speed_hz = config->i2c_dev_config->scl_speed_hz,
            .device_address = config->i2c_dev_config->dev_addr >> 1,
        };
        ESP_GOTO_ON_ERROR(i2c_master_bus_add_device(config->i2c_dev_config->bus_handle, &dev_cfg, &s_ksz8863_ctrl_intf->i2c_handle), err, TAG, "Error when trying to add the I2C device");

        s_ksz8863_ctrl_intf->ksz8863_reg_read = ksz8863_i2c_read;
        s_ksz8863_ctrl_intf->ksz8863_reg_write = ksz8863_i2c_write;
        break;
    case KSZ8863_SPI_MODE:;
        ESP_GOTO_ON_FALSE(s_ksz8863_ctrl_intf->bus_lock = xSemaphoreCreateMutex(), ESP_ERR_NO_MEM, err, TAG, "mutex creation failed");

        spi_device_interface_config_t devcfg = {
            .command_bits = 8,
            .address_bits = 8,
            .mode = 0,
            .clock_speed_hz = config->spi_dev_config->clock_speed_hz,
            .spics_io_num = config->spi_dev_config->spics_io_num,
            .queue_size = 20
        };
        ESP_ERROR_CHECK(spi_bus_add_device(config->spi_dev_config->host_id, &devcfg, &s_ksz8863_ctrl_intf->spi_handle));

        s_ksz8863_ctrl_intf->ksz8863_reg_read = ksz8863_spi_read;
        s_ksz8863_ctrl_intf->ksz8863_reg_write = ksz8863_spi_write;
    default:
        break;
    }
    return ESP_OK;
err:
    free(s_ksz8863_ctrl_intf);
    return ret;
}

esp_err_t ksz8863_ctrl_intf_deinit(void)
{
    if (s_ksz8863_ctrl_intf != NULL) {
        switch (s_ksz8863_ctrl_intf->mode) {
        case KSZ8863_I2C_MODE:
            i2c_master_bus_rm_device(s_ksz8863_ctrl_intf->i2c_handle);
            break;
        case KSZ8863_SPI_MODE:
            vSemaphoreDelete(s_ksz8863_ctrl_intf->bus_lock);
            spi_bus_remove_device(s_ksz8863_ctrl_intf->spi_handle);
        default:
            break;
        }

        free(s_ksz8863_ctrl_intf);
        s_ksz8863_ctrl_intf = NULL;
    }
    return ESP_OK;
}
