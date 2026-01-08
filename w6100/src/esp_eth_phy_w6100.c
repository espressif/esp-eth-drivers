/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include <stdlib.h>
#include <sys/cdefs.h>
#include "esp_eth_phy.h"
#include "esp_log.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "w6100.h"
#include "wiznet_phy_common.h"

#define W6100_WAIT_FOR_RESET_MS (10) // wait for W6100 internal PHY after reset

static const char *TAG = "w6100.phy";

/***************Vendor Specific Register***************/
/**
 * @brief PHYSR (PHY Status Register) bit definitions
 *
 * W6100 PHYSR bit layout:
 * - Bit 7: CAB (Cable Off, 1=unplugged)
 * - Bits 5:3: MODE (Operation mode)
 * - Bit 2: DPX (Duplex, 1=half, 0=full) - NOTE: Inverted from W5500!
 * - Bit 1: SPD (Speed, 1=10Mbps, 0=100Mbps) - NOTE: Inverted from W5500!
 * - Bit 0: LNK (Link, 1=up, 0=down)
 */
typedef union {
    struct {
        uint8_t link: 1;   /*!< Link status (1=up, 0=down) */
        uint8_t speed: 1;  /*!< Speed status (1=10M, 0=100M) - INVERTED from W5500 */
        uint8_t duplex: 1; /*!< Duplex status (1=half, 0=full) - INVERTED from W5500 */
        uint8_t opmode: 3; /*!< Operation mode */
        uint8_t reserved: 1;
        uint8_t cab: 1;    /*!< Cable off (1=unplugged) */
    };
    uint8_t val;
} physr_reg_t;

/**
 * @brief PHY operation modes for PHYCR0 and PHYSR MODE field
 *
 * These values are used both for configuring PHYCR0 and reading the
 * MODE field from PHYSR (bits [5:3]).
 */
typedef enum {
    W6100_OP_MODE_AUTO = 0x00,          /*!< Auto negotiation */
    W6100_OP_MODE_100BT_FULL = 0x04,    /*!< 100BASE-TX Full Duplex */
    W6100_OP_MODE_100BT_HALF = 0x05,    /*!< 100BASE-TX Half Duplex */
    W6100_OP_MODE_10BT_FULL = 0x06,     /*!< 10BASE-T Full Duplex */
    W6100_OP_MODE_10BT_HALF = 0x07,     /*!< 10BASE-T Half Duplex */
} phy_w6100_op_mode_e;

/* Opmode table for get_mode lookup - only fixed modes, autoneg modes fall through */
static const wiznet_opmode_entry_t w6100_opmode_table[] = {
    { W6100_OP_MODE_100BT_FULL, ETH_SPEED_100M, ETH_DUPLEX_FULL },
    { W6100_OP_MODE_100BT_HALF, ETH_SPEED_100M, ETH_DUPLEX_HALF },
    { W6100_OP_MODE_10BT_FULL,  ETH_SPEED_10M,  ETH_DUPLEX_FULL },
    { W6100_OP_MODE_10BT_HALF,  ETH_SPEED_10M,  ETH_DUPLEX_HALF },
};

/**
 * @brief PHYCR1 (PHY Control Register 1) bit definitions
 */
typedef union {
    struct {
        uint8_t reset: 1;    /*!< PHY Reset (write 1 to reset) */
        uint8_t reserved1: 2;
        uint8_t te: 1;       /*!< 10BASE-Te mode */  // codespell:ignore te
        uint8_t reserved2: 1;
        uint8_t pwdn: 1;     /*!< Power Down */
        uint8_t reserved3: 2;
    };
    uint8_t val;
} phycr1_reg_t;

static esp_err_t w6100_reset(esp_eth_phy_t *phy)
{
    esp_err_t ret = ESP_OK;
    phy_wiznet_t *w6100 = __containerof(phy, phy_wiznet_t, parent);
    w6100->link_status = ETH_LINK_DOWN;
    esp_eth_mediator_t *eth = w6100->eth;

    // Unlock PHY configuration
    ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, w6100->addr, W6100_REG_PHYLCKR, W6100_PHYLCKR_UNLOCK), err, TAG, "unlock PHY failed");

    // Reset PHY by setting reset bit in PHYCR1
    phycr1_reg_t phycr1;
    /* Cast safe: Wiznet MAC's read_phy_reg only writes 1 byte to the pointer despite uint32_t* parameter */
    ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, w6100->addr, W6100_REG_PHYCR1, (uint32_t *) & (phycr1.val)), err, TAG, "read PHYCR1 failed");
    phycr1.reset = 1;
    ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, w6100->addr, W6100_REG_PHYCR1, phycr1.val), err, TAG, "write PHYCR1 failed");

    vTaskDelay(pdMS_TO_TICKS(W6100_WAIT_FOR_RESET_MS));

    phycr1.reset = 0;
    ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, w6100->addr, W6100_REG_PHYCR1, phycr1.val), err, TAG, "write PHYCR1 failed");

    return ESP_OK;
err:
    return ret;
}

static esp_err_t w6100_is_autoneg_enabled(phy_wiznet_t *wiznet, bool *enabled)
{
    esp_err_t ret = ESP_OK;
    esp_eth_mediator_t *eth = wiznet->eth;
    physr_reg_t physr;

    /* Cast safe: Wiznet MAC's read_phy_reg only writes 1 byte to the pointer despite uint32_t* parameter */
    ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, wiznet->addr, W6100_REG_PHYSR, (uint32_t *)&physr.val),
                      err, TAG, "read PHYSR failed");
    /* PHYSR MODE[2] (bit 5 of register) = 0 means auto negotiation
     * MODE 0xx = Auto, MODE 1xx = Fixed mode */
    *enabled = ((physr.opmode & 0x04) == 0);
    return ESP_OK;
err:
    return ret;
}

static esp_err_t w6100_set_mode(phy_wiznet_t *wiznet, bool autoneg, eth_speed_t speed, eth_duplex_t duplex)
{
    esp_err_t ret = ESP_OK;
    esp_eth_mediator_t *eth = wiznet->eth;

    // Unlock PHY configuration
    ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, wiznet->addr, W6100_REG_PHYLCKR, W6100_PHYLCKR_UNLOCK),
                      err, TAG, "unlock PHY failed");

    uint32_t phycr0;
    if (autoneg) {
        phycr0 = W6100_OP_MODE_AUTO;
    } else {
        /* Set fixed mode based on speed/duplex */
        if (duplex == ETH_DUPLEX_FULL) {
            phycr0 = (speed == ETH_SPEED_100M) ? W6100_OP_MODE_100BT_FULL : W6100_OP_MODE_10BT_FULL;
        } else {
            phycr0 = (speed == ETH_SPEED_100M) ? W6100_OP_MODE_100BT_HALF : W6100_OP_MODE_10BT_HALF;
        }
    }
    ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, wiznet->addr, W6100_REG_PHYCR0, phycr0), err, TAG, "write PHYCR0 failed");
    return ESP_OK;
err:
    return ret;
}

static esp_err_t w6100_pwrctl(esp_eth_phy_t *phy, bool enable)
{
    esp_err_t ret = ESP_OK;
    phy_wiznet_t *w6100 = __containerof(phy, phy_wiznet_t, parent);
    esp_eth_mediator_t *eth = w6100->eth;

    // Unlock PHY configuration
    ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, w6100->addr, W6100_REG_PHYLCKR, W6100_PHYLCKR_UNLOCK), err, TAG, "unlock PHY failed");

    phycr1_reg_t phycr1;
    /* Cast safe: Wiznet MAC's read_phy_reg only writes 1 byte to the pointer despite uint32_t* parameter */
    ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, w6100->addr, W6100_REG_PHYCR1, (uint32_t *) & (phycr1.val)), err, TAG, "read PHYCR1 failed");
    phycr1.pwdn = enable ? 0 : 1;  // 0 = power on, 1 = power down
    ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, w6100->addr, W6100_REG_PHYCR1, phycr1.val), err, TAG, "write PHYCR1 failed");

    if (enable) {
        // Wait for PHY to power up
        vTaskDelay(pdMS_TO_TICKS(W6100_WAIT_FOR_RESET_MS));
    }
    return ESP_OK;
err:
    return ret;
}

esp_eth_phy_t *esp_eth_phy_new_w6100(const eth_phy_config_t *config)
{
    esp_eth_phy_t *ret = NULL;
    ESP_GOTO_ON_FALSE(config, NULL, err, TAG, "invalid arguments");
    phy_wiznet_t *w6100 = calloc(1, sizeof(phy_wiznet_t));
    ESP_GOTO_ON_FALSE(w6100, NULL, err, TAG, "no mem for PHY instance");
    w6100->addr = config->phy_addr;
    w6100->reset_timeout_ms = config->reset_timeout_ms;
    w6100->reset_gpio_num = config->reset_gpio_num;
    w6100->link_status = ETH_LINK_DOWN;
    w6100->autonego_timeout_ms = config->autonego_timeout_ms;
    /* W6100 PHY status register bit interpretation (inverted from W5500):
     * - speed bit: 1 = 10Mbps, 0 = 100Mbps
     * - duplex bit: 1 = half, 0 = full
     */
    w6100->phy_status_reg = W6100_REG_PHYSR;
    w6100->speed_when_bit_set = ETH_SPEED_10M;
    w6100->speed_when_bit_clear = ETH_SPEED_100M;
    w6100->duplex_when_bit_set = ETH_DUPLEX_HALF;
    w6100->duplex_when_bit_clear = ETH_DUPLEX_FULL;
    /* Table-driven get_mode configuration */
    w6100->opmode_table = w6100_opmode_table;
    w6100->opmode_table_size = sizeof(w6100_opmode_table) / sizeof(w6100_opmode_table[0]);
    w6100->opmode_status_reg = W6100_REG_PHYSR;
    w6100->opmode_shift = 3;  /* opmode is bits [5:3] */
    w6100->opmode_mask = 0x07;
    w6100->is_autoneg_enabled = w6100_is_autoneg_enabled;
    w6100->set_mode = w6100_set_mode;
    w6100->parent.reset = w6100_reset;
    w6100->parent.reset_hw = phy_wiznet_reset_hw;
    w6100->parent.init = phy_wiznet_init;
    w6100->parent.deinit = phy_wiznet_deinit;
    w6100->parent.set_mediator = phy_wiznet_set_mediator;
    w6100->parent.autonego_ctrl = phy_wiznet_autonego_ctrl;
    w6100->parent.get_link = phy_wiznet_get_link;
    w6100->parent.set_link = phy_wiznet_set_link;
    w6100->parent.pwrctl = w6100_pwrctl;
    w6100->parent.get_addr = phy_wiznet_get_addr;
    w6100->parent.set_addr = phy_wiznet_set_addr;
    w6100->parent.advertise_pause_ability = phy_wiznet_advertise_pause_ability;
    w6100->parent.loopback = phy_wiznet_loopback;
    w6100->parent.set_speed = phy_wiznet_set_speed;
    w6100->parent.set_duplex = phy_wiznet_set_duplex;
    w6100->parent.del = phy_wiznet_del;
    return &(w6100->parent);
err:
    return ret;
}
