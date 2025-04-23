/*
 * SPDX-FileCopyrightText: 2019-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_rom_gpio.h"
#include "esp_rom_sys.h"

#include "esp_eth_phy_ksz8863.h"
#include "ksz8863.h"

static const char *TAG = "ksz8863_phy";

/***************Vendor Specific Register***************/

/*************************/

typedef enum {
    KSZ8863_MAC_MAC_MODE,
    KSZ8863_PORT_PHY_MODE
} ksz8863_driver_mode_t;

typedef struct {
    esp_eth_phy_t parent;
    esp_eth_mediator_t *eth;
    int addr;
    uint32_t reset_timeout_ms;
    uint32_t autonego_timeout_ms;
    eth_link_t link_status;
    ksz8863_driver_mode_t driver_mode;
    uint8_t port_reg_offset;
} phy_ksz8863_t;

static esp_err_t ksz8863_update_link_duplex_speed(phy_ksz8863_t *ksz8863)
{
    esp_err_t ret = ESP_OK;
    esp_eth_mediator_t *eth = ksz8863->eth;
    eth_link_t link;
    ksz8863_psr0_reg_t pstat0;

    if (ksz8863->driver_mode == KSZ8863_MAC_MAC_MODE) {
        // MAC-MAC connection at Port 3 does not have link indication so check if Switch is started instead
        ksz8863_chipid1_reg_t chipid1;
        ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, ksz8863->addr, KSZ8863_CHIPID1_REG_ADDR,
                                            &(chipid1.val)), err, TAG, "read Start Switch failed");
        link = chipid1.start_switch ? ETH_LINK_UP : ETH_LINK_DOWN;

    } else {
        ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, ksz8863->addr, KSZ8863_PSR0_BASE_ADDR + ksz8863->port_reg_offset,
                                            &(pstat0.val)), err, TAG, "read Port Status 0 failed");
        link = pstat0.link_good ? ETH_LINK_UP : ETH_LINK_DOWN;
    }

    /* check if link status changed */
    if (ksz8863->link_status != link) {
        eth_speed_t speed = ETH_SPEED_10M;
        eth_duplex_t duplex = ETH_DUPLEX_HALF;
        uint32_t peer_pause_ability = false;
        /* when link up, read port link status  */
        if (link == ETH_LINK_UP) {
            ksz8863_psr1_reg_t pstat1;
            ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, ksz8863->addr, KSZ8863_PSR1_BASE_ADDR + ksz8863->port_reg_offset,
                                                &(pstat1.val)), err, TAG, "read Port Status 1 failed");
            speed = pstat1.speed;
            duplex = pstat1.duplex;

            ESP_GOTO_ON_ERROR(eth->on_state_changed(eth, ETH_STATE_SPEED, (void *)speed), err, TAG, "change speed failed");
            ESP_GOTO_ON_ERROR(eth->on_state_changed(eth, ETH_STATE_DUPLEX, (void *)duplex), err, TAG, "change duplex failed");

            if (ksz8863->driver_mode == KSZ8863_MAC_MAC_MODE) {
                ksz8863_gcr4_reg_t gcr4;
                ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, ksz8863->addr, KSZ8863_GCR4_ADDR, &(gcr4.val)), err, TAG, "read GCR 4 failed");
                /* if we're in duplex mode, and switch port (P3) has the flow control ability enabled */
                if (duplex == ETH_DUPLEX_FULL && gcr4.switch_flow_ctrl_en) {
                    peer_pause_ability = 1;
                } else {
                    peer_pause_ability = 0;
                }
            } else {
                /* if we're in duplex mode, and peer has the flow control ability */
                if (duplex == ETH_DUPLEX_FULL && pstat0.partner_flow_control) {
                    peer_pause_ability = 1;
                } else {
                    peer_pause_ability = 0;
                }
            }
            ESP_GOTO_ON_ERROR(eth->on_state_changed(eth, ETH_STATE_PAUSE, (void *)peer_pause_ability), err, TAG, "change pause ability failed");
        }
        ESP_GOTO_ON_ERROR(eth->on_state_changed(eth, ETH_STATE_LINK, (void *)link), err, TAG, "change link failed");
        ksz8863->link_status = link;
    }
    return ESP_OK;
err:
    return ret;
}

static esp_err_t ksz8863_set_mediator(esp_eth_phy_t *phy, esp_eth_mediator_t *eth)
{
    esp_err_t ret = ESP_OK;
    ESP_GOTO_ON_FALSE(eth, ESP_ERR_INVALID_ARG, err, TAG, "can't set mediator to null");
    phy_ksz8863_t *ksz8863 = __containerof(phy, phy_ksz8863_t, parent);
    ksz8863->eth = eth;
    return ESP_OK;
err:
    return ret;
}

static esp_err_t ksz8863_get_link(esp_eth_phy_t *phy)
{
    esp_err_t ret = ESP_OK;
    phy_ksz8863_t *ksz8863 = __containerof(phy, phy_ksz8863_t, parent);
    /* Update information about link, speed, duplex */
    ESP_GOTO_ON_ERROR(ksz8863_update_link_duplex_speed(ksz8863), err, TAG, "update link duplex speed failed");
    return ESP_OK;
err:
    return ret;
}

static esp_err_t ksz8863_set_link(esp_eth_phy_t *phy, eth_link_t link)
{
    esp_err_t ret = ESP_OK;
    phy_ksz8863_t *ksz8863 = __containerof(phy, phy_ksz8863_t, parent);
    esp_eth_mediator_t *eth = ksz8863->eth;

    if (ksz8863->link_status != link) {
        ksz8863->link_status = link;
        // link status changed, inmiedately report to upper layers
        ESP_GOTO_ON_ERROR(eth->on_state_changed(eth, ETH_STATE_LINK, (void *)ksz8863->link_status), err, TAG, "change link failed");
    }
err:
    return ret;
}

static esp_err_t ksz8863_reset_sw(esp_eth_phy_t *phy)
{
    // reset needs to be performed externally since multiple instances of PHY driver exist and so they could reset the chip multiple times
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t ksz8863_reset_hw(esp_eth_phy_t *phy)
{
    // reset needs to be performed externally since multiple instances of PHY driver exist and so they could reset the chip multiple times
    return ESP_ERR_NOT_SUPPORTED;
}

/**
 * @note This function is responsible for restarting a new auto-negotiation,
 *       the result of negotiation won't be reflected to upper layers.
 *       Instead, the negotiation result is fetched by status link timer, see `ksz8863_get_link()`
 */
static esp_err_t ksz8863_autonego_ctrl(esp_eth_phy_t *phy, eth_phy_autoneg_cmd_t cmd, bool *autonego_en_stat)
{
    esp_err_t ret = ESP_OK;
    phy_ksz8863_t *ksz8863 = __containerof(phy, phy_ksz8863_t, parent);
    esp_eth_mediator_t *eth = ksz8863->eth;

    // Autonegotiation is not available in MAC-MAC mode
    if (ksz8863->driver_mode == KSZ8863_MAC_MAC_MODE) {
        switch (cmd) {
        case ESP_ETH_PHY_AUTONEGO_RESTART:
        /* Fallthrough */
        case ESP_ETH_PHY_AUTONEGO_EN:
            return ESP_ERR_NOT_SUPPORTED;
        case ESP_ETH_PHY_AUTONEGO_DIS:
        /* Fallthrough */
        case ESP_ETH_PHY_AUTONEGO_G_STAT:
            *autonego_en_stat = false;
            break;
        default:
            return ESP_ERR_INVALID_ARG;
        }
        return ESP_OK;
    }

    // Port mode
    ksz8863_pcr12_reg_t pcr12;
    ksz8863_pcr13_reg_t pcr13;
    ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, ksz8863->addr, KSZ8863_PCR12_BASE_ADDR + ksz8863->port_reg_offset,
                                        &(pcr12.val)), err, TAG, "read PCR12 failed");
    ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, ksz8863->addr, KSZ8863_PCR13_BASE_ADDR + ksz8863->port_reg_offset,
                                        &(pcr13.val)), err, TAG, "read PCR13 failed");

    switch (cmd) {
    case ESP_ETH_PHY_AUTONEGO_RESTART:
        ESP_GOTO_ON_FALSE(pcr12.en_auto_nego, ESP_ERR_INVALID_STATE, err, TAG, "auto negotiation is disabled");
        /* in case any link status has changed, let's assume we're in link down status */
        ksz8863->link_status = ETH_LINK_DOWN;

        pcr13.restart_auto_nego = 1; /* Restart Auto Negotiation */

        ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, ksz8863->addr, KSZ8863_PCR13_BASE_ADDR + ksz8863->port_reg_offset,
                                             pcr13.val), err, TAG, "write PCR13 failed");
        /* Wait for auto negotiation complete */
        ksz8863_psr0_reg_t pstat0;
        uint32_t to = 0;
        for (to = 0; to < ksz8863->autonego_timeout_ms / 100; to++) {
            vTaskDelay(pdMS_TO_TICKS(100));
            ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, ksz8863->addr, KSZ8863_PSR0_BASE_ADDR + ksz8863->port_reg_offset,
                                                &(pstat0.val)), err, TAG, "read Port Status 0 failed");
            if (pstat0.auto_nego_done) {
                break;
            }
        }
        if ((to >= ksz8863->autonego_timeout_ms / 100) && (ksz8863->link_status == ETH_LINK_UP)) {
            ESP_LOGW(TAG, "auto negotiation timeout");
        }
        break;
    case ESP_ETH_PHY_AUTONEGO_DIS:
        if (pcr12.en_auto_nego == 1) {
            pcr12.en_auto_nego = 0;     /* Disable Auto Negotiation */
            ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, ksz8863->addr, KSZ8863_PCR12_BASE_ADDR + ksz8863->port_reg_offset,
                                                 pcr12.val), err, TAG, "write PCR12 failed");
            /* read configuration back */
            ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, ksz8863->addr, KSZ8863_PCR12_BASE_ADDR + ksz8863->port_reg_offset,
                                                &(pcr12.val)), err, TAG, "read PCR12 failed");
            ESP_GOTO_ON_FALSE(pcr12.en_auto_nego == 0, ESP_FAIL, err, TAG, "disable auto-negotiation failed");
        }
        break;
    case ESP_ETH_PHY_AUTONEGO_EN:
        if (pcr12.en_auto_nego == 0) {
            pcr12.en_auto_nego = 1;     /* Enable Auto Negotiation */
            ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, ksz8863->addr, KSZ8863_PCR12_BASE_ADDR + ksz8863->port_reg_offset,
                                                 pcr12.val), err, TAG, "write PCR12 failed");
            /* read configuration back */
            ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, ksz8863->addr, KSZ8863_PCR12_BASE_ADDR + ksz8863->port_reg_offset,
                                                &(pcr12.val)), err, TAG, "read PCR12 failed");
            ESP_GOTO_ON_FALSE(pcr12.en_auto_nego == 1, ESP_FAIL, err, TAG, "enable auto-negotiation failed");
        }
        break;
    case ESP_ETH_PHY_AUTONEGO_G_STAT:
        /* do nothing autonego_en_stat is set at the function end */
        break;
    default:
        return ESP_ERR_INVALID_ARG;
    }

    *autonego_en_stat = pcr12.en_auto_nego;
    return ESP_OK;
err:
    return ret;
}

static esp_err_t ksz8863_pwrctl(esp_eth_phy_t *phy, bool enable)
{
    esp_err_t ret = ESP_OK;
    phy_ksz8863_t *ksz8863 = __containerof(phy, phy_ksz8863_t, parent);
    esp_eth_mediator_t *eth = ksz8863->eth;

    if (ksz8863->driver_mode == KSZ8863_MAC_MAC_MODE) {
        return ESP_OK;
    }

    ksz8863_pcr13_reg_t pcr13;
    ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, ksz8863->addr, KSZ8863_PCR13_BASE_ADDR + ksz8863->port_reg_offset, &(pcr13.val)),
                      err, TAG, "read PCR13 failed");
    if (!enable) {
        /* Enable IEEE Power Down Mode */
        pcr13.power_down = 1;
    } else {
        /* Disable IEEE Power Down Mode */
        pcr13.power_down = 0;
    }
    ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, ksz8863->addr, KSZ8863_PCR13_BASE_ADDR + ksz8863->port_reg_offset, pcr13.val),
                      err, TAG, "write PCR13 failed");
    if (!enable) {
        ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, ksz8863->addr, KSZ8863_PCR13_BASE_ADDR + ksz8863->port_reg_offset, &(pcr13.val)),
                          err, TAG, "read PCR13 failed");
        ESP_GOTO_ON_FALSE(pcr13.power_down == 1, ESP_FAIL, err, TAG, "power down failed");
    } else {
        /* wait for power up complete */
        uint32_t to = 0;
        for (to = 0; to < ksz8863->reset_timeout_ms / 10; to++) {
            vTaskDelay(pdMS_TO_TICKS(10));
            ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, ksz8863->addr, KSZ8863_PCR13_BASE_ADDR + ksz8863->port_reg_offset, &(pcr13.val)),
                              err, TAG, "read PCR13 failed");
            if (pcr13.power_down == 0) {
                break;
            }
        }
        ESP_GOTO_ON_FALSE(to < ksz8863->reset_timeout_ms / 10, ESP_FAIL, err, TAG, "power up timeout");
    }
    return ESP_OK;
err:
    return ret;
}

static esp_err_t ksz8863_set_addr(esp_eth_phy_t *phy, uint32_t addr)
{
    phy_ksz8863_t *ksz8863 = __containerof(phy, phy_ksz8863_t, parent);
    ksz8863->addr = addr; // TODO: probably make it not available for KSZ8863
    return ESP_OK;
}

static esp_err_t ksz8863_get_addr(esp_eth_phy_t *phy, uint32_t *addr)
{
    esp_err_t ret = ESP_OK;
    ESP_GOTO_ON_FALSE(addr, ESP_ERR_INVALID_ARG, err, TAG, "addr can't be null");
    phy_ksz8863_t *ksz8863 = __containerof(phy, phy_ksz8863_t, parent);
    *addr = ksz8863->addr;
    return ESP_OK;
err:
    return ret;
}

static esp_err_t ksz8863_del(esp_eth_phy_t *phy)
{
    phy_ksz8863_t *ksz8863 = __containerof(phy, phy_ksz8863_t, parent);
    free(ksz8863);

    return ESP_OK;
}

static esp_err_t ksz8863_advertise_pause_ability(esp_eth_phy_t *phy, uint32_t ability)
{
    esp_err_t ret = ESP_OK;
    phy_ksz8863_t *ksz8863 = __containerof(phy, phy_ksz8863_t, parent);
    esp_eth_mediator_t *eth = ksz8863->eth;

    if (ksz8863->driver_mode == KSZ8863_MAC_MAC_MODE) {
        // Set PAUSE function ability at Switch (P3)
        ksz8863_gcr4_reg_t gcr4;
        ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, ksz8863->addr, KSZ8863_GCR4_ADDR, &(gcr4.val)), err, TAG, "read GCR4 failed");
        if (ability) {
            gcr4.switch_flow_ctrl_en = 1;
        } else {
            gcr4.switch_flow_ctrl_en = 0;
        }
        ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, ksz8863->addr, KSZ8863_GCR4_ADDR, gcr4.val), err, TAG, "write GCR4 failed");
    } else { // Port mode
        // Advertise PAUSE function ability to peer
        ksz8863_pcr12_reg_t pcr12;
        ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, ksz8863->addr, KSZ8863_PCR12_BASE_ADDR + ksz8863->port_reg_offset,
                                            &(pcr12.val)), err, TAG, "read PCR12 failed");
        if (ability) {
            pcr12.advertise_flow_ctrl = 1;
        } else {
            pcr12.advertise_flow_ctrl = 0;
        }
        ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, ksz8863->addr, KSZ8863_PCR12_BASE_ADDR + ksz8863->port_reg_offset,
                                             pcr12.val), err, TAG, "write PCR12 failed");
    }
    return ESP_OK;
err:
    return ret;
}

static esp_err_t ksz8863_loopback(esp_eth_phy_t *phy, bool enable)
{
    esp_err_t ret = ESP_OK;
    phy_ksz8863_t *ksz8863 = __containerof(phy, phy_ksz8863_t, parent);
    esp_eth_mediator_t *eth = ksz8863->eth;
    if (ksz8863->driver_mode == KSZ8863_MAC_MAC_MODE) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    /* Set Loopback function */
    ksz8863_pcr13_reg_t pcr13;
    ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, ksz8863->addr, KSZ8863_PCR13_BASE_ADDR + ksz8863->port_reg_offset,
                                        &(pcr13.val)), err, TAG, "read PCR13 failed");
    if (enable) {
        pcr13.loopback = 1;
    } else {
        pcr13.loopback = 0;
    }
    ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, ksz8863->addr, KSZ8863_PCR13_BASE_ADDR + ksz8863->port_reg_offset,
                                         pcr13.val), err, TAG, "write PCR13 failed");
    return ESP_OK;
err:
    return ret;
}

static esp_err_t ksz8863_set_speed(esp_eth_phy_t *phy, eth_speed_t speed)
{
    esp_err_t ret = ESP_OK;
    phy_ksz8863_t *ksz8863 = __containerof(phy, phy_ksz8863_t, parent);
    esp_eth_mediator_t *eth = ksz8863->eth;

    if (ksz8863->link_status == ETH_LINK_UP) {
        /* Since the link is going to be reconfigured, consider it down for a while */
        ksz8863->link_status = ETH_LINK_DOWN;
        /* Indicate to upper stream apps the link is cosidered down */
        ESP_GOTO_ON_ERROR(eth->on_state_changed(eth, ETH_STATE_LINK, (void *)ksz8863->link_status), err, TAG, "change link failed");
    }
    /* Set speed */
    if (ksz8863->driver_mode == KSZ8863_MAC_MAC_MODE) {
        ksz8863_gcr4_reg_t gcr4;
        ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, ksz8863->addr, KSZ8863_GCR4_ADDR, &(gcr4.val)), err, TAG, "read GCR4 failed");
        if (speed == ETH_SPEED_10M) {
            gcr4.switch_10base_t = 1;
        } else {
            gcr4.switch_10base_t = 0;
        }
        ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, ksz8863->addr, KSZ8863_GCR4_ADDR, gcr4.val), err, TAG, "write GCR4 failed");
    } else {
        ksz8863_pcr12_reg_t pcr12;
        ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, ksz8863->addr, KSZ8863_PCR12_BASE_ADDR + ksz8863->port_reg_offset, &(pcr12.val)),
                          err, TAG, "read PCR12 failed");
        pcr12.force_100bt = speed;
        ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, ksz8863->addr, KSZ8863_PCR12_BASE_ADDR + ksz8863->port_reg_offset, pcr12.val),
                          err, TAG, "write PCR12 failed");
    }
    return ESP_OK;
err:
    return ret;
}

static esp_err_t ksz8863_set_duplex(esp_eth_phy_t *phy, eth_duplex_t duplex)
{
    esp_err_t ret = ESP_OK;
    phy_ksz8863_t *ksz8863 = __containerof(phy, phy_ksz8863_t, parent);
    esp_eth_mediator_t *eth = ksz8863->eth;

    if (ksz8863->link_status == ETH_LINK_UP) {
        /* Since the link is going to be reconfigured, consider it down for a while */
        ksz8863->link_status = ETH_LINK_DOWN;
        /* Indicate to upper stream apps the link is cosidered down */
        ESP_GOTO_ON_ERROR(eth->on_state_changed(eth, ETH_STATE_LINK, (void *)ksz8863->link_status), err, TAG, "change link failed");
    }
    /* Set duplex mode */
    if (ksz8863->driver_mode == KSZ8863_MAC_MAC_MODE) {
        ksz8863_gcr4_reg_t gcr4;
        ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, ksz8863->addr, KSZ8863_GCR4_ADDR, &(gcr4.val)), err, TAG, "read GCR4 failed");
        if (duplex == ETH_DUPLEX_HALF) {
            gcr4.switch_half_duplex = 1;
        } else {
            gcr4.switch_half_duplex = 0;
        }
        ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, ksz8863->addr, KSZ8863_GCR4_ADDR, gcr4.val), err, TAG, "write GCR4 failed");
    } else {
        ksz8863_pcr12_reg_t pcr12;
        ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, ksz8863->addr, KSZ8863_PCR12_BASE_ADDR + ksz8863->port_reg_offset, &(pcr12.val)),
                          err, TAG, "read PCR12 failed");
        pcr12.force_full_duplex = duplex;
        ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, ksz8863->addr, KSZ8863_PCR12_BASE_ADDR + ksz8863->port_reg_offset, pcr12.val),
                          err, TAG, "write PCR12 failed");
    }

    return ESP_OK;
err:
    return ret;
}

static esp_err_t ksz8863_init(esp_eth_phy_t *phy)
{
    esp_err_t ret = ESP_OK;
    phy_ksz8863_t *ksz8863 = __containerof(phy, phy_ksz8863_t, parent);
    esp_eth_mediator_t *eth = ksz8863->eth;

    /* Power on Ethernet PHY */
    ESP_GOTO_ON_ERROR(ksz8863_pwrctl(phy, true), err, TAG, "power control failed");
    /* Check PHY ID */
    ksz8863_chipid0_reg_t id0;
    ksz8863_chipid1_reg_t id1;
    ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, ksz8863->addr, KSZ8863_CHIPID0_REG_ADDR, &(id0.val)), err, TAG, "read ID0 failed");
    ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, ksz8863->addr, KSZ8863_CHIPID1_REG_ADDR, &(id1.val)), err, TAG, "read ID1 failed");
    ESP_GOTO_ON_FALSE(id0.family_id == 0x88 && id1.chip_id == 0x03, ESP_FAIL, err, TAG, "wrong chip ID");

    return ESP_OK;
err:
    return ret;
}

static esp_err_t ksz8863_deinit(esp_eth_phy_t *phy)
{
    esp_err_t ret = ESP_OK;
    /* Power off Ethernet PHY */
    ESP_GOTO_ON_ERROR(ksz8863_pwrctl(phy, false), err, TAG, "power control failed");
    return ESP_OK;
err:
    return ret;
}

esp_eth_phy_t *esp_eth_phy_new_ksz8863(const eth_phy_config_t *config)
{
    esp_eth_phy_t *ret = NULL;
    ESP_GOTO_ON_FALSE(config, NULL, err, TAG, "can't set phy config to null");
    phy_ksz8863_t *ksz8863 = calloc(1, sizeof(phy_ksz8863_t));
    ESP_GOTO_ON_FALSE(ksz8863, NULL, err, TAG, "calloc ksz8863 failed");
    ksz8863->addr = config->phy_addr;
    ksz8863->reset_timeout_ms = config->reset_timeout_ms;
    ksz8863->link_status = ETH_LINK_DOWN;
    ksz8863->autonego_timeout_ms = config->autonego_timeout_ms;
    if (config->phy_addr == -1) { // TODO: this is kind of hacky, consider different approach
        ksz8863->driver_mode = KSZ8863_MAC_MAC_MODE;
        ksz8863->port_reg_offset = KSZ8863_PORT3_ADDR_OFFSET; // Port 3 is interfaced in MAC-MAC mode
    } else if (config->phy_addr == 0) {
        ksz8863->driver_mode = KSZ8863_PORT_PHY_MODE;
        ksz8863->port_reg_offset = KSZ8863_PORT1_ADDR_OFFSET;
    } else if (config->phy_addr == 1) {
        ksz8863->driver_mode = KSZ8863_PORT_PHY_MODE;
        ksz8863->port_reg_offset = KSZ8863_PORT2_ADDR_OFFSET;
    } else {
        ESP_GOTO_ON_FALSE(false, NULL, err, TAG, "invalid phy address");
    }

    ksz8863->parent.reset = ksz8863_reset_sw;
    ksz8863->parent.reset_hw = ksz8863_reset_hw;
    ksz8863->parent.init = ksz8863_init;
    ksz8863->parent.deinit = ksz8863_deinit;
    ksz8863->parent.set_mediator = ksz8863_set_mediator;
    ksz8863->parent.autonego_ctrl = ksz8863_autonego_ctrl;
    ksz8863->parent.get_link = ksz8863_get_link;
    ksz8863->parent.set_link = ksz8863_set_link;
    ksz8863->parent.pwrctl = ksz8863_pwrctl;
    ksz8863->parent.get_addr = ksz8863_get_addr;
    ksz8863->parent.set_addr = ksz8863_set_addr;
    ksz8863->parent.advertise_pause_ability = ksz8863_advertise_pause_ability;
    ksz8863->parent.loopback = ksz8863_loopback;
    ksz8863->parent.set_speed = ksz8863_set_speed;
    ksz8863->parent.set_duplex = ksz8863_set_duplex;
    ksz8863->parent.del = ksz8863_del;

    return &(ksz8863->parent);
err:
    return ret;
}
