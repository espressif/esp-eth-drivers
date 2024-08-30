/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileContributor: 2024 Sergey Kharenko
 * SPDX-FileContributor: 2024 Espressif Systems (Shanghai) CO LTD
 */

#include <string.h>
#include <stdlib.h>
#include <sys/cdefs.h>
#include "esp_log.h"
#include "esp_check.h"
#include "esp_eth_phy_802_3.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "ch182.h"
#include "esp_eth_phy_ch182.h"

typedef struct {
    phy_802_3_t phy_802_3;
    bool use_esp_refclk;
} phy_ch182_t;

static const char *TAG = "ch182.phy";

static esp_err_t ch182_set_led(phy_ch182_t *ch182, uint8_t mode, uint8_t freq, uint8_t duty)
{
    esp_err_t ret = ESP_OK;
    interrupt_mask_reg_t intr_mask;
    phy_802_3_t *phy_802_3 = &(ch182->phy_802_3);
    esp_eth_mediator_t *eth = ch182->phy_802_3.eth;

    ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, phy_802_3->addr, ETH_PHY_PAGE_SEL_REG_ADDR, ETH_INTERRUPT_MASK_REG_PAGE),
                      err, TAG, "write PAGE_SEL failed");
    ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, phy_802_3->addr, ETH_INTERRUPT_MASK_REG_ADDR, &(intr_mask.val)),
                      err, TAG, "read INTERRUPT_MASK failed");
    intr_mask.led_sel = mode;
    ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, phy_802_3->addr, ETH_PHY_PAGE_SEL_REG_ADDR, ETH_INTERRUPT_MASK_REG_PAGE),
                      err, TAG, "write PAGE_SEL failed");
    ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, phy_802_3->addr, ETH_INTERRUPT_MASK_REG_ADDR, intr_mask.val),
                      err, TAG, "write INTERRUPT_MASK failed");

    led_control_reg_t led_ctrl;
    led_ctrl.duty_cycle = duty;
    led_ctrl.led_freq_ctrl = freq;
    ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, phy_802_3->addr, ETH_PHY_PAGE_SEL_REG_ADDR, ETH_LED_CONTROL_REG_PAGE),
                      err, TAG, "write PAGE_SEL failed");
    ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, phy_802_3->addr, ETH_LED_CONTROL_REG_ADDR, led_ctrl.val),
                      err, TAG, "write LED_CONTROL failed");
    return ESP_OK;

err:
    return ret;
}

static esp_err_t ch182_set_mode(phy_ch182_t *ch182, uint8_t mode)
{
    esp_err_t ret = ESP_OK;
    rmii_mode_set1_reg_t rmii_ms1;
    phy_802_3_t *phy_802_3 = &(ch182->phy_802_3);
    esp_eth_mediator_t *eth = ch182->phy_802_3.eth;

    ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, phy_802_3->addr, ETH_PHY_PAGE_SEL_REG_ADDR, ETH_RMII_MODE_SET1_REG_PAGE),
                      err, TAG, "write PAGE_SEL failed");
    ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, phy_802_3->addr, ETH_RMII_MODE_SET1_REG_ADDR, &(rmii_ms1.val)),
                      err, TAG, "read RMII_MODE_SET1 failed");
    rmii_ms1.rmii_mode = mode;
    ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, phy_802_3->addr, ETH_PHY_PAGE_SEL_REG_ADDR, ETH_RMII_MODE_SET1_REG_PAGE),
                      err, TAG, "write PAGE_SEL failed");
    ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, phy_802_3->addr, ETH_RMII_MODE_SET1_REG_ADDR, rmii_ms1.val),
                      err, TAG, "write RMII_MODE_SET1 failed");
    return ESP_OK;

err:
    return ret;
}

static esp_err_t ch182_set_rmii_refclk_dir(phy_ch182_t *ch182, uint32_t dir)
{
    esp_err_t ret = ESP_OK;
    rmii_mode_set1_reg_t rmii_ms1;
    phy_802_3_t *phy_802_3 = &(ch182->phy_802_3);
    esp_eth_mediator_t *eth = ch182->phy_802_3.eth;

    ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, phy_802_3->addr, ETH_PHY_PAGE_SEL_REG_ADDR, ETH_RMII_MODE_SET1_REG_PAGE),
                      err, TAG, "write PAGE_SEL failed");
    ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, phy_802_3->addr, ETH_RMII_MODE_SET1_REG_ADDR, &(rmii_ms1.val)),
                      err, TAG, "read RMII_MODE_SET1 failed");

    rmii_ms1.rg_rmii_clk_dir = dir;

    ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, phy_802_3->addr, ETH_PHY_PAGE_SEL_REG_ADDR, ETH_RMII_MODE_SET1_REG_PAGE),
                      err, TAG, "write PAGE_SEL failed");
    ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, phy_802_3->addr, ETH_RMII_MODE_SET1_REG_ADDR, rmii_ms1.val),
                      err, TAG, "write RMII_MODE_SET1 failed");

    return ESP_OK;

err:
    return ret;
}

static esp_err_t ch182_update_link_duplex_speed(phy_ch182_t *ch182)
{
    esp_err_t ret = ESP_OK;
    esp_eth_mediator_t *eth = ch182->phy_802_3.eth;
    uint32_t addr = ch182->phy_802_3.addr;
    eth_speed_t speed = ETH_SPEED_10M;
    eth_duplex_t duplex = ETH_DUPLEX_HALF;
    bmcr_reg_t bmcr;
    bmsr_reg_t bmsr;
    uint32_t peer_pause_ability = false;
    anlpar_reg_t anlpar;
    ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, addr, ETH_PHY_BMSR_REG_ADDR, &(bmsr.val)), err, TAG, "read BMSR failed");
    ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, addr, ETH_PHY_BMSR_REG_ADDR, &(bmsr.val)), err, TAG, "read BMSR failed");
    ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, addr, ETH_PHY_ANLPAR_REG_ADDR, &(anlpar.val)), err, TAG, "read ANLPAR failed");
    eth_link_t link = bmsr.link_status ? ETH_LINK_UP : ETH_LINK_DOWN;
    /* check if link status changed */
    if (ch182->phy_802_3.link_status != link) {
        /* when link up, read negotiation result */
        if (link == ETH_LINK_UP) {
            ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, addr, ETH_PHY_BMCR_REG_ADDR, &(bmcr.val)), err, TAG, "read BMCR failed");
            if (bmcr.speed_select) {
                speed = ETH_SPEED_100M;
            } else {
                speed = ETH_SPEED_10M;
            }
            if (bmcr.duplex_mode) {
                duplex = ETH_DUPLEX_FULL;
            } else {
                duplex = ETH_DUPLEX_HALF;
            }
            ESP_GOTO_ON_ERROR(eth->on_state_changed(eth, ETH_STATE_SPEED, (void *)speed), err, TAG, "change speed failed");
            ESP_GOTO_ON_ERROR(eth->on_state_changed(eth, ETH_STATE_DUPLEX, (void *)duplex), err, TAG, "change duplex failed");
            /* if we're in duplex mode, and peer has the flow control ability */
            if (duplex == ETH_DUPLEX_FULL && anlpar.symmetric_pause) {
                peer_pause_ability = 1;
            } else {
                peer_pause_ability = 0;
            }
            ESP_GOTO_ON_ERROR(eth->on_state_changed(eth, ETH_STATE_PAUSE, (void *)peer_pause_ability), err, TAG, "change pause ability failed");
        }
        ESP_GOTO_ON_ERROR(eth->on_state_changed(eth, ETH_STATE_LINK, (void *)link), err, TAG, "change link failed");
        ch182->phy_802_3.link_status = link;
    }
    return ESP_OK;
err:
    return ret;
}

static esp_err_t ch182_init(esp_eth_phy_t *phy)
{
    esp_err_t ret = ESP_OK;
    phy_802_3_t *p_phy_802_3 = esp_eth_phy_into_phy_802_3(phy);

    /* Basic PHY init */
    ESP_GOTO_ON_ERROR(esp_eth_phy_802_3_basic_phy_init(p_phy_802_3), err, TAG, "failed to init PHY");

    /* Check PHY ID */
    uint32_t oui;
    ESP_GOTO_ON_ERROR(esp_eth_phy_802_3_read_oui(p_phy_802_3, &oui), err, TAG, "read OUI failed");
    ESP_GOTO_ON_FALSE(oui == CH182_INFO_OUI, ESP_FAIL, err, TAG, "wrong chip ID");

    phy_ch182_t *ch182 = __containerof(p_phy_802_3, phy_ch182_t, phy_802_3);
    uint32_t dir = 0;
    if (ch182->use_esp_refclk) {
        dir = ETH_RMII_CLK_DIR_IN;
    } else {
        dir = ETH_RMII_CLK_DIR_OUT;
    }

    ESP_GOTO_ON_ERROR(ch182_set_led(ch182, ETH_DEFAULT_LED_MODE, ETH_DEFAULT_LED_FREQ, ETH_DEFAULT_LED_DUTY), err, TAG, "cannot setup led");
    ESP_GOTO_ON_ERROR(ch182_set_mode(ch182, ETH_RMII_MODE_RMII), err, TAG, "cannot set MII/RMII Mode");
    ESP_GOTO_ON_ERROR(ch182_set_rmii_refclk_dir(ch182, dir), err, TAG, "cannot set RMII REFCLK direction");

    return ESP_OK;
err:
    return ret;
}

static esp_err_t ch182_get_link(esp_eth_phy_t *phy)
{
    esp_err_t ret = ESP_OK;
    phy_ch182_t *ch182 = __containerof(esp_eth_phy_into_phy_802_3(phy), phy_ch182_t, phy_802_3);
    /* Update information about link, speed, duplex */
    ESP_GOTO_ON_ERROR(ch182_update_link_duplex_speed(ch182), err, TAG, "update link duplex speed failed");
    return ESP_OK;
err:
    return ret;
}

static esp_err_t ch182_loopback(esp_eth_phy_t *phy, bool enable)
{
    esp_err_t ret = ESP_OK;
    phy_802_3_t *phy_802_3 = esp_eth_phy_into_phy_802_3(phy);
    esp_eth_mediator_t *eth = phy_802_3->eth;
    /* Set Loopback function */
    // Enable remote loopback in PHY_Control1 register
    bmcr_reg_t bmcr;
    phy_ctl1_reg_t phy_ctl1;
    ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, phy_802_3->addr, ETH_PHY_BMCR_REG_ADDR, &(bmcr.val)), err, TAG, "read BMCR failed");
    ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, phy_802_3->addr, ETH_PHY_PAGE_SEL_REG_ADDR, ETH_PHY_CTL1_REG_PAGE),
                      err, TAG, "write PAGE_SEL failed");
    ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, phy_802_3->addr, ETH_PHY_CTL1_REG_ADDR, &(phy_ctl1.val)), err, TAG, "read PHY_CTL1 failed");
    if (enable) {
        bmcr.en_loopback = 1;
        phy_ctl1.remote_lpbk = 1;
    } else {
        bmcr.en_loopback = 0;
        phy_ctl1.remote_lpbk = 0;
    }
    ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, phy_802_3->addr, ETH_PHY_BMCR_REG_ADDR, bmcr.val), err, TAG, "write BMCR failed");
    ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, phy_802_3->addr, ETH_PHY_PAGE_SEL_REG_ADDR, ETH_PHY_CTL1_REG_PAGE),
                      err, TAG, "write PAGE_SEL failed");
    ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, phy_802_3->addr, ETH_PHY_CTL1_REG_ADDR, phy_ctl1.val), err, TAG, "write PHY_CTL1 failed");
    return ESP_OK;
err:
    return ret;
}

static esp_eth_phy_t *esp_eth_phy_new_ch182_default(const eth_phy_config_t *config)
{
    esp_eth_phy_t *ret = NULL;
    phy_ch182_t *ch182 = calloc(1, sizeof(phy_ch182_t));
    ESP_GOTO_ON_FALSE(ch182, NULL, err, TAG, "calloc ch182 failed");
    ESP_GOTO_ON_FALSE(esp_eth_phy_802_3_obj_config_init(&ch182->phy_802_3, config) == ESP_OK,
                      NULL, err, TAG, "configuration initialization of PHY 802.3 failed");

    // redefine functions which need to be customized for sake of ch182
    ch182->phy_802_3.parent.init = ch182_init;
    ch182->phy_802_3.parent.get_link = ch182_get_link;
    ch182->phy_802_3.parent.loopback = ch182_loopback;

    return &ch182->phy_802_3.parent;
err:
    if (ch182 != NULL) {
        free(ch182);
    }
    return ret;
}

esp_eth_phy_t *esp_eth_phy_new_ch182(const eth_phy_config_t *config)
{
    esp_eth_phy_t *ret = esp_eth_phy_new_ch182_default(config);
    if (ret != NULL) {
        phy_ch182_t *ch182 = __containerof(esp_eth_phy_into_phy_802_3(ret), phy_ch182_t, phy_802_3);
        ch182->use_esp_refclk = false;
    }
    return ret;
}

esp_eth_phy_t *esp_eth_phy_new_ch182_use_esp_refclk(const eth_phy_config_t *config)
{
    esp_eth_phy_t *ret = esp_eth_phy_new_ch182_default(config);
    if (ret != NULL) {
        phy_ch182_t *ch182 = __containerof(esp_eth_phy_into_phy_802_3(ret), phy_ch182_t, phy_802_3);
        ch182->use_esp_refclk = true;
    }
    return ret;
}
