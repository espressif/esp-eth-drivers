/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include "esp_eth_mac_w5500.h"
#include "esp_log.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "wiznet_mac_common.h"
#include "w5500.h"

static const char *TAG = "w5500.mac";

typedef struct {
    emac_wiznet_t base;  // Must be first member for safe casting
    uint8_t mcast_cnt;
} emac_w5500_t;

/*******************************************************************************
 * Chip-specific ops for W5500
 ******************************************************************************/

static esp_err_t w5500_reset(emac_wiznet_t *emac);
static esp_err_t w5500_verify_id(emac_wiznet_t *emac);

static const wiznet_chip_ops_t w5500_ops = {
    /* Register translation table for common registers */
    .regs = {
        [WIZNET_REG_MAC_ADDR]        = W5500_REG_MAC,
        [WIZNET_REG_SOCK_MR]         = W5500_REG_SOCK_MR(0),
        [WIZNET_REG_SOCK_IMR]        = W5500_REG_SOCK_IMR(0),
        [WIZNET_REG_SOCK_RXBUF_SIZE] = W5500_REG_SOCK_RXBUF_SIZE(0),
        [WIZNET_REG_SOCK_TXBUF_SIZE] = W5500_REG_SOCK_TXBUF_SIZE(0),
        [WIZNET_REG_INT_LEVEL]       = W5500_REG_INTLEVEL,
    },

    /* Socket 0 registers (pre-computed addresses) */
    .reg_sock_cr = W5500_REG_SOCK_CR(0),
    .reg_sock_ir = W5500_REG_SOCK_IR(0),
    .reg_sock_tx_fsr = W5500_REG_SOCK_TX_FSR(0),
    .reg_sock_tx_wr = W5500_REG_SOCK_TX_WR(0),
    .reg_sock_rx_rsr = W5500_REG_SOCK_RX_RSR(0),
    .reg_sock_rx_rd = W5500_REG_SOCK_RX_RD(0),
    .reg_simr = W5500_REG_SIMR,

    /* Memory base addresses (offset added at runtime) */
    .mem_sock_tx_base = W5500_MEM_SOCK_TX(0, 0),
    .mem_sock_rx_base = W5500_MEM_SOCK_RX(0, 0),

    /* W5500 writes to IR register to clear interrupts (same as read) */
    .reg_sock_irclr = W5500_REG_SOCK_IR(0),

    /* Command values */
    .cmd_send = W5500_SCR_SEND,
    .cmd_recv = W5500_SCR_RECV,
    .cmd_open = W5500_SCR_OPEN,
    .cmd_close = W5500_SCR_CLOSE,

    /* Interrupt bits */
    .sir_send = W5500_SIR_SEND,
    .sir_recv = W5500_SIR_RECV,
    .simr_sock0 = W5500_SIMR_SOCK0,

    /* Bit masks */
    .smr_mac_filter = W5500_SMR_MAC_FILTER,
    .smr_mac_raw = W5500_SMR_MAC_RAW,
    .smr_default = W5500_SMR_MAC_RAW | W5500_SMR_MAC_FILTER | W5500_SMR_MAC_BLOCK_MCAST,

    /* PHY status register and link mask */
    .reg_phy_status = W5500_REG_PHYCFGR,
    .phy_link_mask = W5500_PHYCFGR_LNK,  /* Check link status bit */

    /* Chip-specific functions */
    .reset = w5500_reset,
    .verify_id = w5500_verify_id,
    .setup_default = wiznet_setup_default,
};

static esp_err_t w5500_reset(emac_wiznet_t *emac)
{
    esp_err_t ret = ESP_OK;
    /* software reset */
    uint8_t mr = W5500_MR_RST; // Set RST bit (auto clear)
    ESP_GOTO_ON_ERROR(wiznet_write(emac, W5500_REG_MR, &mr, sizeof(mr)), err, TAG, "write MR failed");
    uint32_t to = 0;
    for (to = 0; to < emac->sw_reset_timeout_ms / 10; to++) {
        ESP_GOTO_ON_ERROR(wiznet_read(emac, W5500_REG_MR, &mr, sizeof(mr)), err, TAG, "read MR failed");
        if (!(mr & W5500_MR_RST)) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    ESP_GOTO_ON_FALSE(to < emac->sw_reset_timeout_ms / 10, ESP_ERR_TIMEOUT, err, TAG, "reset timeout");

err:
    return ret;
}

static esp_err_t w5500_verify_id(emac_wiznet_t *emac)
{
    esp_err_t ret = ESP_OK;
    uint8_t version = 0;

    // W5500 doesn't have chip ID, we check the version number instead
    // The version number may be polled multiple times since it was observed that
    // some W5500 units may return version 0 when it is read right after the reset
    ESP_LOGD(TAG, "Waiting W5500 to start & verify version...");
    uint32_t to = 0;
    for (to = 0; to < emac->sw_reset_timeout_ms / 10; to++) {
        ESP_GOTO_ON_ERROR(wiznet_read(emac, W5500_REG_VERSIONR, &version, sizeof(version)), err, TAG, "read VERSIONR failed");
        if (version == W5500_CHIP_VERSION) {
            return ESP_OK;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    ESP_LOGE(TAG, "W5500 version mismatched, expected 0x%02x, got 0x%02" PRIx8, W5500_CHIP_VERSION, version);
    return ESP_ERR_INVALID_VERSION;
err:
    return ret;
}

static esp_err_t emac_w5500_set_block_ip4_mcast(esp_eth_mac_t *mac, bool block)
{
    esp_err_t ret = ESP_OK;
    emac_w5500_t *emac = __containerof(mac, emac_w5500_t, base.parent);
    uint8_t smr;
    ESP_GOTO_ON_ERROR(wiznet_read(&emac->base, W5500_REG_SOCK_MR(0), &smr, sizeof(smr)), err, TAG, "read SMR failed");
    if (block) {
        smr |= W5500_SMR_MAC_BLOCK_MCAST;
    } else {
        smr &= ~W5500_SMR_MAC_BLOCK_MCAST;
    }
    ESP_GOTO_ON_ERROR(wiznet_write(&emac->base, W5500_REG_SOCK_MR(0), &smr, sizeof(smr)), err, TAG, "write SMR failed");
err:
    return ret;
}

static esp_err_t emac_w5500_add_mac_filter(esp_eth_mac_t *mac, uint8_t *addr)
{
    esp_err_t ret = ESP_OK;
    emac_w5500_t *emac = __containerof(mac, emac_w5500_t, base.parent);
    // W5500 doesn't have specific MAC filter, so we just un-block multicast. W5500 filters out all multicast packets
    // except for IP multicast. However, behavior is not consistent. IPv4 multicast can be blocked, but IPv6 is always
    // accepted (this is not documented behavior, but it's observed on the real hardware).
    if (addr[0] == 0x01 && addr[1] == 0x00 && addr[2] == 0x5e) {
        ESP_GOTO_ON_ERROR(emac_w5500_set_block_ip4_mcast(mac, false), err, TAG, "set block multicast failed");
        emac->mcast_cnt++;
    } else if (addr[0] == 0x33 && addr[1] == 0x33) {
        ESP_LOGW(TAG, "IPv6 multicast is always filtered in by W5500.");
    } else {
        ESP_LOGE(TAG, "W5500 filters in IP multicast frames only!");
        ret = ESP_ERR_NOT_SUPPORTED;
    }
err:
    return ret;
}

static esp_err_t emac_w5500_del_mac_filter(esp_eth_mac_t *mac, uint8_t *addr)
{
    esp_err_t ret = ESP_OK;
    emac_w5500_t *emac = __containerof(mac, emac_w5500_t, base.parent);

    ESP_GOTO_ON_FALSE(!(addr[0] == 0x33 && addr[1] == 0x33), ESP_FAIL, err, TAG, "IPv6 multicast is always filtered in by W5500.");

    if (addr[0] == 0x01 && addr[1] == 0x00 && addr[2] == 0x5e && emac->mcast_cnt > 0) {
        emac->mcast_cnt--;
    }
    if (emac->mcast_cnt == 0) {
        // W5500 doesn't have specific MAC filter, so we just block multicast
        ESP_GOTO_ON_ERROR(emac_w5500_set_block_ip4_mcast(mac, true), err, TAG, "set block multicast failed");
    }
err:
    return ret;
}

static esp_err_t emac_w5500_set_all_multicast(esp_eth_mac_t *mac, bool enable)
{
    emac_w5500_t *emac = __containerof(mac, emac_w5500_t, base.parent);
    ESP_RETURN_ON_ERROR(emac_w5500_set_block_ip4_mcast(mac, !enable), TAG, "set block multicast failed");
    emac->mcast_cnt = 0;
    if (enable) {
        ESP_LOGW(TAG, "W5500 filters in IP multicast frames only!");
    } else {
        ESP_LOGW(TAG, "W5500 always filters in IPv6 multicast frames!");
    }
    return ESP_OK;
}

esp_eth_mac_t *esp_eth_mac_new_w5500(const eth_w5500_config_t *w5500_config, const eth_mac_config_t *mac_config)
{
    esp_eth_mac_t *ret = NULL;
    emac_w5500_t *emac = NULL;
    ESP_GOTO_ON_FALSE(w5500_config && mac_config, NULL, err, TAG, "invalid argument");
    ESP_GOTO_ON_FALSE((w5500_config->int_gpio_num >= 0) != (w5500_config->poll_period_ms > 0), NULL, err, TAG, "invalid configuration argument combination");
    emac = calloc(1, sizeof(emac_w5500_t));
    ESP_GOTO_ON_FALSE(emac, NULL, err, TAG, "no mem for MAC instance");

    /* Initialize common parts */
    ESP_GOTO_ON_FALSE(emac_wiznet_init_common(&emac->base,
                                              (const eth_wiznet_config_t *)w5500_config,
                                              mac_config,
                                              &w5500_ops,
                                              TAG,
                                              "w5500_tsk") == ESP_OK, NULL, err, TAG, "common init failed");

    /* Set chip-specific methods */
    emac->base.parent.add_mac_filter = emac_w5500_add_mac_filter;
    emac->base.parent.rm_mac_filter = emac_w5500_del_mac_filter;
    emac->base.parent.set_all_multicast = emac_w5500_set_all_multicast;

    return &(emac->base.parent);

err:
    if (emac) {
        emac_wiznet_cleanup_common(&emac->base);
        free(emac);
    }
    return ret;
}
