/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include <stdlib.h>
#include <sys/cdefs.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_eth_phy_802_3.h"
#include "esp_eth_phy_vsc8541.h"

static const char *TAG = "vsc8541";

#define VSC8541_PHY_RESET_ASSERTION_TIME_US 1
#define VSC8541_PHY_POST_RESET_INIT_TIME_MS 15

#define VSC8541_PHY_OUI 0x0001C1 /* Microchip/Vitesse */

typedef enum {
    VSC8541_MAC_INTF_SEL_GMII_MII = 0x0,
    VSC8541_MAC_INTF_SEL_RMII     = 0x1,
    VSC8541_MAC_INTF_SEL_RGMII    = 0x2,
    VSC8541_MAC_INTF_SEL_RESERVED = 0x3,
} vsc8541_mac_interface_sel_t;

/***************Vendor Specific Register***************/
typedef enum {
    VSC8541_PAGE_STANDARD        = 0x00,
    VSC8541_PAGE_EXTENDED_1      = 0x01,
    VSC8541_PAGE_EXTENDED_2      = 0x02,
    VSC8541_PAGE_GPIO            = 0x10,
} vsc8541_page_t;

/**
 * @brief PCR(Page Control Register)
 *
 */
typedef union {
    struct {
        uint32_t register_page_select : 16; /* Select register page, default is 0 */
    };
    uint32_t val;
} pcr_reg_t;
#define ETH_PHY_PCR_REG_ADDR (0x1F)

/**
 * @brief EPC(Extended PHY Control Register), Page 0
 *
 */
typedef union {
    struct {
        uint32_t reserved1 : 3;            /* Reserved */
        uint32_t far_end_loopback : 1;     /* Far-end loopback enable */
        uint32_t reserved2 : 7;            /* Reserved */
        uint32_t mac_interface_sel : 2;    /* 00: GMII/MII, 01: RMII, 10: RGMII, 11: Reserved.
                                              Must be written prior to a soft reset to take effect. */
        uint32_t mac_supplied_clk_en : 1;  /* MAC-supplied clock enable */
        uint32_t reserved3 : 2;            /* Reserved */
    };
    uint32_t val;
} epc_reg_t;
#define ETH_PHY_EPC_REG_ADDR (0x17)

/**
 * @brief DACS(Device Auxiliary Control and Status Register), Page 0
 *
 */
typedef union {
    struct {
        uint32_t media_mode_status : 2;              /* Media mode status */
        uint32_t actiphy_link_status_to_0 : 1;       /* ActiPHY link status time-out 0 */
        uint32_t speed_status : 2;                   /* 00: 10BASE-T, 01: 100BASE-TX, 10: 1000BASE-T, 11: reserved */
        uint32_t duplex_status : 1;                  /* 1: Full-duplex, 0: Half-duplex */
        uint32_t actiphy_mode_en : 1;                /* ActiPHY mode enable */
        uint32_t actiphy_link_status_to_1 : 1;       /* ActiPHY link status time-out 1 */
        uint32_t d_polarity_inversion : 1;           /* Pair D polarity inversion */
        uint32_t c_polarity_inversion : 1;           /* Pair C polarity inversion */
        uint32_t b_polarity_inversion : 1;           /* Pair B polarity inversion */
        uint32_t a_polarity_inversion : 1;           /* Pair A polarity inversion */
        uint32_t cd_pair_swap : 1;                   /* Pair C/D swap */
        uint32_t hp_auto_mdix_crossover : 1;         /* HP Auto-MDIX crossover indication */
        uint32_t auto_negotiation_disabled : 1;      /* Auto-negotiation disabled */
        uint32_t auto_negotiation_complete : 1;      /* Auto-negotiation complete */
    };
    uint32_t val;
} dacs_reg_t;
#define ETH_PHY_DACS_REG_ADDR (0x1C)

/**
 * @brief RCR (RGMII Control Register), page 0x02, Address 0x14 (Register 20E2)
 *
 */
typedef union {
    struct {
        uint32_t tx_clk_delay : 3;          /* delay time(ns): 000:0.2ns,001:0.8, 010:1.1,011:1.7,100:2.0,101:2.3,110:2.6,111:3.4 */
        uint32_t rgmii_txd_reversal : 1;    /* RGMII/GMII TXD bit reversal */
        uint32_t rx_clk_delay : 3;          /* delay time(ns): 000:0.2ns,001:0.8, 010:1.1,011:1.7,100:2.0,101:2.3,110:2.6,111:3.4 */
        uint32_t rgmii_rxd_reversal : 1;    /* RGMII/GMII RXD bit reversal */
        uint32_t reserved1 : 4;             /* Reserved */
        uint32_t sof_en : 1;                /* SOF Enable: 1: Enable, 0: Disable */
        uint32_t reserved2 : 2;             /* Reserved */
        uint32_t flf2_en : 1;               /* Fast Link Failure 2 indication Enable: 1: Enable, 0: Disable */
    };
    uint32_t val;
} rcr_reg_t;
#define ETH_PHY_RCR_REG_ADDR (0x14)

/**
 * @brief GC2R (GPIO Control 2 Register), page 0x10, address 0x0E
 *
 */
typedef union {
    struct {
        uint32_t reserved1 : 9;
        uint32_t tri_state_en : 1;
        uint32_t reserved2 : 1;
        uint32_t coma_mode_input_pin_data : 1;   /* RO: COMA_MODE pin sampled level */
        uint32_t coma_mode_output_pin_data : 1;  /* R/W: output value when pin is output */
        uint32_t coma_mode_output_en : 1;        /* 1: COMA_MODE is input; 0: COMA_MODE is output */
        uint32_t reserved3 : 2;
    };
    uint32_t val;
} gc2r_reg_t;
#define ETH_PHY_GC2R_REG_ADDR (0x0E)

typedef struct {
    phy_802_3_t phy_802_3;
} phy_vsc8541_t;

static esp_err_t vsc8541_page_select(phy_vsc8541_t *vsc8541, vsc8541_page_t page)
{
    esp_err_t ret = ESP_OK;
    esp_eth_mediator_t *eth = vsc8541->phy_802_3.eth;
    pcr_reg_t pcr = {
        .register_page_select = (uint16_t)page
    };
    ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, vsc8541->phy_802_3.addr, ETH_PHY_PCR_REG_ADDR, pcr.val), err, TAG, "write PCR failed");
    return ESP_OK;
err:
    return ret;
}

static esp_err_t vsc8541_update_link_duplex_speed(phy_vsc8541_t *vsc8541)
{
    esp_err_t ret = ESP_OK;
    esp_eth_mediator_t *eth = vsc8541->phy_802_3.eth;
    uint32_t addr = vsc8541->phy_802_3.addr;
    eth_speed_t speed = ETH_SPEED_10M;
    eth_duplex_t duplex = ETH_DUPLEX_HALF;
    uint32_t peer_pause_ability = false;
    dacs_reg_t dacs;
    bmsr_reg_t bmsr;
    anlpar_reg_t anlpar;

    ESP_GOTO_ON_ERROR(vsc8541_page_select(vsc8541, VSC8541_PAGE_STANDARD), err, TAG, "select page 0 failed");
    ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, addr, ETH_PHY_BMSR_REG_ADDR, &(bmsr.val)), err, TAG, "read BMSR failed");
    ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, addr, ETH_PHY_DACS_REG_ADDR, &(dacs.val)), err, TAG, "read DACS failed");
    ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, addr, ETH_PHY_ANLPAR_REG_ADDR, &(anlpar.val)), err, TAG, "read ANLPAR failed");
    eth_link_t link = bmsr.link_status ? ETH_LINK_UP : ETH_LINK_DOWN;
    /* check if link status changed */
    if (vsc8541->phy_802_3.link_status != link) {
        /* when link up, read negotiation result */
        if (link == ETH_LINK_UP) {
            switch (dacs.speed_status) {
            case 0: //10M
                speed = ETH_SPEED_10M;
                break;
            case 1: //100M
                speed = ETH_SPEED_100M;
                break;
            case 2: //1000M
                speed = ETH_SPEED_1000M;
                break;
            default:
                break;
            }
            duplex = dacs.duplex_status ? ETH_DUPLEX_FULL : ETH_DUPLEX_HALF;
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
        vsc8541->phy_802_3.link_status = link;
    }
    return ESP_OK;
err:
    return ret;
}

static esp_err_t vsc8541_get_link(esp_eth_phy_t *phy)
{
    esp_err_t ret = ESP_OK;
    phy_vsc8541_t *vsc8541 = __containerof(esp_eth_phy_into_phy_802_3(phy), phy_vsc8541_t, phy_802_3);

    /* Update information about link, speed, duplex */
    ESP_GOTO_ON_ERROR(vsc8541_update_link_duplex_speed(vsc8541), err, TAG, "update link duplex speed failed");
    return ESP_OK;
err:
    return ret;
}

static esp_err_t vsc8541_custom_ioctl(esp_eth_phy_t *phy, int cmd, void *data)
{
    esp_err_t ret = ESP_OK;
    phy_vsc8541_t *vsc8541 = __containerof(esp_eth_phy_into_phy_802_3(phy), phy_vsc8541_t, phy_802_3);
    phy_802_3_t *phy_802_3 = &vsc8541->phy_802_3;
    esp_eth_mediator_t *eth = phy_802_3->eth;
    epc_reg_t epc;
    rcr_reg_t rcr;

    switch (cmd) {
    case VSC8541_ETH_CMD_S_RGMII_CLK_DELAY: {
        ESP_GOTO_ON_FALSE(data != NULL, ESP_ERR_INVALID_ARG, err, TAG, "data can't be null");
        vsc8541_rgmii_clk_delay_config_t *cfg = (vsc8541_rgmii_clk_delay_config_t *)data;
        ESP_GOTO_ON_FALSE(cfg->rx_clk_delay <= 0x7, ESP_ERR_INVALID_ARG, err, TAG, "rx_clk_delay out of range");
        ESP_GOTO_ON_FALSE(cfg->tx_clk_delay <= 0x7, ESP_ERR_INVALID_ARG, err, TAG, "tx_clk_delay out of range");

        ESP_GOTO_ON_ERROR(vsc8541_page_select(vsc8541, VSC8541_PAGE_STANDARD), err, TAG, "select page 0 failed");
        ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, phy_802_3->addr, ETH_PHY_EPC_REG_ADDR, &epc.val), err, TAG, "read EPC failed");
        ESP_GOTO_ON_FALSE(epc.mac_interface_sel == VSC8541_MAC_INTF_SEL_RGMII, ESP_ERR_INVALID_STATE, err, TAG, "RGMII mode is required");

        ESP_GOTO_ON_ERROR(vsc8541_page_select(vsc8541, VSC8541_PAGE_EXTENDED_2), err, TAG, "select page 2 failed");
        ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, phy_802_3->addr, ETH_PHY_RCR_REG_ADDR, &rcr.val), err, TAG, "read 20E2 failed");
        rcr.rx_clk_delay = cfg->rx_clk_delay;
        rcr.tx_clk_delay = cfg->tx_clk_delay;
        ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, phy_802_3->addr, ETH_PHY_RCR_REG_ADDR, rcr.val), err, TAG, "write 20E2 failed");
        ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, phy_802_3->addr, ETH_PHY_RCR_REG_ADDR, &rcr.val), err, TAG, "read 20E2 failed");
        ESP_GOTO_ON_ERROR(vsc8541_page_select(vsc8541, VSC8541_PAGE_STANDARD), err, TAG, "select page 0 failed");
        break;
    }
    default:
        ret = ESP_ERR_INVALID_ARG;
        break;
    }

    ESP_GOTO_ON_ERROR(vsc8541_page_select(vsc8541, VSC8541_PAGE_STANDARD), err, TAG, "restore page 0 failed");
    return ESP_OK;
err:
    vsc8541_page_select(vsc8541, VSC8541_PAGE_STANDARD);
    return ret;
}

static esp_err_t vsc8541_init(esp_eth_phy_t *phy)
{
    esp_err_t ret = ESP_OK;
    phy_802_3_t *phy_802_3 = esp_eth_phy_into_phy_802_3(phy);

    /* Detect PHY address */
    if (phy_802_3->addr == ESP_ETH_PHY_ADDR_AUTO) {
        ESP_GOTO_ON_ERROR(esp_eth_phy_802_3_detect_phy_addr(phy_802_3->eth, &phy_802_3->addr), err, TAG, "Detect PHY address failed");
    }

    /* Basic PHY init */
    ESP_GOTO_ON_ERROR(esp_eth_phy_802_3_basic_phy_init(phy_802_3), err, TAG, "failed to init PHY");

    /* Check PHY ID */
    uint32_t oui;
    uint8_t model;
    ESP_GOTO_ON_ERROR(esp_eth_phy_802_3_read_oui(phy_802_3, &oui), err, TAG, "read OUI failed");
    ESP_GOTO_ON_ERROR(esp_eth_phy_802_3_read_manufac_info(phy_802_3, &model, NULL), err, TAG, "read manufacturer's info failed");
    ESP_GOTO_ON_FALSE(oui == VSC8541_PHY_OUI, ESP_FAIL, err, TAG, "wrong chip OUI (read 0x%" PRIx32 ", model 0x%" PRIx8 ")", oui, model);

    phy_vsc8541_t *vsc8541 = __containerof(phy_802_3, phy_vsc8541_t, phy_802_3);
    esp_eth_mediator_t *eth = vsc8541->phy_802_3.eth;
    uint32_t addr = vsc8541->phy_802_3.addr;
    gc2r_reg_t gc2r;

    ESP_GOTO_ON_ERROR(vsc8541_page_select(vsc8541, VSC8541_PAGE_GPIO), err, TAG, "select GPIO register space page failed");
    ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, addr, ETH_PHY_GC2R_REG_ADDR, &gc2r.val), err, TAG, "read GC2R failed");
    if (gc2r.coma_mode_input_pin_data) {
        /* COMA_MODE reads high: drive pin as output low to leave coma mode.
         * GC2R bit selects output when clear (0); set output data to 0. */
        gc2r.coma_mode_output_en = 0;
        gc2r.coma_mode_output_pin_data = 0;
        ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, addr, ETH_PHY_GC2R_REG_ADDR, gc2r.val), err, TAG, "write GC2R failed");
    }
    ESP_GOTO_ON_ERROR(vsc8541_page_select(vsc8541, VSC8541_PAGE_STANDARD), err, TAG, "select page 0 failed");

    return ESP_OK;
err:
    return ret;
}

esp_eth_phy_t *esp_eth_phy_new_vsc8541(const eth_phy_config_t *config)
{
    esp_eth_phy_t *ret = NULL;
    phy_vsc8541_t *vsc8541 = calloc(1, sizeof(phy_vsc8541_t));
    ESP_GOTO_ON_FALSE(vsc8541, NULL, err, TAG, "calloc vsc8541 failed");
    eth_phy_config_t vsc8541_config = *config;
    // default chip specific configuration
    if (config->hw_reset_assert_time_us == 0) {
        vsc8541_config.hw_reset_assert_time_us = VSC8541_PHY_RESET_ASSERTION_TIME_US;
    }
    if (config->post_hw_reset_delay_ms == 0) {
        vsc8541_config.post_hw_reset_delay_ms = VSC8541_PHY_POST_RESET_INIT_TIME_MS;
    }
    ESP_GOTO_ON_FALSE(esp_eth_phy_802_3_obj_config_init(&vsc8541->phy_802_3, &vsc8541_config) == ESP_OK,
                      NULL, err, TAG, "configuration initialization of PHY 802.3 failed");

    // redefine functions which need to be customized for sake of VSC8541
    vsc8541->phy_802_3.parent.init = vsc8541_init;
    vsc8541->phy_802_3.parent.get_link = vsc8541_get_link;
    vsc8541->phy_802_3.parent.custom_ioctl = vsc8541_custom_ioctl;

    return &vsc8541->phy_802_3.parent;
err:
    if (vsc8541 != NULL) {
        free(vsc8541);
    }
    return ret;
}
