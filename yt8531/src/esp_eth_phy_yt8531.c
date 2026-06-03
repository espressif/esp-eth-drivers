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
#include "esp_eth_phy_yt8531.h"

static const char *TAG = "yt8531";

/* Hardware reset must be held low for at least 10 ms (datasheet section 5.1) */
#define YT8531_PHY_RESET_ASSERTION_TIME_MS 10
/* Allow enough time for the PHY to complete its internal initialisation */
#define YT8531_PHY_POST_RESET_INIT_TIME_MS 30

/*
 * OUI = (PHY_ID1 << 6) | (PHY_ID2[15:10])
 *     = (0x4F51 << 6) | 0x3A
 *     = 0x13D47A
 * Motorcomm Electronic Technology Co., Ltd.
 */
#define YT8531_PHY_OUI 0x13D47A

/***************Vendor Specific Registers***************/

/*
 * Extended register access is performed indirectly through two standard MII
 * registers:
 *   0x1E - Extended Register Address (write the 16-bit EXT address here)
 *   0x1F - Extended Register Data    (read/write the register value here)
 *
 * The common (chip-global) EXT registers live at addresses 0xA000+.
 * The UTP-specific EXT registers live at addresses 0x00-0xFF.
 *
 * Accessing the standard MII register space (0x00-0x1F) targets either the
 * UTP or SerDes register bank depending on SMI_SDS_PHY (EXT_0xA000) bit[1].
 * For the typical UTP-to-RGMII application this bit defaults to 0 (UTP), but
 * the driver explicitly sets it during init to be safe.
 */
#define YT8531_EXT_ADDR_REG   (0x1E)
#define YT8531_EXT_DATA_REG   (0x1F)

/* Common (chip-global) EXT register addresses */
#define YT8531_EXT_SMI_SDS_PHY    (0xA000) /*!< UTP/SDS bank select */
#define YT8531_EXT_CHIP_CONFIG    (0xA001) /*!< Chip configuration */
#define YT8531_EXT_RGMII_CONFIG1  (0xA003) /*!< RGMII TX/RX delay configuration */
#define YT8531_EXT_MISC_CONFIG    (0xA006) /*!< Miscellaneous configuration */

/**
 * @brief SMI_SDS_PHY register (EXT_0xA000)
 */
typedef union {
    struct {
        uint32_t reserved0      : 1;
        uint32_t smi_sds_phy    : 1; /*!< 0 = access UTP registers; 1 = access SDS registers */
        uint32_t reserved2_15   : 14;
    };
    uint32_t val;
} smi_sds_phy_reg_t;

/**
 * @brief Chip_Config register (EXT_0xA001)
 */
typedef union {
    struct {
        uint32_t mode_sel          : 3;  /*!< Chip operation mode (strapping) */
        uint32_t reserved3         : 1;
        uint32_t cfg_ldo           : 2;  /*!< RGMII I/O LDO voltage */
        uint32_t en_ldo            : 1;  /*!< LDO enable */
        uint32_t reserved7         : 1;
        uint32_t rxc_dly_en        : 1;  /*!< 1 = add ~2 ns delay on RX_CLK */
        uint32_t en_gate_rx_clk    : 1;  /*!< 1 = gate RXC when link is down */
        uint32_t reserved10        : 1;
        uint32_t iddq_mode         : 1;  /*!< IDDQ test mode */
        uint32_t reserved12_14     : 3;
        uint32_t sw_rst_n_mode     : 1;  /*!< Whole-chip software reset (SC, active low) */
    };
    uint32_t val;
} chip_config_reg_t;

/**
 * @brief RGMII_Config1 register (EXT_0xA003)
 */
typedef union {
    struct {
        uint32_t tx_delay_sel      : 4;  /*!< TX delay at 1000 Mbps, ~150 ps/step */
        uint32_t tx_delay_sel_fe   : 4;  /*!< TX delay at 100/10 Mbps, ~150 ps/step */
        uint32_t en_rgmii_crs      : 1;
        uint32_t en_rgmii_fd_crs   : 1;
        uint32_t rx_delay_sel      : 4;  /*!< RX clock fine delay, ~150 ps/step */
        uint32_t tx_clk_sel        : 1;
        uint32_t rgmac_cfg_mode    : 1;
    };
    uint32_t val;
} rgmii_config1_reg_t;

/**
 * @brief Misc_Config register (EXT_0xA006)
 *
 * Datasheet table 18:
 * - bit[5] Rem_lpbk_phy: remote loopback for UTP
 * - bit[4] Uldata_rloopback: keep upload data when remote loopback is enabled
 */
typedef union {
    struct {
        uint32_t reserved0_2       : 3;
        uint32_t bp_gmii_fatal_rst : 1;
        uint32_t uldata_rloopback  : 1;
        uint32_t rem_lpbk_phy      : 1;
        uint32_t reserved6         : 1;
        uint32_t jumbo_enable      : 1;
        uint32_t reserved8_15      : 8;
    };
    uint32_t val;
} misc_config_reg_t;

/**
 * @brief PHY Specific Status Register (MII 0x11)
 *
 * Speed and duplex are only valid when bit[11] (speed_duplex_resolved) is set.
 * bit[10] is a real-time (non-latched) link status indicator.
 */
typedef union {
    struct {
        uint32_t jabber_rt          : 1;  /*!< Jabber real-time */
        uint32_t polarity_rt        : 1;  /*!< Polarity real-time (1 = reversed) */
        uint32_t rx_pause           : 1;  /*!< Receive pause enabled */
        uint32_t tx_pause           : 1;  /*!< Transmit pause enabled */
        uint32_t reserved4          : 1;
        uint32_t wirespeed_dg       : 1;  /*!< Wirespeed downgrade */
        uint32_t mdi_crossover      : 1;  /*!< MDI crossover status */
        uint32_t reserved7_9        : 3;
        uint32_t link_status_rt     : 1;  /*!< Real-time link status */
        uint32_t speed_duplex_res   : 1;  /*!< Speed and duplex resolved */
        uint32_t page_received      : 1;
        uint32_t duplex             : 1;  /*!< 1 = full-duplex */
        uint32_t speed_mode         : 2;  /*!< 00=10M, 01=100M, 10=1000M, 11=reserved */
    };
    uint32_t val;
} phy_specific_status_reg_t;
#define YT8531_PHY_SPECIFIC_STATUS_REG_ADDR (0x11)

typedef struct {
    phy_802_3_t phy_802_3;
} phy_yt8531_t;

/* ---------------------------------------------------------------------------
 * Extended register helpers
 * --------------------------------------------------------------------------- */

static esp_err_t yt8531_ext_reg_write(phy_yt8531_t *yt8531, uint16_t ext_addr, uint32_t val)
{
    esp_err_t ret = ESP_OK;
    esp_eth_mediator_t *eth = yt8531->phy_802_3.eth;
    uint32_t phy_addr = yt8531->phy_802_3.addr;

    ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, phy_addr, YT8531_EXT_ADDR_REG, ext_addr),
                      err, TAG, "write EXT addr reg failed");
    ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, phy_addr, YT8531_EXT_DATA_REG, val),
                      err, TAG, "write EXT data reg failed");
    return ESP_OK;
err:
    return ret;
}

static esp_err_t yt8531_ext_reg_read(phy_yt8531_t *yt8531, uint16_t ext_addr, uint32_t *val)
{
    esp_err_t ret = ESP_OK;
    esp_eth_mediator_t *eth = yt8531->phy_802_3.eth;
    uint32_t phy_addr = yt8531->phy_802_3.addr;

    ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, phy_addr, YT8531_EXT_ADDR_REG, ext_addr),
                      err, TAG, "write EXT addr reg failed");
    ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, phy_addr, YT8531_EXT_DATA_REG, val),
                      err, TAG, "read EXT data reg failed");
    return ESP_OK;
err:
    return ret;
}

/**
 * @brief Select the UTP register bank for standard MII register access.
 *
 * YT8531 can expose either UTP or SDS registers on addresses 0x00-0x1F
 * depending on SMI_SDS_PHY (EXT_0xA000) bit[1]. In the standard
 * UTP-to-RGMII use-case we always want UTP (bit[1] = 0).
 */
static esp_err_t yt8531_select_utp_regs(phy_yt8531_t *yt8531)
{
    esp_err_t ret = ESP_OK;
    smi_sds_phy_reg_t reg;

    ESP_GOTO_ON_ERROR(yt8531_ext_reg_read(yt8531, YT8531_EXT_SMI_SDS_PHY, &reg.val),
                      err, TAG, "read SMI_SDS_PHY failed");
    if (reg.smi_sds_phy != 0) {
        reg.smi_sds_phy = 0;
        ESP_GOTO_ON_ERROR(yt8531_ext_reg_write(yt8531, YT8531_EXT_SMI_SDS_PHY, reg.val),
                          err, TAG, "write SMI_SDS_PHY failed");
    }
    return ESP_OK;
err:
    return ret;
}

/* ---------------------------------------------------------------------------
 * Auto-negotiation / loopback
 * --------------------------------------------------------------------------- */

static esp_err_t yt8531_autonego_ctrl(esp_eth_phy_t *phy, eth_phy_autoneg_cmd_t cmd, bool *autonego_en_stat)
{
    esp_err_t ret = ESP_OK;
    phy_802_3_t *phy_802_3 = esp_eth_phy_into_phy_802_3(phy);
    esp_eth_mediator_t *eth = phy_802_3->eth;
    if (cmd == ESP_ETH_PHY_AUTONEGO_EN) {
        bmcr_reg_t bmcr;
        ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, phy_802_3->addr, ETH_PHY_BMCR_REG_ADDR, &(bmcr.val)), err, TAG, "read BMCR failed");
        ESP_GOTO_ON_FALSE(bmcr.en_loopback == 0, ESP_ERR_INVALID_STATE, err, TAG, "Autonegotiation can't be enabled while in loopback operation");
    }
    return esp_eth_phy_802_3_autonego_ctrl(phy_802_3, cmd, autonego_en_stat);
err:
    return ret;
}

static esp_err_t yt8531_loopback(esp_eth_phy_t *phy, bool enable)
{
    esp_err_t ret = ESP_OK;
    phy_802_3_t *phy_802_3 = esp_eth_phy_into_phy_802_3(phy);
    bool auto_nego_en = true;
    ESP_GOTO_ON_ERROR(yt8531_autonego_ctrl(phy, ESP_ETH_PHY_AUTONEGO_G_STAT, &auto_nego_en), err, TAG, "get status of autonegotiation failed");
    ESP_GOTO_ON_FALSE(!(auto_nego_en && enable), ESP_ERR_INVALID_STATE, err, TAG, "Unable to set loopback while autonegotiation is enabled. Disable it to use loopback");
    return esp_eth_phy_802_3_loopback(phy_802_3, enable);
err:
    return ret;
}

/* ---------------------------------------------------------------------------
 * Link / speed / duplex
 * --------------------------------------------------------------------------- */

static esp_err_t yt8531_update_link_duplex_speed(phy_yt8531_t *yt8531)
{
    esp_err_t ret = ESP_OK;
    esp_eth_mediator_t *eth = yt8531->phy_802_3.eth;
    uint32_t addr = yt8531->phy_802_3.addr;
    eth_speed_t speed = ETH_SPEED_10M;
    eth_duplex_t duplex = ETH_DUPLEX_HALF;
    uint32_t peer_pause_ability = false;
    bmsr_reg_t bmsr;
    anlpar_reg_t anlpar;
    phy_specific_status_reg_t pssr;

    ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, addr, ETH_PHY_BMSR_REG_ADDR, &(bmsr.val)),
                      err, TAG, "read BMSR failed");
    ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, addr, ETH_PHY_ANLPAR_REG_ADDR, &(anlpar.val)),
                      err, TAG, "read ANLPAR failed");
    ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, addr, YT8531_PHY_SPECIFIC_STATUS_REG_ADDR, &(pssr.val)),
                      err, TAG, "read PHY Specific Status failed");

    eth_link_t link = bmsr.link_status ? ETH_LINK_UP : ETH_LINK_DOWN;

    if (yt8531->phy_802_3.link_status != link) {
        if (link == ETH_LINK_UP) {
            if (pssr.speed_duplex_res) {
                switch (pssr.speed_mode) {
                case 0:
                    speed = ETH_SPEED_10M;
                    break;
                case 1:
                    speed = ETH_SPEED_100M;
                    break;
                case 2:
                    speed = ETH_SPEED_1000M;
                    break;
                default:
                    ESP_LOGW(TAG, "unexpected speed_mode value 0x%x", pssr.speed_mode);
                    break;
                }
                duplex = pssr.duplex ? ETH_DUPLEX_FULL : ETH_DUPLEX_HALF;
            }
            ESP_GOTO_ON_ERROR(eth->on_state_changed(eth, ETH_STATE_SPEED, (void *)speed),
                              err, TAG, "change speed failed");
            ESP_GOTO_ON_ERROR(eth->on_state_changed(eth, ETH_STATE_DUPLEX, (void *)duplex),
                              err, TAG, "change duplex failed");
            if (duplex == ETH_DUPLEX_FULL && anlpar.symmetric_pause) {
                peer_pause_ability = 1;
            }
            ESP_GOTO_ON_ERROR(eth->on_state_changed(eth, ETH_STATE_PAUSE, (void *)peer_pause_ability),
                              err, TAG, "change pause ability failed");
        }
        ESP_GOTO_ON_ERROR(eth->on_state_changed(eth, ETH_STATE_LINK, (void *)link),
                          err, TAG, "change link failed");
        yt8531->phy_802_3.link_status = link;
    }
    return ESP_OK;
err:
    return ret;
}

static esp_err_t yt8531_get_link(esp_eth_phy_t *phy)
{
    esp_err_t ret = ESP_OK;
    phy_yt8531_t *yt8531 = __containerof(esp_eth_phy_into_phy_802_3(phy), phy_yt8531_t, phy_802_3);

    ESP_GOTO_ON_ERROR(yt8531_update_link_duplex_speed(yt8531), err, TAG,
                      "update link duplex speed failed");
    return ESP_OK;
err:
    return ret;
}

/* ---------------------------------------------------------------------------
 * Custom IOCTL
 * --------------------------------------------------------------------------- */

static esp_err_t yt8531_custom_ioctl(esp_eth_phy_t *phy, int cmd, void *data)
{
    esp_err_t ret = ESP_OK;
    phy_yt8531_t *yt8531 = __containerof(esp_eth_phy_into_phy_802_3(phy), phy_yt8531_t, phy_802_3);

    switch (cmd) {
    case YT8531_ETH_CMD_S_RGMII_CLK_DELAY: {
        ESP_GOTO_ON_FALSE(data != NULL, ESP_ERR_INVALID_ARG, err, TAG, "data can't be null");
        yt8531_rgmii_clk_delay_config_t *cfg = (yt8531_rgmii_clk_delay_config_t *)data;
        ESP_GOTO_ON_FALSE(cfg->rx_delay_sel    <= 0xF, ESP_ERR_INVALID_ARG, err, TAG,
                          "rx_delay_sel out of range (max 15)");
        ESP_GOTO_ON_FALSE(cfg->tx_delay_sel    <= 0xF, ESP_ERR_INVALID_ARG, err, TAG,
                          "tx_delay_sel out of range (max 15)");
        ESP_GOTO_ON_FALSE(cfg->tx_delay_sel_fe <= 0xF, ESP_ERR_INVALID_ARG, err, TAG,
                          "tx_delay_sel_fe out of range (max 15)");

        /* Update rxc_dly_en in Chip_Config (EXT_0xA001) bit[8] */
        chip_config_reg_t chip_cfg;
        ESP_GOTO_ON_ERROR(yt8531_ext_reg_read(yt8531, YT8531_EXT_CHIP_CONFIG, &chip_cfg.val),
                          err, TAG, "read Chip_Config failed");
        chip_cfg.rxc_dly_en = cfg->rxc_dly_en ? 1 : 0;
        ESP_GOTO_ON_ERROR(yt8531_ext_reg_write(yt8531, YT8531_EXT_CHIP_CONFIG, chip_cfg.val),
                          err, TAG, "write Chip_Config failed");

        /* Update TX/RX fine delay in RGMII_Config1 (EXT_0xA003) */
        rgmii_config1_reg_t rgmii_cfg1;
        ESP_GOTO_ON_ERROR(yt8531_ext_reg_read(yt8531, YT8531_EXT_RGMII_CONFIG1, &rgmii_cfg1.val),
                          err, TAG, "read RGMII_Config1 failed");
        rgmii_cfg1.rx_delay_sel    = cfg->rx_delay_sel;
        rgmii_cfg1.tx_delay_sel    = cfg->tx_delay_sel;
        rgmii_cfg1.tx_delay_sel_fe = cfg->tx_delay_sel_fe;
        ESP_GOTO_ON_ERROR(yt8531_ext_reg_write(yt8531, YT8531_EXT_RGMII_CONFIG1, rgmii_cfg1.val),
                          err, TAG, "write RGMII_Config1 failed");
        break;
    }
    case YT8531_ETH_CMD_S_FAREND_LOOPBACK: {
        ESP_GOTO_ON_FALSE(data != NULL, ESP_ERR_INVALID_ARG, err, TAG, "data can't be null");
        bool *enable = (bool *)data;
        misc_config_reg_t misc_cfg;
        ESP_GOTO_ON_ERROR(yt8531_ext_reg_read(yt8531, YT8531_EXT_MISC_CONFIG, &misc_cfg.val),
                          err, TAG, "read Misc_Config failed");
        misc_cfg.rem_lpbk_phy = (*enable != false);
        ESP_GOTO_ON_ERROR(yt8531_ext_reg_write(yt8531, YT8531_EXT_MISC_CONFIG, misc_cfg.val),
                          err, TAG, "write Misc_Config failed");
        break;
    }
    default:
        ret = ESP_ERR_INVALID_ARG;
        break;
    }
    return ret;
err:
    return ret;
}

/* ---------------------------------------------------------------------------
 * Initialisation
 * --------------------------------------------------------------------------- */

static esp_err_t yt8531_init(esp_eth_phy_t *phy)
{
    esp_err_t ret = ESP_OK;
    phy_802_3_t *phy_802_3 = esp_eth_phy_into_phy_802_3(phy);
    phy_yt8531_t *yt8531 = __containerof(phy_802_3, phy_yt8531_t, phy_802_3);

    if (phy_802_3->addr == ESP_ETH_PHY_ADDR_AUTO) {
        ESP_GOTO_ON_ERROR(esp_eth_phy_802_3_detect_phy_addr(phy_802_3->eth, &phy_802_3->addr),
                          err, TAG, "Detect PHY address failed");
    }

    ESP_GOTO_ON_ERROR(esp_eth_phy_802_3_basic_phy_init(phy_802_3), err, TAG, "failed to init PHY");

    uint32_t oui;
    uint8_t model;
    ESP_GOTO_ON_ERROR(esp_eth_phy_802_3_read_oui(phy_802_3, &oui), err, TAG, "read OUI failed");
    ESP_GOTO_ON_ERROR(esp_eth_phy_802_3_read_manufac_info(phy_802_3, &model, NULL),
                      err, TAG, "read manufacturer info failed");
    ESP_GOTO_ON_FALSE(oui == YT8531_PHY_OUI, ESP_FAIL, err, TAG,
                      "wrong chip OUI (read 0x%" PRIx32 ", model 0x%" PRIx8 ")", oui, model);

    /* Ensure standard MII register space maps to UTP registers, not SDS */
    ESP_GOTO_ON_ERROR(yt8531_select_utp_regs(yt8531), err, TAG, "select UTP regs failed");

    /* Re-enable auto-negotiation which was disabled by PHY reset (deviation to IEEE 802.3 standard) */
    bool autonego_en_stat;
    ESP_GOTO_ON_ERROR(yt8531_autonego_ctrl(phy, ESP_ETH_PHY_AUTONEGO_EN, &autonego_en_stat),
                      err, TAG, "enable auto-negotiation failed");
    ESP_GOTO_ON_FALSE(autonego_en_stat, ESP_FAIL, err, TAG, "auto-negotiation is not enabled");
    return ESP_OK;
err:
    return ret;
}

/* ---------------------------------------------------------------------------
 * Constructor
 * --------------------------------------------------------------------------- */

esp_eth_phy_t *esp_eth_phy_new_yt8531(const eth_phy_config_t *config)
{
    esp_eth_phy_t *ret = NULL;
    phy_yt8531_t *yt8531 = calloc(1, sizeof(phy_yt8531_t));
    ESP_GOTO_ON_FALSE(yt8531, NULL, err, TAG, "calloc yt8531 failed");
    ESP_LOGI(TAG, "esp_eth_phy_new_yt8531");
    eth_phy_config_t yt8531_config = *config;
    if (config->hw_reset_assert_time_us == 0) {
        /* Convert ms to us: datasheet requires ≥10 ms hardware reset pulse */
        yt8531_config.hw_reset_assert_time_us = YT8531_PHY_RESET_ASSERTION_TIME_MS * 1000;
    }
    if (config->post_hw_reset_delay_ms == 0) {
        yt8531_config.post_hw_reset_delay_ms = YT8531_PHY_POST_RESET_INIT_TIME_MS;
    }

    ESP_GOTO_ON_FALSE(esp_eth_phy_802_3_obj_config_init(&yt8531->phy_802_3, &yt8531_config) == ESP_OK,
                      NULL, err, TAG, "configuration initialization of PHY 802.3 failed");

    yt8531->phy_802_3.parent.init         = yt8531_init;
    yt8531->phy_802_3.parent.get_link     = yt8531_get_link;
    yt8531->phy_802_3.parent.autonego_ctrl = yt8531_autonego_ctrl;
    yt8531->phy_802_3.parent.loopback     = yt8531_loopback;
    yt8531->phy_802_3.parent.custom_ioctl = yt8531_custom_ioctl;

    return &yt8531->phy_802_3.parent;
err:
    if (yt8531 != NULL) {
        free(yt8531);
    }
    return ret;
}
