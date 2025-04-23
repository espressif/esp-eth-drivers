/*
 * SPDX-FileCopyrightText: 2019-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include <stdlib.h>
#include <sys/cdefs.h>
#include <inttypes.h>
#include "esp_log.h"
#include "esp_check.h"
#include "esp_eth_phy_802_3.h"

static const char *TAG = "adin1200";

/***************Vendor Specific Register***************/
/**
 * @brief PHY Status 1 Register
 *
 */
typedef union {
    struct {
        uint32_t lp_apause_adv : 1; /* The link partner has advertised asymmetric pause */
        uint32_t lp_pause_adv : 1; /* The link partner has advertised pause */
        uint32_t autoneg_sup : 1; /* Local and remote PHYs support autonegotiation */
        uint32_t col_stat : 1; /* Indicates that collision is asserted */
        uint32_t rx_dv_stat : 1; /* Indication that receive data valid (RX_DV) is asserted. */
        uint32_t tx_en_stat : 1; /* Indication that transmit enable (TX_EN) is asserted */
        uint32_t link_stat : 1; /* Link status */
        uint32_t hcd_tech : 3; /* Indication of the resolved technology after the link is established */
        uint32_t b_10_pol_inv : 1; /*  polarity of the 10BASE-T signal inversion */
        uint32_t pair_01_swap : 1; /* Pair 0 and Pair 1 swap */
        uint32_t autoneg_stat : 1; /* Autonegotiation Status Bit */
        uint32_t par_det_flt_stat: 1; /* Parallel Detection Fault Status Bit */
        uint32_t reserved : 1; /* Reserved */
        uint32_t phy_in_stndby : 1; /* PHY is in standby state and does not attempt to bring up links */
    };
    uint32_t val;
} ps1r_reg_t;
#define ETH_PHY_PS1R_REG_ADDR (0x1A)

typedef struct {
    phy_802_3_t phy_802_3;
} phy_adin1200_t;

static esp_err_t adin1200_update_link_duplex_speed(phy_adin1200_t *adin1200)
{
    esp_err_t ret = ESP_OK;
    esp_eth_mediator_t *eth = adin1200->phy_802_3.eth;
    uint32_t addr = adin1200->phy_802_3.addr;
    eth_speed_t speed = ETH_SPEED_10M;
    eth_duplex_t duplex = ETH_DUPLEX_HALF;
    uint32_t peer_pause_ability = false;
    anlpar_reg_t anlpar;
    bmsr_reg_t bmsr;

    ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, addr, ETH_PHY_ANLPAR_REG_ADDR, &(anlpar.val)), err, TAG, "read ANLPAR failed");
    ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, addr, ETH_PHY_BMSR_REG_ADDR, &(bmsr.val)), err, TAG, "read BMSR failed");
    eth_link_t link = bmsr.link_status ? ETH_LINK_UP : ETH_LINK_DOWN;
    /* check if link status changed */
    if (adin1200->phy_802_3.link_status != link) {
        /* when link up, read negotiation result */
        if (link == ETH_LINK_UP) {
            ps1r_reg_t ps1r;
            ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, addr, ETH_PHY_PS1R_REG_ADDR, &(ps1r.val)), err, TAG, "read PS1R failed");
            switch (ps1r.hcd_tech) {
            case 0: //10Base-T half-duplex
                speed = ETH_SPEED_10M;
                duplex = ETH_DUPLEX_HALF;
                break;
            case 1: //10Base-T full-duplex
                speed = ETH_SPEED_10M;
                duplex = ETH_DUPLEX_FULL;
                break;
            case 2: //100Base-TX half-duplex
                speed = ETH_SPEED_100M;
                duplex = ETH_DUPLEX_HALF;
                break;
            case 3: //100Base-TX full-duplex
                speed = ETH_SPEED_100M;
                duplex = ETH_DUPLEX_FULL;
                break;
            default:
                break;
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
        adin1200->phy_802_3.link_status = link;
    }
    return ESP_OK;
err:
    return ret;
}

static esp_err_t adin1200_get_link(esp_eth_phy_t *phy)
{
    esp_err_t ret = ESP_OK;
    phy_adin1200_t *adin1200 = __containerof(esp_eth_phy_into_phy_802_3(phy), phy_adin1200_t, phy_802_3);

    /* Update information about link, speed, duplex */
    ESP_GOTO_ON_ERROR(adin1200_update_link_duplex_speed(adin1200), err, TAG, "update link duplex speed failed");
    return ESP_OK;
err:
    return ret;
}

static esp_err_t adin1200_init(esp_eth_phy_t *phy)
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
    ESP_GOTO_ON_FALSE(oui == 0xa0ef && model == 0x02, ESP_FAIL, err, TAG, "wrong chip ID (read oui=0x%" PRIx32 ", model=0x%" PRIx8 ")", oui, model);

    return ESP_OK;
err:
    return ret;
}

esp_eth_phy_t *esp_eth_phy_new_adin1200(const eth_phy_config_t *config)
{
    esp_eth_phy_t *ret = NULL;
    phy_adin1200_t *adin1200 = calloc(1, sizeof(phy_adin1200_t));
    ESP_GOTO_ON_FALSE(adin1200, NULL, err, TAG, "calloc adin1200 failed");
    ESP_GOTO_ON_FALSE(esp_eth_phy_802_3_obj_config_init(&adin1200->phy_802_3, config) == ESP_OK,
                      NULL, err, TAG, "configuration initialization of PHY 802.3 failed");

    // redefine functions which need to be customized for sake of adin1200
    adin1200->phy_802_3.parent.init = adin1200_init;
    adin1200->phy_802_3.parent.get_link = adin1200_get_link;

    return &adin1200->phy_802_3.parent;
err:
    if (adin1200 != NULL) {
        free(adin1200);
    }
    return ret;
}
