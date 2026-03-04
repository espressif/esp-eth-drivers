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

/*******************************************************************************
 * Chip-specific ops for W6100
 ******************************************************************************/

static esp_err_t w6100_mac_reset(emac_wiznet_t *emac);
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
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
    /* Block IPv4/IPv6 multicast by default until add_mac_filter is called */
    .smr_default = W6100_SMR_MACRAW | W6100_SMR_MF | W6100_SMR_MMB | W6100_SMR_MMB6,
#else
    .smr_default = W6100_SMR_MACRAW | W6100_SMR_MF,
#endif

    /* PHY status register and link mask */
    .reg_phy_status = W6100_REG_PHYSR,
    .phy_link_mask = W6100_PHYSR_LNK,

    /* Chip-specific functions */
    .reset = w6100_mac_reset,
    .verify_id = w6100_verify_id,
    .setup_default = w6100_setup_default,
};

static esp_err_t w6100_mac_reset(emac_wiznet_t *emac)
{
    esp_err_t ret = ESP_OK;
    /* software reset - write 0 to RST bit to trigger reset */
    uint8_t sycr0 = 0x00; // Clear RST bit (bit 7) to reset
    ESP_GOTO_ON_ERROR(wiznet_write(emac, W6100_REG_SYCR0, &sycr0, sizeof(sycr0)), err, TAG, "write SYCR0 failed");

    /* Wait for reset to complete. Datasheet specifies T_STA = 60.3ms stabilization time.
     * SYCR0 is write-only so we cannot poll it. Chip readiness is verified by w6100_verify_id. */
    vTaskDelay(pdMS_TO_TICKS(emac_wiznet_get_sw_reset_timeout_ms(emac)));

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
    uint32_t retries = emac_wiznet_get_sw_reset_timeout_ms(emac) / 10;
    for (to = 0; to < retries; to++) {
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

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
/**
 * @brief Set IPv4 multicast blocking state
 *
 * Per datasheet: MMB=1 blocks IPv4 multicast, MMB=0 allows it.
 */
static esp_err_t emac_w6100_set_block_v4_mcast(emac_wiznet_t *emac, bool block)
{
    esp_err_t ret = ESP_OK;
    uint8_t smr;
    ESP_GOTO_ON_ERROR(wiznet_read(emac, W6100_REG_SOCK_MR(0), &smr, sizeof(smr)), err, TAG, "read SMR failed");
    if (block) {
        smr |= W6100_SMR_MMB;
    } else {
        smr &= ~W6100_SMR_MMB;
    }
    ESP_GOTO_ON_ERROR(wiznet_write(emac, W6100_REG_SOCK_MR(0), &smr, sizeof(smr)), err, TAG, "write SMR failed");
err:
    return ret;
}

/**
 * @brief Set IPv6 multicast blocking state
 *
 * Per datasheet: MMB6=1 blocks IPv6 multicast, MMB6=0 allows it.
 */
static esp_err_t emac_w6100_set_block_v6_mcast(emac_wiznet_t *emac, bool block)
{
    esp_err_t ret = ESP_OK;
    uint8_t smr;
    ESP_GOTO_ON_ERROR(wiznet_read(emac, W6100_REG_SOCK_MR(0), &smr, sizeof(smr)), err, TAG, "read SMR failed");
    if (block) {
        smr |= W6100_SMR_MMB6;
    } else {
        smr &= ~W6100_SMR_MMB6;
    }
    ESP_GOTO_ON_ERROR(wiznet_write(emac, W6100_REG_SOCK_MR(0), &smr, sizeof(smr)), err, TAG, "write SMR failed");
err:
    return ret;
}

static esp_err_t emac_w6100_add_mac_filter(esp_eth_mac_t *mac, uint8_t *addr)
{
    esp_err_t ret = ESP_OK;
    emac_wiznet_t *emac = emac_wiznet_from_parent(mac);
    uint8_t v4_cnt = emac_wiznet_get_mcast_v4_cnt(emac);
    uint8_t v6_cnt = emac_wiznet_get_mcast_v6_cnt(emac);
    ESP_LOGD(TAG, "add_mac_filter: %02x:%02x:%02x:%02x:%02x:%02x (v4_cnt=%d, v6_cnt=%d)",
             addr[0], addr[1], addr[2], addr[3], addr[4], addr[5], v4_cnt, v6_cnt);
    // W6100 doesn't have specific MAC filter, so we just un-block multicast.
    if (addr[0] == 0x01 && addr[1] == 0x00 && addr[2] == 0x5e) {
        // IPv4 multicast
        if (v4_cnt == 0) {
            ESP_GOTO_ON_ERROR(emac_w6100_set_block_v4_mcast(emac, false),
                              err, TAG, "unblock IPv4 multicast failed");
        }
        emac_wiznet_set_mcast_v4_cnt(emac, v4_cnt + 1);
    } else if (addr[0] == 0x33 && addr[1] == 0x33) {
        // IPv6 multicast
        if (v6_cnt == 0) {
            ESP_GOTO_ON_ERROR(emac_w6100_set_block_v6_mcast(emac, false),
                              err, TAG, "unblock IPv6 multicast failed");
        }
        emac_wiznet_set_mcast_v6_cnt(emac, v6_cnt + 1);
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
    emac_wiznet_t *emac = emac_wiznet_from_parent(mac);
    uint8_t v4_cnt = emac_wiznet_get_mcast_v4_cnt(emac);
    uint8_t v6_cnt = emac_wiznet_get_mcast_v6_cnt(emac);
    ESP_LOGD(TAG, "rm_mac_filter: %02x:%02x:%02x:%02x:%02x:%02x (v4_cnt=%d, v6_cnt=%d)",
             addr[0], addr[1], addr[2], addr[3], addr[4], addr[5], v4_cnt, v6_cnt);
    if (addr[0] == 0x01 && addr[1] == 0x00 && addr[2] == 0x5e) {
        // IPv4 multicast
        if (v4_cnt > 0) {
            v4_cnt--;
            emac_wiznet_set_mcast_v4_cnt(emac, v4_cnt);
            if (v4_cnt == 0) {
                ESP_GOTO_ON_ERROR(emac_w6100_set_block_v4_mcast(emac, true),
                                  err, TAG, "block IPv4 multicast failed");
            }
        }
    } else if (addr[0] == 0x33 && addr[1] == 0x33) {
        // IPv6 multicast
        if (v6_cnt > 0) {
            v6_cnt--;
            emac_wiznet_set_mcast_v6_cnt(emac, v6_cnt);
            if (v6_cnt == 0) {
                ESP_GOTO_ON_ERROR(emac_w6100_set_block_v6_mcast(emac, true),
                                  err, TAG, "block IPv6 multicast failed");
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
    emac_wiznet_t *emac = emac_wiznet_from_parent(mac);
    ESP_RETURN_ON_ERROR(emac_w6100_set_block_v4_mcast(emac, !enable), TAG, "set IPv4 multicast block failed");
    ESP_RETURN_ON_ERROR(emac_w6100_set_block_v6_mcast(emac, !enable), TAG, "set IPv6 multicast block failed");
    emac_wiznet_set_mcast_v4_cnt(emac, 0);
    emac_wiznet_set_mcast_v6_cnt(emac, 0);
    return ESP_OK;
}
#endif // ESP_IDF_VERSION >= 5.5.0

esp_eth_mac_t *esp_eth_mac_new_w6100(const eth_w6100_config_t *w6100_config, const eth_mac_config_t *mac_config)
{
    esp_eth_mac_t *ret = NULL;
    emac_wiznet_t *emac = NULL;

    ESP_GOTO_ON_FALSE(w6100_config && mac_config, NULL, err, TAG, "invalid argument");
    ESP_GOTO_ON_FALSE((w6100_config->int_gpio_num >= 0) != (w6100_config->poll_period_ms > 0), NULL, err, TAG, "invalid configuration argument combination");

    /* Create common EMAC instance via factory */
    emac = emac_wiznet_new((const eth_wiznet_config_t *)w6100_config,
                           mac_config,
                           &w6100_ops,
                           TAG,
                           "w6100_tsk");
    ESP_GOTO_ON_FALSE(emac, NULL, err, TAG, "emac_wiznet_new failed");

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
    /* Set chip-specific methods */
    esp_eth_mac_t *mac = emac_wiznet_get_parent(emac);
    mac->add_mac_filter = emac_w6100_add_mac_filter;
    mac->rm_mac_filter = emac_w6100_rm_mac_filter;
    mac->set_all_multicast = emac_w6100_set_all_multicast;

    return mac;
#else
    return emac_wiznet_get_parent(emac);
#endif

err:
    if (emac) {
        emac_wiznet_get_parent(emac)->del(emac_wiznet_get_parent(emac));
    }
    return ret;
}
