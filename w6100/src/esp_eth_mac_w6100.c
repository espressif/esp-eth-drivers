/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include "esp_log.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_eth_mac_w6100.h"
#include "esp_idf_version.h"
#include "wiznet_mac_common.h"
#include "w6100.h"

static const char *TAG = "w6100.mac";

typedef struct {
    emac_wiznet_t base;  // Must be first member for safe casting
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
    uint8_t mcast_v4_cnt;
    uint8_t mcast_v6_cnt;
#endif
} emac_w6100_t;

/*******************************************************************************
 * Chip-specific ops for W6100
 ******************************************************************************/

static esp_err_t w6100_reset(emac_wiznet_t *emac);
static esp_err_t w6100_verify_id(emac_wiznet_t *emac);
static esp_err_t w6100_setup_default(emac_wiznet_t *emac);

static const wiznet_chip_ops_t w6100_ops = {
    /* Register translation table for common registers */
    .regs = {
        [WIZNET_REG_MAC_ADDR]        = W6100_REG_SHAR,
        [WIZNET_REG_SOCK_MR]         = W6100_REG_SOCK_MR(0),
        [WIZNET_REG_SOCK_IMR]        = W6100_REG_SOCK_IMR(0),
        [WIZNET_REG_SOCK_RXBUF_SIZE] = W6100_REG_SOCK_RX_BSR(0),
        [WIZNET_REG_SOCK_TXBUF_SIZE] = W6100_REG_SOCK_TX_BSR(0),
        [WIZNET_REG_INT_LEVEL]       = W6100_REG_INTPTMR,
    },

    /* Socket 0 registers (pre-computed addresses) */
    .reg_sock_cr = W6100_REG_SOCK_CR(0),
    .reg_sock_ir = W6100_REG_SOCK_IR(0),
    .reg_sock_tx_fsr = W6100_REG_SOCK_TX_FSR(0),
    .reg_sock_tx_wr = W6100_REG_SOCK_TX_WR(0),
    .reg_sock_rx_rsr = W6100_REG_SOCK_RX_RSR(0),
    .reg_sock_rx_rd = W6100_REG_SOCK_RX_RD(0),
    .reg_simr = W6100_REG_SIMR,

    /* Memory base addresses (offset added at runtime) */
    .mem_sock_tx_base = W6100_MEM_SOCK_TX(0, 0),
    .mem_sock_rx_base = W6100_MEM_SOCK_RX(0, 0),

    /* W6100 uses separate IRCLR register to clear interrupts */
    .reg_sock_irclr = W6100_REG_SOCK_IRCLR(0),

    /* Command values */
    .cmd_send = W6100_SCR_SEND,
    .cmd_recv = W6100_SCR_RECV,
    .cmd_open = W6100_SCR_OPEN,
    .cmd_close = W6100_SCR_CLOSE,

    /* Interrupt bits */
    .sir_send = W6100_SIR_SENDOK,
    .sir_recv = W6100_SIR_RECV,
    .simr_sock0 = W6100_SIMR_SOCK0,

    /* Bit masks */
    .smr_mac_filter = W6100_SMR_MF,
    .smr_mac_raw = W6100_SMR_MACRAW,
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
    /* Block IPv4/IPv6 multicast by default until add_mac_filter is called */
    .smr_default = W6100_SMR_MACRAW | W6100_SMR_MF | W6100_SMR_MMB | W6100_SMR_MMB6,
#else
    .smr_default = W6100_SMR_MACRAW | W6100_SMR_MF,
#endif

    /* PHY status register and link mask */
    .reg_phy_status = W6100_REG_PHYSR,
    .phy_link_mask = W6100_PHYSR_LNK,

    /* Chip-specific functions */
    .reset = w6100_reset,
    .verify_id = w6100_verify_id,
    .setup_default = w6100_setup_default,
};

static esp_err_t w6100_reset(emac_wiznet_t *emac)
{
    esp_err_t ret = ESP_OK;
    /* software reset - write 0 to RST bit to trigger reset */
    uint8_t sycr0 = 0x00; // Clear RST bit (bit 7) to reset
    ESP_GOTO_ON_ERROR(wiznet_write(emac, W6100_REG_SYCR0, &sycr0, sizeof(sycr0)), err, TAG, "write SYCR0 failed");

    /* Wait for reset to complete - need to wait for chip to stabilize */
    vTaskDelay(pdMS_TO_TICKS(100));  // W6100 needs ~60.3ms after reset

    return ESP_OK;
err:
    return ret;
}

static esp_err_t w6100_verify_id(emac_wiznet_t *emac)
{
    esp_err_t ret = ESP_OK;
    uint16_t chip_id = 0;
    uint16_t version = 0;

    // Read chip ID
    ESP_LOGD(TAG, "Waiting W6100 to start & verify chip ID...");
    uint32_t to = 0;
    for (to = 0; to < emac->sw_reset_timeout_ms / 10; to++) {
        ESP_GOTO_ON_ERROR(wiznet_read(emac, W6100_REG_CIDR, &chip_id, sizeof(chip_id)), err, TAG, "read CIDR failed");
        chip_id = __builtin_bswap16(chip_id);
        if (chip_id == W6100_CHIP_ID) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    if (chip_id != W6100_CHIP_ID) {
        ESP_LOGE(TAG, "W6100 chip ID mismatched, expected 0x%04x, got 0x%04" PRIx16, W6100_CHIP_ID, chip_id);
        return ESP_ERR_INVALID_VERSION;
    }

    // Also verify version
    ESP_GOTO_ON_ERROR(wiznet_read(emac, W6100_REG_VER, &version, sizeof(version)), err, TAG, "read VER failed");
    version = __builtin_bswap16(version);
    ESP_LOGI(TAG, "W6100 chip ID: 0x%04" PRIx16 ", version: 0x%04" PRIx16, chip_id, version);

    return ESP_OK;
err:
    return ret;
}

static esp_err_t w6100_setup_default(emac_wiznet_t *emac)
{
    esp_err_t ret = ESP_OK;
    uint8_t reg_value;

    /* W6100 requires unlocking network configuration before modifying registers */
    reg_value = W6100_NETLCKR_UNLOCK;
    ESP_GOTO_ON_ERROR(wiznet_write(emac, W6100_REG_NETLCKR, &reg_value, sizeof(reg_value)),
                      err, TAG, "unlock network config failed");

    /* Clear network mode register - disable IPv4/IPv6 blocking.
     * Even in MACRAW mode, NETMR bits can affect frame reception. */
    reg_value = 0;
    ESP_GOTO_ON_ERROR(wiznet_write(emac, W6100_REG_NETMR, &reg_value, sizeof(reg_value)),
                      err, TAG, "write NETMR failed");

    /* Common setup: buffer allocation, socket mode, interrupts */
    ESP_GOTO_ON_ERROR(wiznet_setup_default(emac), err, TAG, "common setup failed");

    /* Enable global interrupt. SYCR1 defaults to 0x80 (IEN=1), but we write
     * it explicitly for clarity and to ensure proper operation after reset. */
    reg_value = W6100_SYCR1_IEN;
    ESP_GOTO_ON_ERROR(wiznet_write(emac, W6100_REG_SYCR1, &reg_value, sizeof(reg_value)),
                      err, TAG, "write SYCR1 failed");

err:
    return ret;
}

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
/**
 * @brief Set multicast blocking state for IPv4 and IPv6
 *
 * Per datasheet: MMB=1/MMB6=1 blocks multicast, MMB=0/MMB6=0 allows it.
 */
static esp_err_t emac_w6100_set_mcast_block(emac_w6100_t *emac, bool block_v4, bool block_v6)
{
    esp_err_t ret = ESP_OK;
    uint8_t smr;
    ESP_GOTO_ON_ERROR(wiznet_read(&emac->base, W6100_REG_SOCK_MR(0), &smr, sizeof(smr)), err, TAG, "read SMR failed");
    ESP_LOGD(TAG, "set_mcast_block: block_v4=%d, block_v6=%d, SMR before=0x%02x", block_v4, block_v6, smr);
    /* Datasheet logic: set bit to block, clear bit to allow */
    if (block_v4) {
        smr |= W6100_SMR_MMB;    // Set to block
    } else {
        smr &= ~W6100_SMR_MMB;   // Clear to allow
    }
    if (block_v6) {
        smr |= W6100_SMR_MMB6;   // Set to block
    } else {
        smr &= ~W6100_SMR_MMB6;  // Clear to allow
    }
    ESP_GOTO_ON_ERROR(wiznet_write(&emac->base, W6100_REG_SOCK_MR(0), &smr, sizeof(smr)), err, TAG, "write SMR failed");
    ESP_LOGD(TAG, "set_mcast_block: SMR after=0x%02x (MMB=%d, MMB6=%d)", smr, (smr & W6100_SMR_MMB) ? 1 : 0, (smr & W6100_SMR_MMB6) ? 1 : 0);
err:
    return ret;
}

static esp_err_t emac_w6100_add_mac_filter(esp_eth_mac_t *mac, uint8_t *addr)
{
    esp_err_t ret = ESP_OK;
    emac_w6100_t *emac = __containerof(mac, emac_w6100_t, base.parent);
    ESP_LOGD(TAG, "add_mac_filter: %02x:%02x:%02x:%02x:%02x:%02x (v4_cnt=%d, v6_cnt=%d)",
             addr[0], addr[1], addr[2], addr[3], addr[4], addr[5],
             emac->mcast_v4_cnt, emac->mcast_v6_cnt);
    // W6100 doesn't have specific MAC filter, so we just un-block multicast.
    if (addr[0] == 0x01 && addr[1] == 0x00 && addr[2] == 0x5e) {
        // IPv4 multicast
        if (emac->mcast_v4_cnt == 0) {
            ESP_GOTO_ON_ERROR(emac_w6100_set_mcast_block(emac, false, emac->mcast_v6_cnt == 0),
                              err, TAG, "set multicast block failed");
        }
        emac->mcast_v4_cnt++;
    } else if (addr[0] == 0x33 && addr[1] == 0x33) {
        // IPv6 multicast
        if (emac->mcast_v6_cnt == 0) {
            ESP_GOTO_ON_ERROR(emac_w6100_set_mcast_block(emac, emac->mcast_v4_cnt == 0, false),
                              err, TAG, "set multicast block failed");
        }
        emac->mcast_v6_cnt++;
    } else {
        ESP_LOGE(TAG, "W6100 filters in IP multicast frames only!");
        ret = ESP_ERR_NOT_SUPPORTED;
    }
err:
    return ret;
}

static esp_err_t emac_w6100_rm_mac_filter(esp_eth_mac_t *mac, uint8_t *addr)
{
    esp_err_t ret = ESP_OK;
    emac_w6100_t *emac = __containerof(mac, emac_w6100_t, base.parent);
    ESP_LOGD(TAG, "rm_mac_filter: %02x:%02x:%02x:%02x:%02x:%02x (v4_cnt=%d, v6_cnt=%d)",
             addr[0], addr[1], addr[2], addr[3], addr[4], addr[5],
             emac->mcast_v4_cnt, emac->mcast_v6_cnt);
    if (addr[0] == 0x01 && addr[1] == 0x00 && addr[2] == 0x5e) {
        // IPv4 multicast
        if (emac->mcast_v4_cnt > 0) {
            emac->mcast_v4_cnt--;
            if (emac->mcast_v4_cnt == 0) {
                ESP_GOTO_ON_ERROR(emac_w6100_set_mcast_block(emac, true, emac->mcast_v6_cnt == 0),
                                  err, TAG, "set multicast block failed");
            }
        }
    } else if (addr[0] == 0x33 && addr[1] == 0x33) {
        // IPv6 multicast
        if (emac->mcast_v6_cnt > 0) {
            emac->mcast_v6_cnt--;
            if (emac->mcast_v6_cnt == 0) {
                ESP_GOTO_ON_ERROR(emac_w6100_set_mcast_block(emac, emac->mcast_v4_cnt == 0, true),
                                  err, TAG, "set multicast block failed");
            }
        }
    } else {
        ESP_LOGE(TAG, "W6100 filters in IP multicast frames only!");
        ret = ESP_ERR_NOT_SUPPORTED;
    }
err:
    return ret;
}

static esp_err_t emac_w6100_set_all_multicast(esp_eth_mac_t *mac, bool enable)
{
    emac_w6100_t *emac = __containerof(mac, emac_w6100_t, base.parent);
    ESP_RETURN_ON_ERROR(emac_w6100_set_mcast_block(emac, !enable, !enable), TAG, "set multicast block failed");
    emac->mcast_v4_cnt = 0;
    emac->mcast_v6_cnt = 0;
    if (enable) {
        ESP_LOGW(TAG, "W6100 filters in IP multicast frames only!");
    }
    return ESP_OK;
}
#endif // ESP_IDF_VERSION >= 6.0.0

esp_eth_mac_t *esp_eth_mac_new_w6100(const eth_w6100_config_t *w6100_config, const eth_mac_config_t *mac_config)
{
    esp_eth_mac_t *ret = NULL;
    emac_w6100_t *emac = NULL;
    ESP_GOTO_ON_FALSE(w6100_config && mac_config, NULL, err, TAG, "invalid argument");
    ESP_GOTO_ON_FALSE((w6100_config->int_gpio_num >= 0) != (w6100_config->poll_period_ms > 0), NULL, err, TAG, "invalid configuration argument combination");
    emac = calloc(1, sizeof(emac_w6100_t));
    ESP_GOTO_ON_FALSE(emac, NULL, err, TAG, "no mem for MAC instance");

    /* Initialize common parts */
    ESP_GOTO_ON_FALSE(emac_wiznet_init_common(&emac->base,
                                              (const eth_wiznet_config_t *)w6100_config,
                                              mac_config,
                                              &w6100_ops,
                                              TAG,
                                              "w6100_tsk") == ESP_OK, NULL, err, TAG, "common init failed");

    /* Set chip-specific methods */
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
    emac->base.parent.add_mac_filter = emac_w6100_add_mac_filter;
    emac->base.parent.rm_mac_filter = emac_w6100_rm_mac_filter;
    emac->base.parent.set_all_multicast = emac_w6100_set_all_multicast;
#endif

    return &(emac->base.parent);

err:
    if (emac) {
        emac_wiznet_cleanup_common(&emac->base);
        free(emac);
    }
    return ret;
}
