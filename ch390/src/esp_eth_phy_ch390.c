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

#include "esp_eth_phy_ch390.h"

/** 
 * @warning This value is NOT the same as the datasheet!!! Hoping WCH fix it 
 * in the furture version!
*/
#define CH390_INFO_OUI                       0x1CDC64

#define CH390_INFO_MODEL                     0x01

#define ETH_PHY_PAGE_SEL_REG_ADDR 0x1F

typedef union{
    struct
    {
        uint32_t reserved1 : 3;
        uint32_t force_link : 1;
        uint32_t remote_lpbk : 1;
        uint32_t pcs_lpbk : 1;
        uint32_t pma_lpbk : 1;
        uint32_t jabber_en : 1;
        uint32_t sqe_en : 1;
        uint32_t reserved2 : 7;
    };
    uint32_t val;
}phy_ctl1_reg_t;
#define ETH_PHY_CTL1_REG_ADDR 0X19
#define ETH_PHY_CTL1_REG_PAGE 0x00

typedef struct {
    phy_802_3_t phy_802_3;
} phy_ch390_t;

static const char *TAG = "ch390.phy";

static esp_err_t ch390_update_link_duplex_speed(phy_ch390_t *ch390)
{
    esp_err_t ret = ESP_OK;
    esp_eth_mediator_t *eth = ch390->phy_802_3.eth;
    uint32_t addr = ch390->phy_802_3.addr;
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
    if (ch390->phy_802_3.link_status != link) {
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
        ch390->phy_802_3.link_status = link;
    }
    return ESP_OK;
err:
    return ret;
}

static esp_err_t ch390_get_link(esp_eth_phy_t *phy)
{
    esp_err_t ret = ESP_OK;
    phy_ch390_t *ch390 = __containerof(esp_eth_phy_into_phy_802_3(phy), phy_ch390_t, phy_802_3);
    /* Update information about link, speed, duplex */
    ESP_GOTO_ON_ERROR(ch390_update_link_duplex_speed(ch390), err, TAG, "update link duplex speed failed");
    return ESP_OK;
err:
    return ret;
}

static esp_err_t ch390_loopback(esp_eth_phy_t *phy, bool enable)
{
    esp_err_t ret = ESP_OK;
    phy_802_3_t *phy_802_3 = esp_eth_phy_into_phy_802_3(phy);
    esp_eth_mediator_t *eth = phy_802_3->eth;
    /* Set Loopback function */
    // Enable PMA loopback in PHY_Control1 register
    bmcr_reg_t bmcr;
    phy_ctl1_reg_t phy_ctl1;
    ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, phy_802_3->addr, ETH_PHY_BMCR_REG_ADDR, &(bmcr.val)), err, TAG, "read BMCR failed");
    ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, phy_802_3->addr, ETH_PHY_PAGE_SEL_REG_ADDR, ETH_PHY_CTL1_REG_PAGE),
                      err, TAG, "write PAGE_SEL failed");
    ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, phy_802_3->addr, ETH_PHY_CTL1_REG_ADDR, &(phy_ctl1.val)), err, TAG, "read PHY_CTL1 failed");

    if (enable) {
        bmcr.en_loopback = 1;
        phy_ctl1.pma_lpbk = 1;
    } else {
        bmcr.en_loopback = 0;
        phy_ctl1.pma_lpbk = 0;
    }
    ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, phy_802_3->addr, ETH_PHY_BMCR_REG_ADDR, bmcr.val), err, TAG, "write BMCR failed");
    ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, phy_802_3->addr, ETH_PHY_PAGE_SEL_REG_ADDR, ETH_PHY_CTL1_REG_PAGE),
                      err, TAG, "write PAGE_SEL failed");
    ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, phy_802_3->addr, ETH_PHY_CTL1_REG_ADDR, phy_ctl1.val), err, TAG, "write PHY_CTL1 failed");
    return ESP_OK;
err:
    return ret;
}

static esp_err_t ch390_init(esp_eth_phy_t *phy)
{
    esp_err_t ret = ESP_OK;
    phy_802_3_t *phy_802_3 = esp_eth_phy_into_phy_802_3(phy);

    /* Basic PHY init */
    ESP_GOTO_ON_ERROR(esp_eth_phy_802_3_basic_phy_init(phy_802_3), err, TAG, "failed to init PHY");

    /* Check PHY ID */
    uint32_t oui;
    uint8_t model;
    ESP_GOTO_ON_ERROR(esp_eth_phy_802_3_read_oui(phy_802_3, &oui), err, TAG, "read OUI failed");
    ESP_GOTO_ON_ERROR(esp_eth_phy_802_3_read_manufac_info(phy_802_3, &model, NULL), err, TAG, "read manufacturer's info failed");
    ESP_GOTO_ON_FALSE(oui == CH390_INFO_OUI && model == CH390_INFO_MODEL, ESP_FAIL, err, TAG, "wrong chip ID");

    return ESP_OK;
err:
    return ret;
}

esp_eth_phy_t *esp_eth_phy_new_ch390(const eth_phy_config_t *config)
{
    esp_eth_phy_t *ret = NULL;
    phy_ch390_t *ch390 = calloc(1, sizeof(phy_ch390_t));
    ESP_GOTO_ON_FALSE(ch390, NULL, err, TAG, "calloc ch390 failed");
    ESP_GOTO_ON_FALSE(esp_eth_phy_802_3_obj_config_init(&ch390->phy_802_3, config) == ESP_OK,
                      NULL, err, TAG, "configuration initialization of PHY 802.3 failed");

    // redefine functions which need to be customized for sake of ch390
    ch390->phy_802_3.parent.init = ch390_init;
    ch390->phy_802_3.parent.get_link = ch390_get_link;
    ch390->phy_802_3.parent.loopback = ch390_loopback;

    return &ch390->phy_802_3.parent;
err:
    if (ch390 != NULL) {
        free(ch390);
    }
    return ret;
}
