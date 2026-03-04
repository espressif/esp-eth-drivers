/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include <stdlib.h>
#include <sys/cdefs.h>
#include "esp_eth_phy.h"
#include "esp_eth_phy_w6100.h"
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

static esp_err_t w6100_phy_reset(esp_eth_phy_t *phy)
{
    esp_err_t ret = ESP_OK;
    phy_wiznet_t *w6100 = phy_wiznet_from_parent(phy);
    phy_wiznet_set_link_down(w6100);
    esp_eth_mediator_t *eth = phy_wiznet_get_eth(w6100);
    uint32_t addr = phy_wiznet_get_addr_val(w6100);

    // Unlock PHY configuration
    ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, addr, W6100_REG_PHYLCKR, W6100_PHYLCKR_UNLOCK), err, TAG, "unlock PHY failed");

    // Reset PHY by setting reset bit in PHYCR1
    phycr1_reg_t phycr1;
    /* Cast safe: Wiznet MAC's read_phy_reg only writes 1 byte to the pointer despite uint32_t* parameter */
    ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, addr, W6100_REG_PHYCR1, (uint32_t *) & (phycr1.val)), err, TAG, "read PHYCR1 failed");
    phycr1.reset = 1;
    ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, addr, W6100_REG_PHYCR1, phycr1.val), err, TAG, "write PHYCR1 failed");

    vTaskDelay(pdMS_TO_TICKS(W6100_WAIT_FOR_RESET_MS));

    phycr1.reset = 0;
    ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, addr, W6100_REG_PHYCR1, phycr1.val), err, TAG, "write PHYCR1 failed");

    return ESP_OK;
err:
    return ret;
}

static esp_err_t w6100_is_autoneg_enabled(phy_wiznet_t *wiznet, bool *enabled)
{
    esp_err_t ret = ESP_OK;
    esp_eth_mediator_t *eth = phy_wiznet_get_eth(wiznet);
    uint32_t addr = phy_wiznet_get_addr_val(wiznet);
    physr_reg_t physr;

    /* Cast safe: Wiznet MAC's read_phy_reg only writes 1 byte to the pointer despite uint32_t* parameter */
    ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, addr, W6100_REG_PHYSR, (uint32_t *)&physr.val),
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
    esp_eth_mediator_t *eth = phy_wiznet_get_eth(wiznet);
    uint32_t addr = phy_wiznet_get_addr_val(wiznet);

    // Unlock PHY configuration
    ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, addr, W6100_REG_PHYLCKR, W6100_PHYLCKR_UNLOCK),
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
    ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, addr, W6100_REG_PHYCR0, phycr0), err, TAG, "write PHYCR0 failed");
    return ESP_OK;
err:
    return ret;
}

static esp_err_t w6100_pwrctl(esp_eth_phy_t *phy, bool enable)
{
    esp_err_t ret = ESP_OK;
    phy_wiznet_t *w6100 = phy_wiznet_from_parent(phy);
    esp_eth_mediator_t *eth = phy_wiznet_get_eth(w6100);
    uint32_t addr = phy_wiznet_get_addr_val(w6100);

    // Unlock PHY configuration
    ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, addr, W6100_REG_PHYLCKR, W6100_PHYLCKR_UNLOCK), err, TAG, "unlock PHY failed");

    phycr1_reg_t phycr1;
    /* Cast safe: Wiznet MAC's read_phy_reg only writes 1 byte to the pointer despite uint32_t* parameter */
    ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, addr, W6100_REG_PHYCR1, (uint32_t *) & (phycr1.val)), err, TAG, "read PHYCR1 failed");
    phycr1.pwdn = enable ? 0 : 1;  // 0 = power on, 1 = power down
    ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, addr, W6100_REG_PHYCR1, phycr1.val), err, TAG, "write PHYCR1 failed");

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

    phy_wiznet_config_t wiznet_config = {
        /* Basic config */
        .phy_addr = config->phy_addr,
        .reset_timeout_ms = config->reset_timeout_ms,
        .autonego_timeout_ms = config->autonego_timeout_ms,
        .reset_gpio_num = config->reset_gpio_num,
        /* W6100 PHY status register bit interpretation (inverted from W5500):
         * - speed bit: 1 = 10Mbps, 0 = 100Mbps
         * - duplex bit: 1 = half, 0 = full */
        .phy_status_reg = W6100_REG_PHYSR,
        .speed_when_bit_set = ETH_SPEED_10M,
        .speed_when_bit_clear = ETH_SPEED_100M,
        .duplex_when_bit_set = ETH_DUPLEX_HALF,
        .duplex_when_bit_clear = ETH_DUPLEX_FULL,
        /* Table-driven get_mode configuration */
        .opmode_table = w6100_opmode_table,
        .opmode_table_size = sizeof(w6100_opmode_table) / sizeof(w6100_opmode_table[0]),
        .opmode_status_reg = W6100_REG_PHYSR,
        .opmode_shift = 3,  /* opmode is bits [5:3] */
        .opmode_mask = 0x07,
        /* Chip-specific function pointers */
        .is_autoneg_enabled = w6100_is_autoneg_enabled,
        .set_mode = w6100_set_mode,
        .reset = w6100_phy_reset,
        .pwrctl = w6100_pwrctl,
    };

    return phy_wiznet_new(&wiznet_config);
err:
    return ret;
}
