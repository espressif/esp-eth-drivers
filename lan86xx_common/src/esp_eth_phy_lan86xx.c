/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include <stdlib.h>
#include <sys/cdefs.h>
#include "esp_log.h"
#include "esp_check.h"
#include "esp_eth_phy_802_3.h"
#include "esp_eth_driver.h"
#include "esp_eth_phy_lan86xx.h"

static const char *TAG = "lan86xx_phy";

/***************List of Supported Models***************/
#define LAN86XX_OUI 0xC0001C

#define LAN867x_MODEL_NUM  0x16
#define LAN865X_MODEL_NUM  0x1B

static const uint8_t supported_models[] = {
    LAN867x_MODEL_NUM,
    LAN865X_MODEL_NUM,
};

/***************Vendor Specific Register***************/
typedef union {
    struct {
        uint32_t oui_bits_2_9 : 8; /*!< Organizationally Unique Identifier(OUI) bits 3 to 10 */
        uint32_t oui_bits_10_17 : 8; /*!< Organizationally Unique Identifier(OUI) bits 11 to 18 */
    };
    uint32_t val;
} lan86xx_phyidr1_reg_t;
#define ETH_PHY_IDR1_REG_ADDR (0x02)

typedef union {
    struct {
        uint32_t model_revision : 4; /*!< Model revision number */
        uint32_t vendor_model : 6;   /*!< Vendor model number */
        uint32_t oui_bits_18_23 : 6; /*!< Organizationally Unique Identifier(OUI) bits 19 to 24 */
    };
    uint32_t val;
} lan86xx_phyidr2_reg_t;
#define ETH_PHY_IDR2_REG_ADDR (0x03)

typedef union {
    struct {
        uint32_t reserved1 : 14;    // Reserved
        uint32_t rst : 1;           // PLCA Reset
        uint32_t en : 1;            // PLCA Enable
        uint16_t padding1;          // Padding
    };
    uint32_t val;
} lan86xx_plca_ctrl0_reg_t;
#define ETH_PHY_PLCA_CTRL0_REG_MMD_ADDR (0xCA01)

typedef union {
    struct {
        uint8_t id;         // PLCA ID
        uint8_t ncnt;       // Node count
        uint16_t padding1;  // Padding
    };
    uint32_t val;
} lan86xx_plca_ctrl1_reg_t;
#define ETH_PHY_PLCA_CTRL1_REG_MMD_ADDR (0xCA02)

typedef union {
    struct {
        uint8_t totmr;      // Transmit Opportunity Timer
        uint8_t reserved1;  // Reserved
        uint16_t padding1;  // Padding
    };
    uint32_t val;
} lan86xx_plca_totmr_reg_t;
#define ETH_PHY_PLCA_TOTMR_REG_MMD_ADDR (0xCA04)

typedef union {
    struct {
        uint8_t btmr;       // Burst timer
        uint8_t maxbc;      // Maximum burst count
        uint16_t padding1;  // Padding
    };
    uint32_t val;
} lan86xx_plca_burst_reg_t;
#define ETH_PHY_PLCA_BURST_REG_MMD_ADDR (0xCA05)

typedef union {
    struct {
        uint8_t entries[2];
    };
    uint32_t val;
} lan86xx_plca_multiple_id_reg_t;
#define ETH_PHY_PLCA_MULTID_BASE_MMD_ADDR (0x0030)

typedef struct {
    phy_802_3_t phy_802_3;
} phy_lan86xx_t;

#define MISC_REGISTERS_DEVICE   0x1f

/***********Custom functions implementations***********/
esp_err_t esp_eth_phy_lan86xx_read_oui(phy_802_3_t *phy_802_3, uint32_t *oui)
{
    esp_err_t ret = ESP_OK;
    lan86xx_phyidr1_reg_t id1;
    lan86xx_phyidr2_reg_t id2;
    esp_eth_mediator_t *eth = phy_802_3->eth;

    ESP_GOTO_ON_FALSE(oui != NULL, ESP_ERR_INVALID_ARG, err, TAG, "oui can't be null");

    ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, phy_802_3->addr, ETH_PHY_IDR1_REG_ADDR, &(id1.val)), err, TAG, "read ID1 failed");
    ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, phy_802_3->addr, ETH_PHY_IDR2_REG_ADDR, &(id2.val)), err, TAG, "read ID2 failed");

    *oui = (id2.oui_bits_18_23 << 18) + ((id1.oui_bits_10_17 << 10) + (id1.oui_bits_2_9 << 2));
    return ESP_OK;
err:
    return ret;
}

static esp_err_t lan86xx_update_link_duplex_speed(phy_lan86xx_t *lan86xx)
{
    esp_err_t ret = ESP_OK;
    esp_eth_mediator_t *eth = lan86xx->phy_802_3.eth;
    uint32_t addr = lan86xx->phy_802_3.addr;
    bmcr_reg_t bmcr;
    bmsr_reg_t bmsr;
    ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, addr, ETH_PHY_BMCR_REG_ADDR, &(bmcr.val)), err, TAG, "read BMCR failed");
    ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, addr, ETH_PHY_BMSR_REG_ADDR, &(bmsr.val)), err, TAG, "read BMSR failed");
    eth_speed_t speed = bmcr.speed_select ? ETH_SPEED_100M : ETH_SPEED_10M;
    eth_duplex_t duplex;
    if (bmcr.en_loopback) {
        // if loopback enabled, we need to falsely indicate full duplex to the EMAC to be able to Tx and Rx at the same time
        duplex = ETH_DUPLEX_FULL;
    } else {
        duplex = bmcr.duplex_mode  ? ETH_DUPLEX_FULL : ETH_DUPLEX_HALF;
    }
    eth_link_t link = bmsr.link_status ? ETH_LINK_UP : ETH_LINK_DOWN;
    // This will be ran exactly once, when everything is setting up
    if (lan86xx->phy_802_3.link_status != link) {
        ESP_GOTO_ON_ERROR(eth->on_state_changed(eth, ETH_STATE_SPEED, (void *)speed), err, TAG, "change speed failed");
        ESP_GOTO_ON_ERROR(eth->on_state_changed(eth, ETH_STATE_DUPLEX, (void *)duplex), err, TAG, "change duplex failed");
        ESP_GOTO_ON_ERROR(eth->on_state_changed(eth, ETH_STATE_LINK, (void *)link), err, TAG, "change link failed");
        lan86xx->phy_802_3.link_status = link;
    }
    return ESP_OK;
err:
    return ret;
}

static esp_err_t lan86xx_get_link(esp_eth_phy_t *phy)
{
    esp_err_t ret = ESP_OK;
    phy_lan86xx_t *lan86xx = __containerof(esp_eth_phy_into_phy_802_3(phy), phy_lan86xx_t, phy_802_3);

    /* Update information about link, speed, duplex */
    ESP_GOTO_ON_ERROR(lan86xx_update_link_duplex_speed(lan86xx), err, TAG, "update link duplex speed failed");
    return ESP_OK;
err:
    return ret;
}

// Software reset of PHY module of LAN865x is not recommended
static esp_err_t lan865x_reset(esp_eth_phy_t *phy)
{
    ESP_LOGW(TAG, "Software reset of PHY module of LAN865x not performed as it is not recommended");
    return ESP_OK;
}

static esp_err_t lan86xx_init(esp_eth_phy_t *phy)
{
    esp_err_t ret = ESP_OK;
    phy_802_3_t *phy_802_3 = esp_eth_phy_into_phy_802_3(phy);

    /* Basic PHY init */
    ESP_GOTO_ON_ERROR(esp_eth_phy_802_3_basic_phy_init(phy_802_3), err, TAG, "failed to init PHY");
    /* Check PHY ID */
    uint32_t oui;
    uint8_t model;
    ESP_GOTO_ON_ERROR(esp_eth_phy_lan86xx_read_oui(phy_802_3, &oui), err, TAG, "read OUI failed");
    ESP_GOTO_ON_ERROR(esp_eth_phy_802_3_read_manufac_info(phy_802_3, &model, NULL), err, TAG, "read manufacturer's info failed");
    ESP_GOTO_ON_FALSE(oui == LAN86XX_OUI, ESP_FAIL, err, TAG, "wrong chip OUI %lx (expected %x)", oui, LAN86XX_OUI);

    bool supported_model = false;
    for (unsigned int i = 0; i < sizeof(supported_models); i++) {
        if (model == supported_models[i]) {
            supported_model = true;
            if (model == LAN865X_MODEL_NUM) {
                phy_802_3->parent.reset = lan865x_reset;
            }
            break;
        }
    }
    ESP_GOTO_ON_FALSE(supported_model, ESP_FAIL, err, TAG, "unsupported chip model %x", model);
    return ESP_OK;
err:
    return ret;
}

static esp_err_t lan86xx_autonego_ctrl(esp_eth_phy_t *phy, eth_phy_autoneg_cmd_t cmd, bool *autonego_en_stat)
{
    switch (cmd) {
    case ESP_ETH_PHY_AUTONEGO_RESTART:
    // Fallthrough
    case ESP_ETH_PHY_AUTONEGO_EN:
    // Fallthrough
    case ESP_ETH_PHY_AUTONEGO_DIS:
        return ESP_ERR_NOT_SUPPORTED; // no autonegotiation operations are supported
    case ESP_ETH_PHY_AUTONEGO_G_STAT:
        // since autonegotiation is not supported it is always indicated disabled
        *autonego_en_stat = false;
        break;
    default:
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}

static esp_err_t lan86xx_advertise_pause_ability(esp_eth_phy_t *phy, uint32_t ability)
{
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t lan86xx_set_speed(esp_eth_phy_t *phy, eth_speed_t speed)
{
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t lan86xx_set_duplex(esp_eth_phy_t *phy, eth_duplex_t duplex)
{
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t lan86xx_custom_ioctl(esp_eth_phy_t *phy, int cmd, void *data)
{
    esp_err_t ret;
    phy_802_3_t *phy_802_3 = esp_eth_phy_into_phy_802_3(phy);
    esp_eth_mediator_t *eth = phy_802_3->eth;
    lan86xx_plca_ctrl0_reg_t plca_ctrl0;
    lan86xx_plca_ctrl1_reg_t plca_ctrl1;
    lan86xx_plca_totmr_reg_t plca_totmr;
    lan86xx_plca_burst_reg_t plca_burst_reg;
    lan86xx_plca_multiple_id_reg_t plca_multiple_id_reg;
    switch (cmd) {
    case LAN86XX_ETH_CMD_S_EN_PLCA:
        // check if loopback is enabled if user wants to enable PLCA
        bool plca_en = *(bool *)data;
        if (plca_en) {
            bmcr_reg_t bmcr;
            ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, phy_802_3->addr, ETH_PHY_BMCR_REG_ADDR, &(bmcr.val)), err, TAG, "read BMCR failed");
            ESP_GOTO_ON_FALSE(bmcr.en_loopback == false, ESP_ERR_INVALID_STATE, err, TAG, "PLCA can't be enabled at the same time as loopback");
        }
        ESP_GOTO_ON_ERROR(esp_eth_phy_802_3_read_mmd_register(phy_802_3, MISC_REGISTERS_DEVICE, ETH_PHY_PLCA_CTRL0_REG_MMD_ADDR, &plca_ctrl0.val), err, TAG, "read PLCA_CTRL0 failed");
        plca_ctrl0.en = (*(bool *)data != false); // anything but 0 will be regarded as true
        ESP_GOTO_ON_ERROR(esp_eth_phy_802_3_write_mmd_register(phy_802_3, MISC_REGISTERS_DEVICE, ETH_PHY_PLCA_CTRL0_REG_MMD_ADDR, plca_ctrl0.val), err, TAG, "write PLCA_CTRL0 failed");
        break;
    case LAN86XX_ETH_CMD_G_EN_PLCA:
        ESP_GOTO_ON_ERROR(esp_eth_phy_802_3_read_mmd_register(phy_802_3, MISC_REGISTERS_DEVICE, ETH_PHY_PLCA_CTRL0_REG_MMD_ADDR, &plca_ctrl0.val), err, TAG, "read PLCA_CTRL0 failed");
        *((bool *)data) = plca_ctrl0.en;
        break;
    case LAN86XX_ETH_CMD_S_PLCA_NCNT:
        ESP_GOTO_ON_ERROR(esp_eth_phy_802_3_read_mmd_register(phy_802_3, MISC_REGISTERS_DEVICE, ETH_PHY_PLCA_CTRL1_REG_MMD_ADDR, &plca_ctrl1.val), err, TAG, "read PLCA_CTRL1 failed");
        plca_ctrl1.ncnt = *((uint8_t *) data);
        ESP_GOTO_ON_ERROR(esp_eth_phy_802_3_write_mmd_register(phy_802_3, MISC_REGISTERS_DEVICE, ETH_PHY_PLCA_CTRL1_REG_MMD_ADDR, plca_ctrl1.val), err, TAG, "write PLCA_CTRL1 failed");
        break;
    case LAN86XX_ETH_CMD_G_PLCA_NCNT:
        ESP_GOTO_ON_ERROR(esp_eth_phy_802_3_read_mmd_register(phy_802_3, MISC_REGISTERS_DEVICE, ETH_PHY_PLCA_CTRL1_REG_MMD_ADDR, &plca_ctrl1.val), err, TAG, "read PLCA_CTRL1 failed");
        *((uint8_t *) data) = plca_ctrl1.ncnt;
        break;
    case LAN86XX_ETH_CMD_S_PLCA_ID:
        ESP_GOTO_ON_ERROR(esp_eth_phy_802_3_read_mmd_register(phy_802_3, MISC_REGISTERS_DEVICE, ETH_PHY_PLCA_CTRL1_REG_MMD_ADDR, &plca_ctrl1.val), err, TAG, "read PLCA_CTRL1 failed");
        plca_ctrl1.id = *((uint8_t *) data);
        ESP_GOTO_ON_ERROR(esp_eth_phy_802_3_write_mmd_register(phy_802_3, MISC_REGISTERS_DEVICE, ETH_PHY_PLCA_CTRL1_REG_MMD_ADDR, plca_ctrl1.val), err, TAG, "write PLCA_CTRL1 failed");
        break;
    case LAN86XX_ETH_CMD_G_PLCA_ID:
        ESP_GOTO_ON_ERROR(esp_eth_phy_802_3_read_mmd_register(phy_802_3, MISC_REGISTERS_DEVICE, ETH_PHY_PLCA_CTRL1_REG_MMD_ADDR, &plca_ctrl1.val), err, TAG, "read PLCA_CTRL1 failed");
        *((uint8_t *) data) = plca_ctrl1.id;
        break;
    case LAN86XX_ETH_CMD_S_PLCA_TOT:
        ESP_GOTO_ON_ERROR(esp_eth_phy_802_3_read_mmd_register(phy_802_3, MISC_REGISTERS_DEVICE, ETH_PHY_PLCA_TOTMR_REG_MMD_ADDR, &plca_totmr.val), err, TAG, "read PLCA_TOTMR failed");
        plca_totmr.totmr = *((uint8_t *) data);
        ESP_GOTO_ON_ERROR(esp_eth_phy_802_3_write_mmd_register(phy_802_3, MISC_REGISTERS_DEVICE, ETH_PHY_PLCA_TOTMR_REG_MMD_ADDR, plca_totmr.val), err, TAG, "write PLCA_TOTMR failed");
        break;
    case LAN86XX_ETH_CMD_G_PLCA_TOT:
        ESP_GOTO_ON_ERROR(esp_eth_phy_802_3_read_mmd_register(phy_802_3, MISC_REGISTERS_DEVICE, ETH_PHY_PLCA_TOTMR_REG_MMD_ADDR, &plca_totmr.val), err, TAG, "read PLCA_TOTMR failed");
        *((uint8_t *) data) = plca_totmr.totmr;
        break;
    case LAN86XX_ETH_CMD_PLCA_RST:
        ESP_GOTO_ON_ERROR(esp_eth_phy_802_3_read_mmd_register(phy_802_3, MISC_REGISTERS_DEVICE, ETH_PHY_PLCA_CTRL0_REG_MMD_ADDR, &plca_ctrl0.val), err, TAG, "read PLCA_CTRL0 failed");
        plca_ctrl0.rst = true;
        ESP_GOTO_ON_ERROR(esp_eth_phy_802_3_write_mmd_register(phy_802_3, MISC_REGISTERS_DEVICE, ETH_PHY_PLCA_CTRL0_REG_MMD_ADDR, plca_ctrl0.val), err, TAG, "write PLCA_CTRL0 failed");
        break;
    case LAN86XX_ETH_CMD_ADD_TX_OPPORTUNITY:
        // Transmit opportunities are stored in four registers
        // Additional transmit opportunity is assigned if value is not 0x00 or 0xff
        // So the algorithm is to find first 0x00 or 0xff and replace with id
        for (uint16_t i = 0; i < 4; i++) {
            ESP_GOTO_ON_ERROR(esp_eth_phy_802_3_read_mmd_register(phy_802_3, MISC_REGISTERS_DEVICE, ETH_PHY_PLCA_MULTID_BASE_MMD_ADDR + i, &plca_multiple_id_reg.val), err, TAG, "read MULTID%d failed", i);
            for (uint8_t j = 0; j < 2; j++) {
                if (plca_multiple_id_reg.entries[j] == 0x00 || plca_multiple_id_reg.entries[j] == 0xff) {
                    plca_multiple_id_reg.entries[j] = *((uint8_t *) data);
                    ESP_GOTO_ON_ERROR(esp_eth_phy_802_3_write_mmd_register(phy_802_3, MISC_REGISTERS_DEVICE, ETH_PHY_PLCA_MULTID_BASE_MMD_ADDR + i, plca_multiple_id_reg.val), err, TAG, "write MULTID%d failed", i);
                    return ESP_OK;
                }
            }
        }
        ESP_GOTO_ON_FALSE(false, ESP_ERR_NO_MEM, err, TAG, "Unable to add additional transmit opportunity for 0x%02x. Maximum amount (8) reached.", *((uint8_t *) data));
        break;
    case LAN86XX_ETH_CMD_RM_TX_OPPORTUNITY:
        // Look for the first occurrence of id and replace it with 0x00
        for (uint16_t i = 0; i < 4; i++) {
            ESP_GOTO_ON_ERROR(esp_eth_phy_802_3_read_mmd_register(phy_802_3, MISC_REGISTERS_DEVICE, ETH_PHY_PLCA_MULTID_BASE_MMD_ADDR + i, &plca_multiple_id_reg.val), err, TAG, "read MULTID%d failed", i);
            for (uint8_t j = 0; j < 2; j++) {
                if (plca_multiple_id_reg.entries[j] == *((uint8_t *) data)) {
                    plca_multiple_id_reg.entries[j] = 0x00;
                    ESP_GOTO_ON_ERROR(esp_eth_phy_802_3_write_mmd_register(phy_802_3, MISC_REGISTERS_DEVICE, ETH_PHY_PLCA_MULTID_BASE_MMD_ADDR + i, plca_multiple_id_reg.val), err, TAG, "write MULTID%d failed", i);
                    return ESP_OK;
                }
            }
        }
        ESP_GOTO_ON_FALSE(false, ESP_ERR_NOT_FOUND, err, TAG, "Unable to remove additional transmit opportunity for 0x%02x since it doesn't have one already.", *((uint8_t *) data));
        break;
    case LAN86XX_ETH_CMD_S_MAX_BURST_COUNT:
        ESP_GOTO_ON_ERROR(esp_eth_phy_802_3_read_mmd_register(phy_802_3, MISC_REGISTERS_DEVICE, ETH_PHY_PLCA_BURST_REG_MMD_ADDR, &plca_burst_reg.val), err, TAG, "read PLCA_BURST failed");
        plca_burst_reg.maxbc = *((uint8_t *) data);
        ESP_GOTO_ON_ERROR(esp_eth_phy_802_3_write_mmd_register(phy_802_3, MISC_REGISTERS_DEVICE, ETH_PHY_PLCA_BURST_REG_MMD_ADDR, plca_burst_reg.val), err, TAG, "write PLCA_BURST failed");
        break;
    case LAN86XX_ETH_CMD_G_MAX_BURST_COUNT:
        ESP_GOTO_ON_ERROR(esp_eth_phy_802_3_read_mmd_register(phy_802_3, MISC_REGISTERS_DEVICE, ETH_PHY_PLCA_BURST_REG_MMD_ADDR, &plca_burst_reg.val), err, TAG, "read PLCA_BURST failed");
        *((uint8_t *) data) = plca_burst_reg.maxbc;
        break;
    case LAN86XX_ETH_CMD_S_BURST_TIMER:
        ESP_GOTO_ON_ERROR(esp_eth_phy_802_3_read_mmd_register(phy_802_3, MISC_REGISTERS_DEVICE, ETH_PHY_PLCA_BURST_REG_MMD_ADDR, &plca_burst_reg.val), err, TAG, "read PLCA_BURST failed");
        plca_burst_reg.btmr = *((uint8_t *) data);
        ESP_GOTO_ON_ERROR(esp_eth_phy_802_3_write_mmd_register(phy_802_3, MISC_REGISTERS_DEVICE, ETH_PHY_PLCA_BURST_REG_MMD_ADDR, plca_burst_reg.val), err, TAG, "write PLCA_BURST failed");
        break;
    case LAN86XX_ETH_CMD_G_BURST_TIMER:
        ESP_GOTO_ON_ERROR(esp_eth_phy_802_3_read_mmd_register(phy_802_3, MISC_REGISTERS_DEVICE, ETH_PHY_PLCA_BURST_REG_MMD_ADDR, &plca_burst_reg.val), err, TAG, "read PLCA_BURST failed");
        *((uint8_t *) data) = plca_burst_reg.btmr;
        break;
    default:
        ret = ESP_ERR_INVALID_ARG;
        break;
    }
    return ESP_OK;
err:
    return ret;
}

static esp_err_t lan86xx_loopback(esp_eth_phy_t *phy, bool enable)
{
    esp_err_t ret = ESP_OK;
    phy_802_3_t *phy_802_3 = esp_eth_phy_into_phy_802_3(phy);
    // For loopback to work PLCA must be disabled
    bool plca_status = false;
    lan86xx_custom_ioctl(phy, LAN86XX_ETH_CMD_G_EN_PLCA, &plca_status);
    ESP_GOTO_ON_FALSE(plca_status == false, ESP_ERR_INVALID_STATE, err, TAG, "Unable to set loopback while PLCA is enabled. Disable it to use loopback");
    return esp_eth_phy_802_3_loopback(phy_802_3, enable);
err:
    return ret;
}

esp_eth_phy_t *esp_eth_phy_new_lan86xx(const eth_phy_config_t *config)
{
    esp_eth_phy_t *ret = NULL;
    phy_lan86xx_t *lan86xx = calloc(1, sizeof(phy_lan86xx_t));
    ESP_GOTO_ON_FALSE(lan86xx, NULL, err, TAG, "calloc lan86xx failed");
    ESP_GOTO_ON_FALSE(esp_eth_phy_802_3_obj_config_init(&lan86xx->phy_802_3, config) == ESP_OK,
                      NULL, err, TAG, "configuration initialization of PHY 802.3 failed");

    // redefine functions which need to be customized for sake of lan86xx
    lan86xx->phy_802_3.parent.init = lan86xx_init;
    lan86xx->phy_802_3.parent.get_link = lan86xx_get_link;
    lan86xx->phy_802_3.parent.autonego_ctrl = lan86xx_autonego_ctrl;
    lan86xx->phy_802_3.parent.set_speed = lan86xx_set_speed;
    lan86xx->phy_802_3.parent.set_duplex = lan86xx_set_duplex;
    lan86xx->phy_802_3.parent.loopback = lan86xx_loopback;
    lan86xx->phy_802_3.parent.custom_ioctl = lan86xx_custom_ioctl;
    lan86xx->phy_802_3.parent.advertise_pause_ability = lan86xx_advertise_pause_ability;

    return &lan86xx->phy_802_3.parent;
err:
    free(lan86xx);
    return ret;
}
