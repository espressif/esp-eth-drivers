/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include "esp_log.h"
#include "esp_check.h"
#include "esp_idf_version.h"
#include "driver/gpio.h"
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
#include "esp_private/gpio.h"
#include "soc/io_mux_reg.h"
#else
#include "esp_rom_gpio.h"
#endif
#include "esp_rom_sys.h"
#include "wiznet_phy_common.h"

static const char *TAG = "wiznet.phy";

esp_err_t phy_wiznet_set_mediator(esp_eth_phy_t *phy, esp_eth_mediator_t *eth)
{
    esp_err_t ret = ESP_OK;
    ESP_GOTO_ON_FALSE(eth, ESP_ERR_INVALID_ARG, err, TAG, "mediator can't be null");
    phy_wiznet_t *wiznet = __containerof(phy, phy_wiznet_t, parent);
    wiznet->eth = eth;
    return ESP_OK;
err:
    return ret;
}

esp_err_t phy_wiznet_set_link(esp_eth_phy_t *phy, eth_link_t link)
{
    esp_err_t ret = ESP_OK;
    phy_wiznet_t *wiznet = __containerof(phy, phy_wiznet_t, parent);
    esp_eth_mediator_t *eth = wiznet->eth;

    if (wiznet->link_status != link) {
        wiznet->link_status = link;
        // link status changed, immediately report to upper layers
        ESP_GOTO_ON_ERROR(eth->on_state_changed(eth, ETH_STATE_LINK, (void *)wiznet->link_status), err, TAG, "change link failed");
    }
err:
    return ret;
}

esp_err_t phy_wiznet_set_addr(esp_eth_phy_t *phy, uint32_t addr)
{
    phy_wiznet_t *wiznet = __containerof(phy, phy_wiznet_t, parent);
    wiznet->addr = addr;
    return ESP_OK;
}

esp_err_t phy_wiznet_get_addr(esp_eth_phy_t *phy, uint32_t *addr)
{
    esp_err_t ret = ESP_OK;
    ESP_GOTO_ON_FALSE(addr, ESP_ERR_INVALID_ARG, err, TAG, "addr can't be null");
    phy_wiznet_t *wiznet = __containerof(phy, phy_wiznet_t, parent);
    *addr = wiznet->addr;
    return ESP_OK;
err:
    return ret;
}

esp_err_t phy_wiznet_del(esp_eth_phy_t *phy)
{
    phy_wiznet_t *wiznet = __containerof(phy, phy_wiznet_t, parent);
    free(wiznet);
    return ESP_OK;
}

esp_err_t phy_wiznet_advertise_pause_ability(esp_eth_phy_t *phy, uint32_t ability)
{
    // pause ability advertisement is not supported for WIZnet internal PHYs
    return ESP_OK;
}

esp_err_t phy_wiznet_loopback(esp_eth_phy_t *phy, bool enable)
{
    // Loopback is not supported for WIZnet internal PHYs
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t phy_wiznet_reset_hw(esp_eth_phy_t *phy)
{
    phy_wiznet_t *wiznet = __containerof(phy, phy_wiznet_t, parent);
    // set reset_gpio_num to a negative value to skip hardware reset
    if (wiznet->reset_gpio_num >= 0) {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
        gpio_func_sel(wiznet->reset_gpio_num, PIN_FUNC_GPIO);
        gpio_set_level(wiznet->reset_gpio_num, 0);
        gpio_output_enable(wiznet->reset_gpio_num);
#else
        // We need 5.3+ compatibility for w6100 driver, which doesn't exist in
        // IDF 5.x
        esp_rom_gpio_pad_select_gpio(wiznet->reset_gpio_num);
        gpio_set_direction(wiznet->reset_gpio_num, GPIO_MODE_OUTPUT);
        gpio_set_level(wiznet->reset_gpio_num, 0);
#endif
        esp_rom_delay_us(100); // insert min input assert time
        gpio_set_level(wiznet->reset_gpio_num, 1);
    }
    return ESP_OK;
}

/**
 * @brief Common PHY status register bit layout for WIZnet chips
 *
 * Both W5500 and W6100 use the same bit positions for link, speed, and duplex
 * in their PHY status registers (bits 0, 1, 2 respectively).
 * The interpretation of the speed and duplex bits differs between chips.
 */
typedef union {
    struct {
        uint8_t link: 1;   /**< Link status (1=up, 0=down) - same for both chips */
        uint8_t speed: 1;  /**< Speed bit - interpretation varies by chip */
        uint8_t duplex: 1; /**< Duplex bit - interpretation varies by chip */
        uint8_t reserved: 5;
    };
    uint8_t val;
} phy_status_reg_t;

esp_err_t phy_wiznet_get_link(esp_eth_phy_t *phy)
{
    esp_err_t ret = ESP_OK;
    phy_wiznet_t *wiznet = __containerof(phy, phy_wiznet_t, parent);
    esp_eth_mediator_t *eth = wiznet->eth;
    phy_status_reg_t status;

    /* Cast safe: Wiznet MAC's read_phy_reg only writes 1 byte to the pointer despite uint32_t* parameter */
    ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, wiznet->addr, wiznet->phy_status_reg,
                                        (uint32_t *)&status.val), err, TAG, "read PHY status failed");
    eth_link_t link = status.link ? ETH_LINK_UP : ETH_LINK_DOWN;

    /* check if link status changed */
    if (wiznet->link_status != link) {
        /* when link up, read negotiation result */
        if (link == ETH_LINK_UP) {
            eth_speed_t speed = status.speed ? wiznet->speed_when_bit_set : wiznet->speed_when_bit_clear;
            eth_duplex_t duplex = status.duplex ? wiznet->duplex_when_bit_set : wiznet->duplex_when_bit_clear;
            ESP_GOTO_ON_ERROR(eth->on_state_changed(eth, ETH_STATE_SPEED, (void *)speed), err, TAG, "change speed failed");
            ESP_GOTO_ON_ERROR(eth->on_state_changed(eth, ETH_STATE_DUPLEX, (void *)duplex), err, TAG, "change duplex failed");
        }
        ESP_GOTO_ON_ERROR(eth->on_state_changed(eth, ETH_STATE_LINK, (void *)link), err, TAG, "change link failed");
        wiznet->link_status = link;
    }
    return ESP_OK;
err:
    return ret;
}

esp_err_t phy_wiznet_init(esp_eth_phy_t *phy)
{
    esp_err_t ret = ESP_OK;
    phy_wiznet_t *wiznet = __containerof(phy, phy_wiznet_t, parent);

    /* Validate that chip-specific function pointers and table are set */
    ESP_GOTO_ON_FALSE(wiznet->is_autoneg_enabled && wiznet->set_mode,
                      ESP_ERR_INVALID_STATE, err, TAG, "chip-specific PHY ops not configured");
    ESP_GOTO_ON_FALSE(wiznet->opmode_table && wiznet->opmode_table_size > 0,
                      ESP_ERR_INVALID_STATE, err, TAG, "opmode_table not configured");

    /* Power on Ethernet PHY */
    ESP_GOTO_ON_ERROR(phy->pwrctl(phy, true), err, TAG, "power control failed");
    /* Reset Ethernet PHY */
    ESP_GOTO_ON_ERROR(phy->reset(phy), err, TAG, "reset failed");
    return ESP_OK;
err:
    return ret;
}

esp_err_t phy_wiznet_deinit(esp_eth_phy_t *phy)
{
    esp_err_t ret = ESP_OK;
    /* Power off Ethernet PHY */
    ESP_GOTO_ON_ERROR(phy->pwrctl(phy, false), err, TAG, "power control failed");
    return ESP_OK;
err:
    return ret;
}

esp_err_t phy_wiznet_autonego_ctrl(esp_eth_phy_t *phy, eth_phy_autoneg_cmd_t cmd, bool *autonego_en_stat)
{
    esp_err_t ret = ESP_OK;
    phy_wiznet_t *wiznet = __containerof(phy, phy_wiznet_t, parent);
    esp_eth_mediator_t *eth = wiznet->eth;

    bool autoneg_enabled;
    ESP_GOTO_ON_ERROR(wiznet->is_autoneg_enabled(wiznet, &autoneg_enabled), err, TAG, "get autoneg status failed");

    switch (cmd) {
    case ESP_ETH_PHY_AUTONEGO_RESTART:
        ESP_GOTO_ON_FALSE(autoneg_enabled, ESP_ERR_INVALID_STATE, err, TAG, "auto negotiation is disabled");
        /* Restart autoneg by resetting the PHY (reset also sets link_status = ETH_LINK_DOWN) */
        ESP_GOTO_ON_ERROR(phy->reset(phy), err, TAG, "reset PHY failed");
        *autonego_en_stat = true;
        break;

    case ESP_ETH_PHY_AUTONEGO_DIS: {
        /* Read current speed/duplex from status register */
        phy_status_reg_t status;
        /* Cast safe: Wiznet MAC's read_phy_reg only writes 1 byte to the pointer despite uint32_t* parameter */
        ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, wiznet->addr, wiznet->phy_status_reg,
                                            (uint32_t *)&status.val), err, TAG, "read PHY status failed");
        eth_speed_t speed = status.speed ? wiznet->speed_when_bit_set : wiznet->speed_when_bit_clear;
        eth_duplex_t duplex = status.duplex ? wiznet->duplex_when_bit_set : wiznet->duplex_when_bit_clear;
        /* Set fixed mode matching current speed/duplex */
        ESP_GOTO_ON_ERROR(wiznet->set_mode(wiznet, false, speed, duplex),
                          err, TAG, "disable autoneg failed");
        *autonego_en_stat = false;
        break;
    }

    case ESP_ETH_PHY_AUTONEGO_EN:
        ESP_GOTO_ON_ERROR(wiznet->set_mode(wiznet, true, ETH_SPEED_10M, ETH_DUPLEX_HALF),
                          err, TAG, "enable autoneg failed");
        *autonego_en_stat = true;
        break;

    case ESP_ETH_PHY_AUTONEGO_G_STAT:
        *autonego_en_stat = autoneg_enabled;
        break;

    default:
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
err:
    return ret;
}

esp_err_t phy_wiznet_set_speed(esp_eth_phy_t *phy, eth_speed_t speed)
{
    esp_err_t ret = ESP_OK;
    phy_wiznet_t *wiznet = __containerof(phy, phy_wiznet_t, parent);

    /* Since the link is going to be reconfigured, consider it down to be status updated once the driver re-started */
    wiznet->link_status = ETH_LINK_DOWN;

    /* Get current duplex (get_mode reads status bits in autoneg mode) */
    eth_duplex_t duplex;
    bool autoneg;
    eth_speed_t current_speed;

    ESP_GOTO_ON_ERROR(phy_wiznet_get_mode(wiznet, &autoneg, &current_speed, &duplex),
                      err, TAG, "get mode failed");

    /* Set fixed mode with new speed and current duplex */
    ESP_GOTO_ON_ERROR(wiznet->set_mode(wiznet, false, speed, duplex), err, TAG, "set mode failed");

    /* Reset PHY for configuration to take effect */
    ESP_GOTO_ON_ERROR(phy->reset(phy), err, TAG, "reset PHY failed");

    return ESP_OK;
err:
    return ret;
}

esp_err_t phy_wiznet_set_duplex(esp_eth_phy_t *phy, eth_duplex_t duplex)
{
    esp_err_t ret = ESP_OK;
    phy_wiznet_t *wiznet = __containerof(phy, phy_wiznet_t, parent);

    /* Since the link is going to be reconfigured, consider it down to be status updated once the driver re-started */
    wiznet->link_status = ETH_LINK_DOWN;

    /* Get current speed (get_mode reads status bits in autoneg mode) */
    eth_speed_t speed;
    bool autoneg;
    eth_duplex_t current_duplex;

    ESP_GOTO_ON_ERROR(phy_wiznet_get_mode(wiznet, &autoneg, &speed, &current_duplex),
                      err, TAG, "get mode failed");

    /* Set fixed mode with current speed and new duplex */
    ESP_GOTO_ON_ERROR(wiznet->set_mode(wiznet, false, speed, duplex), err, TAG, "set mode failed");

    /* Reset PHY for configuration to take effect */
    ESP_GOTO_ON_ERROR(phy->reset(phy), err, TAG, "reset PHY failed");

    return ESP_OK;
err:
    return ret;
}

esp_err_t phy_wiznet_get_mode(phy_wiznet_t *wiznet, bool *autoneg, eth_speed_t *speed, eth_duplex_t *duplex)
{
    esp_err_t ret = ESP_OK;
    esp_eth_mediator_t *eth = wiznet->eth;
    phy_status_reg_t status;

    /* Cast safe: Wiznet MAC's read_phy_reg only writes 1 byte to the pointer despite uint32_t* parameter */
    ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, wiznet->addr, wiznet->opmode_status_reg, (uint32_t *)&status.val),
                      err, TAG, "read opmode status failed");

    uint8_t opmode = (status.val >> wiznet->opmode_shift) & wiznet->opmode_mask;

    /* Search for opmode in table */
    for (int i = 0; i < wiznet->opmode_table_size; i++) {
        if (wiznet->opmode_table[i].opmode == opmode) {
            *autoneg = false;
            *speed = wiznet->opmode_table[i].speed;
            *duplex = wiznet->opmode_table[i].duplex;
            return ESP_OK;
        }
    }

    /* Not found in fixed-mode table - must be autoneg, read from status bits */
    *autoneg = true;
    *speed = status.speed ? wiznet->speed_when_bit_set : wiznet->speed_when_bit_clear;
    *duplex = status.duplex ? wiznet->duplex_when_bit_set : wiznet->duplex_when_bit_clear;
    return ESP_OK;
err:
    return ret;
}
