/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include <stdlib.h>
#include <sys/cdefs.h>
#include "esp_eth_phy.h"
#include "esp_eth_phy_w5500.h"
#include "esp_log.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "w5500.h"
#include "wiznet_phy_common.h"

#define W5500_WAIT_FOR_RESET_MS (10) // wait for W5500 internal PLL to be Locked after reset assert

static const char *TAG = "w5500.phy";

/***************Vendor Specific Register***************/
/**
 * @brief PHYCFGR(PHY Configuration Register)
 *
 */
typedef union {
    struct {
        uint8_t link: 1;   /*!< Link status */
        uint8_t speed: 1;  /*!< Speed status */
        uint8_t duplex: 1; /*!< Duplex status */
        uint8_t opmode: 3; /*!< Operation mode */
        uint8_t opsel: 1;  /*!< Operation select */
        uint8_t reset: 1;  /*!< Reset, when this bit is '0', PHY will get reset */
    };
    uint8_t val;
} phycfg_reg_t;

typedef enum {
    W5500_OP_MODE_10BT_HALF_AUTO_DIS,
    W5500_OP_MODE_10BT_FULL_AUTO_DIS,
    W5500_OP_MODE_100BT_HALF_AUTO_DIS,
    W5500_OP_MODE_100BT_FULL_AUTO_DIS,
    W5500_OP_MODE_100BT_HALF_AUTO_EN,
    W5500_OP_MODE_NOT_USED,
    W5500_OP_MODE_PWR_DOWN,
    W5500_OP_MODE_ALL_CAPABLE,
} phy_w5500_op_mode_e;

/* Opmode table for get_mode lookup - only fixed modes, autoneg modes fall through */
static const wiznet_opmode_entry_t w5500_opmode_table[] = {
    { W5500_OP_MODE_10BT_HALF_AUTO_DIS,  ETH_SPEED_10M,  ETH_DUPLEX_HALF },
    { W5500_OP_MODE_10BT_FULL_AUTO_DIS,  ETH_SPEED_10M,  ETH_DUPLEX_FULL },
    { W5500_OP_MODE_100BT_HALF_AUTO_DIS, ETH_SPEED_100M, ETH_DUPLEX_HALF },
    { W5500_OP_MODE_100BT_FULL_AUTO_DIS, ETH_SPEED_100M, ETH_DUPLEX_FULL },
};

static esp_err_t w5500_phy_reset(esp_eth_phy_t *phy)
{
    esp_err_t ret = ESP_OK;
    phy_wiznet_t *w5500 = phy_wiznet_from_parent(phy);
    phy_wiznet_set_link_down(w5500);
    esp_eth_mediator_t *eth = phy_wiznet_get_eth(w5500);
    uint32_t addr = phy_wiznet_get_addr_val(w5500);
    phycfg_reg_t phycfg;

    /* Cast safe: Wiznet MAC's read_phy_reg only writes 1 byte to the pointer despite uint32_t* parameter */
    ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, addr, W5500_REG_PHYCFGR, (uint32_t *) & (phycfg.val)), err, TAG, "read PHYCFG failed");
    phycfg.reset = 0; // set to '0' will reset internal PHY
    ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, addr, W5500_REG_PHYCFGR, phycfg.val), err, TAG, "write PHYCFG failed");
    vTaskDelay(pdMS_TO_TICKS(W5500_WAIT_FOR_RESET_MS));
    phycfg.reset = 1; // set to '1' after reset
    ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, addr, W5500_REG_PHYCFGR, phycfg.val), err, TAG, "write PHYCFG failed");
    return ESP_OK;
err:
    return ret;
}

static esp_err_t w5500_is_autoneg_enabled(phy_wiznet_t *wiznet, bool *enabled)
{
    esp_err_t ret = ESP_OK;
    esp_eth_mediator_t *eth = phy_wiznet_get_eth(wiznet);
    uint32_t addr = phy_wiznet_get_addr_val(wiznet);
    phycfg_reg_t phycfg;

    /* Cast safe: Wiznet MAC's read_phy_reg only writes 1 byte to the pointer despite uint32_t* parameter */
    ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, addr, W5500_REG_PHYCFGR, (uint32_t *)&phycfg.val),
                      err, TAG, "read PHYCFG failed");
    *enabled = (phycfg.opmode == W5500_OP_MODE_ALL_CAPABLE || phycfg.opmode == W5500_OP_MODE_100BT_HALF_AUTO_EN);
    return ESP_OK;
err:
    return ret;
}

static esp_err_t w5500_set_mode(phy_wiznet_t *wiznet, bool autoneg, eth_speed_t speed, eth_duplex_t duplex)
{
    esp_err_t ret = ESP_OK;
    esp_eth_mediator_t *eth = phy_wiznet_get_eth(wiznet);
    uint32_t addr = phy_wiznet_get_addr_val(wiznet);
    phycfg_reg_t phycfg;

    /* Cast safe: Wiznet MAC's read_phy_reg only writes 1 byte to the pointer despite uint32_t* parameter */
    ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, addr, W5500_REG_PHYCFGR, (uint32_t *)&phycfg.val),
                      err, TAG, "read PHYCFG failed");

    if (autoneg) {
        phycfg.opmode = W5500_OP_MODE_ALL_CAPABLE;
    } else {
        /* Set fixed mode based on speed/duplex */
        if (duplex == ETH_DUPLEX_FULL) {
            phycfg.opmode = (speed == ETH_SPEED_100M) ? W5500_OP_MODE_100BT_FULL_AUTO_DIS : W5500_OP_MODE_10BT_FULL_AUTO_DIS;
        } else {
            phycfg.opmode = (speed == ETH_SPEED_100M) ? W5500_OP_MODE_100BT_HALF_AUTO_DIS : W5500_OP_MODE_10BT_HALF_AUTO_DIS;
        }
    }

    phycfg.opsel = 1;  // PHY working mode configured by register
    ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, addr, W5500_REG_PHYCFGR, phycfg.val), err, TAG, "write PHYCFG failed");
    return ESP_OK;
err:
    return ret;
}

static esp_err_t w5500_pwrctl(esp_eth_phy_t *phy, bool enable)
{
    // power control is not supported for W5500 internal PHY
    return ESP_OK;
}

esp_eth_phy_t *esp_eth_phy_new_w5500(const eth_phy_config_t *config)
{
    esp_eth_phy_t *ret = NULL;
    ESP_GOTO_ON_FALSE(config, NULL, err, TAG, "invalid arguments");

    phy_wiznet_config_t wiznet_config = {
        /* Basic config */
        .phy_addr = config->phy_addr,
        .reset_timeout_ms = config->reset_timeout_ms,
        .autonego_timeout_ms = config->autonego_timeout_ms,
        .reset_gpio_num = config->reset_gpio_num,
        /* W5500 PHY status register bit interpretation:
         * - speed bit: 1 = 100Mbps, 0 = 10Mbps
         * - duplex bit: 1 = full, 0 = half */
        .phy_status_reg = W5500_REG_PHYCFGR,
        .speed_when_bit_set = ETH_SPEED_100M,
        .speed_when_bit_clear = ETH_SPEED_10M,
        .duplex_when_bit_set = ETH_DUPLEX_FULL,
        .duplex_when_bit_clear = ETH_DUPLEX_HALF,
        /* Table-driven get_mode configuration */
        .opmode_table = w5500_opmode_table,
        .opmode_table_size = sizeof(w5500_opmode_table) / sizeof(w5500_opmode_table[0]),
        .opmode_status_reg = W5500_REG_PHYCFGR,
        .opmode_shift = 3,  /* opmode is bits [5:3] */
        .opmode_mask = 0x07,
        /* Chip-specific function pointers */
        .is_autoneg_enabled = w5500_is_autoneg_enabled,
        .set_mode = w5500_set_mode,
        .reset = w5500_phy_reset,
        .pwrctl = w5500_pwrctl,
    };

    return phy_wiznet_new(&wiznet_config);
err:
    return ret;
}
