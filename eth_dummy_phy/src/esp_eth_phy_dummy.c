/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include <stdlib.h>
#include <sys/cdefs.h>
#include "esp_log.h"
#include "esp_check.h"
#include "esp_eth_driver.h"
#include "driver/gpio.h"

typedef struct {
    esp_eth_phy_t parent;
    esp_eth_mediator_t *eth;
    int reset_gpio_num;
    eth_link_t link;
    eth_speed_t speed;
    eth_duplex_t duplex;
} phy_dummy_t;

static const char *TAG = "dummy_phy";

static esp_err_t get_link(esp_eth_phy_t *phy)
{
    esp_err_t ret = ESP_OK;
    phy_dummy_t *dummy_phy = __containerof(phy, phy_dummy_t, parent);
    esp_eth_mediator_t *eth = dummy_phy->eth;

    // TODO: check if EMAC is working (e.g. External CLK is present)

    if (dummy_phy->link == ETH_LINK_DOWN) {
        dummy_phy->link = ETH_LINK_UP;

        ESP_GOTO_ON_ERROR(eth->on_state_changed(eth, ETH_STATE_SPEED, (void *)dummy_phy->speed), err, TAG, "change speed failed");
        ESP_GOTO_ON_ERROR(eth->on_state_changed(eth, ETH_STATE_DUPLEX, (void *)dummy_phy->duplex), err, TAG, "change duplex failed");

        bool peer_pause_ability = false; // always false
        ESP_GOTO_ON_ERROR(eth->on_state_changed(eth, ETH_STATE_PAUSE, (void *)peer_pause_ability), err, TAG, "change pause ability failed");

        ESP_GOTO_ON_ERROR(eth->on_state_changed(eth, ETH_STATE_LINK, (void *)dummy_phy->link), err, TAG, "change link failed");
    }

err:
    return ret;
}

static esp_err_t set_link(esp_eth_phy_t *phy, eth_link_t link)
{
    esp_err_t ret = ESP_OK;
    phy_dummy_t *dummy_phy = __containerof(phy, phy_dummy_t, parent);
    esp_eth_mediator_t *eth   = dummy_phy->eth;

    if (dummy_phy->link != link) {
        dummy_phy->link = link;
        // link status changed, inmiedately report to upper layers
        ESP_GOTO_ON_ERROR(eth->on_state_changed(eth, ETH_STATE_LINK, (void *)dummy_phy->link), err, TAG, "change link failed");
    }
err:
    return ret;
}

static esp_err_t set_mediator(esp_eth_phy_t *phy, esp_eth_mediator_t *eth)
{
    esp_err_t ret = ESP_OK;
    ESP_GOTO_ON_FALSE(eth, ESP_ERR_INVALID_ARG, err, TAG, "mediator can't be null");
    phy_dummy_t *dummy_phy = __containerof(phy, phy_dummy_t, parent);
    dummy_phy->eth = eth;
err:
    return ret;
}

static esp_err_t reset_hw(esp_eth_phy_t *phy)
{
    phy_dummy_t *dummy_phy = __containerof(phy, phy_dummy_t, parent);

    if (dummy_phy->reset_gpio_num >= 0) {
        esp_rom_gpio_pad_select_gpio(dummy_phy->reset_gpio_num);
        gpio_set_direction(dummy_phy->reset_gpio_num, GPIO_MODE_OUTPUT);
        gpio_set_level(dummy_phy->reset_gpio_num, 0);
        esp_rom_delay_us(100);
        gpio_set_level(dummy_phy->reset_gpio_num, 1);
    }
    return ESP_OK;
}

static esp_err_t autonego_ctrl(esp_eth_phy_t *phy, eth_phy_autoneg_cmd_t cmd, bool *autonego_en_stat)
{
    switch (cmd) {
    case ESP_ETH_PHY_AUTONEGO_RESTART:
    // Fallthrough
    case ESP_ETH_PHY_AUTONEGO_EN:
    // Fallthrough
    case ESP_ETH_PHY_AUTONEGO_DIS:
        return ESP_ERR_NOT_SUPPORTED; // no autonegotiation operations are supported
    case ESP_ETH_PHY_AUTONEGO_G_STAT:
        // report that auto-negotiation is not supported
        *autonego_en_stat = false;
        break;
    default:
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}

static esp_err_t set_speed(esp_eth_phy_t *phy, eth_speed_t speed)
{
    phy_dummy_t *dummy_phy = __containerof(phy, phy_dummy_t, parent);
    dummy_phy->link = ETH_LINK_DOWN;
    dummy_phy->speed = speed;
    get_link(phy); // propagate the change to higher layer
    return ESP_OK;
}

static esp_err_t set_duplex(esp_eth_phy_t *phy, eth_duplex_t duplex)
{
    phy_dummy_t *dummy_phy = __containerof(phy, phy_dummy_t, parent);
    dummy_phy->link = ETH_LINK_DOWN;
    dummy_phy->duplex = duplex;
    get_link(phy); // propagate the change to higher layer
    return ESP_OK;
}

static esp_err_t do_nothing(esp_eth_phy_t *phy)
{
    return ESP_OK;
}

static esp_err_t do_nothing_arg_bool(esp_eth_phy_t *phy, bool option)
{
    return ESP_OK;
}

static esp_err_t do_nothing_arg_uint32(esp_eth_phy_t *phy, uint32_t option)
{
    return ESP_OK;
}

static esp_err_t do_nothing_arg_uint32p(esp_eth_phy_t *phy, uint32_t *option)
{
    return ESP_OK;
}

static esp_err_t del(esp_eth_phy_t *phy)
{
    free(phy);
    return ESP_OK;
}

esp_eth_phy_t *esp_eth_phy_new_dummy(const eth_phy_config_t *config)
{
    esp_eth_phy_t *ret = NULL;
    phy_dummy_t *dummy_phy = calloc(1, sizeof(phy_dummy_t));
    ESP_GOTO_ON_FALSE(dummy_phy, NULL, err, TAG, "calloc dummy_phy failed");

    dummy_phy->link = ETH_LINK_DOWN;
    // default link configuration
    dummy_phy->speed = ETH_SPEED_100M;
    dummy_phy->duplex = ETH_DUPLEX_FULL;

    dummy_phy->reset_gpio_num = config->reset_gpio_num;

    dummy_phy->parent.reset = do_nothing;
    dummy_phy->parent.reset_hw = reset_hw;
    dummy_phy->parent.init = do_nothing;
    dummy_phy->parent.deinit = do_nothing;
    dummy_phy->parent.set_mediator = set_mediator;
    dummy_phy->parent.autonego_ctrl = autonego_ctrl;
    dummy_phy->parent.pwrctl = do_nothing_arg_bool;
    dummy_phy->parent.get_addr = do_nothing_arg_uint32p;
    dummy_phy->parent.set_addr = do_nothing_arg_uint32;
    dummy_phy->parent.advertise_pause_ability = do_nothing_arg_uint32;
    dummy_phy->parent.loopback = do_nothing_arg_bool;
    dummy_phy->parent.set_speed = set_speed;
    dummy_phy->parent.set_duplex = set_duplex;
    dummy_phy->parent.del = del;
    dummy_phy->parent.get_link = get_link;
    dummy_phy->parent.set_link = set_link;
    dummy_phy->parent.custom_ioctl = NULL;

    return &dummy_phy->parent;
err:
    if (dummy_phy != NULL) {
        free(dummy_phy);
    }
    return ret;
}
