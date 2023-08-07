/*
 * SPDX-FileCopyrightText: 2019-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include <stdlib.h>
#include <sys/cdefs.h>
#include "esp_log.h"
#include "esp_check.h"
#include "esp_eth_phy_802_3.h"
#include "esp_eth_phy_dp83640.h"
#include "esp_eth_phy_dp83640_struct.h"

static const char *TAG = "dp83640";

/* Helper macro for selecting register page */
#define DP83640_SET_PAGE(pg) ESP_RETURN_ON_ERROR(eth->phy_reg_write(eth, addr, DP83640_REG_ADDR(pagesel), pg), TAG, "Select page "#pg": %d failed", pg);

/* Helper macro for writing/reading registers on different pages */
#define _DP83640_WR_PAGE_REG_VAL(pg, reg, val)  \
        ESP_RETURN_ON_ERROR(eth->phy_reg_write(eth, addr, DP83640_REG_ADDR(page##pg.reg), val), TAG, "Write "#reg" register failed")
#define _DP83640_RD_PAGE_REG_VAL(pg, reg, val)  \
        ESP_RETURN_ON_ERROR(eth->phy_reg_read(eth, addr, DP83640_REG_ADDR(page##pg.reg), &(val)), TAG, "Read "#reg" register failed")
#define DP83640_WR_PAGE_REG_VAL(pg, reg, val)   _DP83640_WR_PAGE_REG_VAL(pg, reg, val)
#define DP83640_RD_PAGE_REG_VAL(pg, reg, val)   _DP83640_RD_PAGE_REG_VAL(pg, reg, val)
#define DP83640_WR_PAGE_REG(pg, reg)            DP83640_WR_PAGE_REG_VAL(pg, reg, (reg).val)
#define DP83640_RD_PAGE_REG(pg, reg)            DP83640_RD_PAGE_REG_VAL(pg, reg, (reg).val)

/* Helper macro for writing/reading base registers */
#define DP83640_WR_BASE_REG(reg)    ESP_RETURN_ON_ERROR(eth->phy_reg_write(eth, addr, DP83640_REG_ADDR(reg), (reg).val), TAG, "Write "#reg" register failed")
#define DP83640_RD_BASE_REG(reg)    ESP_RETURN_ON_ERROR(eth->phy_reg_read(eth, addr, DP83640_REG_ADDR(reg), &((reg).val)), TAG, "Read "#reg" register failed")

// TODO: move out when 1588 protocol ported, extract into the ieee1588 header
typedef struct {

} phy_1588_t;

typedef struct phy_dp83640_s {
    phy_802_3_t phy_802_3;
    phy_1588_t phy_1588;
    uint32_t last_duration;
} phy_dp83640_t;

static esp_err_t dp83640_update_link_duplex_speed(dp83640_handle_t dp83640)
{
    esp_err_t ret = ESP_OK;
    esp_eth_mediator_t *eth = dp83640->phy_802_3.eth;
    uint32_t addr = dp83640->phy_802_3.addr;
    eth_speed_t speed = ETH_SPEED_10M;
    eth_duplex_t duplex = ETH_DUPLEX_HALF;
    uint32_t peer_pause_ability = false;
    dp83640_anlpar_reg_t anlpar;
    dp83640_physts_reg_t physts;
    DP83640_RD_BASE_REG(anlpar);
    DP83640_RD_BASE_REG(physts);
    eth_link_t link = physts.link_status ? ETH_LINK_UP : ETH_LINK_DOWN;
    /* check if link status changed */
    if (dp83640->phy_802_3.link_status != link) {
        /* when link up, read negotiation result */
        if (link == ETH_LINK_UP) {
            if (physts.speed_status) {
                speed = ETH_SPEED_10M;
            } else {
                speed = ETH_SPEED_100M;
            }
            if (physts.duplex_status) {
                duplex = ETH_DUPLEX_FULL;
            } else {
                duplex = ETH_DUPLEX_HALF;
            }
            ESP_GOTO_ON_ERROR(eth->on_state_changed(eth, ETH_STATE_SPEED, (void *)speed), err, TAG, "change speed failed");
            ESP_GOTO_ON_ERROR(eth->on_state_changed(eth, ETH_STATE_DUPLEX, (void *)duplex), err, TAG, "change duplex failed");
            /* if we're in duplex mode, and peer has the flow control ability */
            if (duplex == ETH_DUPLEX_FULL && anlpar.pause) {
                peer_pause_ability = 1;
            } else {
                peer_pause_ability = 0;
            }
            ESP_GOTO_ON_ERROR(eth->on_state_changed(eth, ETH_STATE_PAUSE, (void *)peer_pause_ability), err, TAG, "change pause ability failed");
        }
        ESP_GOTO_ON_ERROR(eth->on_state_changed(eth, ETH_STATE_LINK, (void *)link), err, TAG, "change link failed");
        dp83640->phy_802_3.link_status = link;
    }
    return ESP_OK;
err:
    return ret;
}

static esp_err_t dp83640_get_link(esp_eth_phy_t *phy)
{
    esp_err_t ret = ESP_OK;
    dp83640_handle_t dp83640 = __containerof(esp_eth_phy_into_phy_802_3(phy), phy_dp83640_t, phy_802_3);
    /* Update information about link, speed, duplex */
    ESP_GOTO_ON_ERROR(dp83640_update_link_duplex_speed(dp83640), err, TAG, "update link duplex speed failed");
    return ESP_OK;
err:
    return ret;
}

static esp_err_t dp83640_init(esp_eth_phy_t *phy)
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
    ESP_GOTO_ON_FALSE(oui == 0x80017 && model == 0x0E, ESP_FAIL, err, TAG, "wrong chip ID");

    return ESP_OK;
err:
    return ret;
}

esp_eth_phy_t *esp_eth_phy_new_dp83640(const eth_phy_config_t *config)
{
    esp_eth_phy_t *ret = NULL;
    dp83640_handle_t dp83640 = calloc(1, sizeof(phy_dp83640_t));
    ESP_GOTO_ON_FALSE(dp83640, NULL, err, TAG, "calloc dp83640 failed");
    ESP_GOTO_ON_FALSE(esp_eth_phy_802_3_obj_config_init(&dp83640->phy_802_3, config) == ESP_OK,
                      NULL, err, TAG, "configuration initialization of PHY 802.3 failed");

    // redefine functions which need to be customized for sake of dp83640
    dp83640->phy_802_3.parent.init = dp83640_init;
    dp83640->phy_802_3.parent.get_link = dp83640_get_link;

    return &dp83640->phy_802_3.parent;
err:
    if (dp83640 != NULL) {
        free(dp83640);
    }
    return ret;
}

/*************************** PTP Specific Operations **************************/

esp_err_t dp83640_ptp_enable(dp83640_handle_t dp83640, bool enable)
{
    esp_eth_mediator_t *eth = dp83640->phy_802_3.eth;
    uint32_t addr = dp83640->phy_802_3.addr;

    DP83640_SET_PAGE(PTP1588_BASE_PAGE);

    dp83640_ptp_ctl_reg_t ptp_ctl = {};
    DP83640_RD_PAGE_REG(PTP1588_BASE_PAGE, ptp_ctl);
    ptp_ctl.enable = enable;
    ptp_ctl.disable = !enable;
    DP83640_WR_PAGE_REG(PTP1588_BASE_PAGE, ptp_ctl);

    return ESP_OK;
}

esp_err_t dp83640_ptp_reset(dp83640_handle_t dp83640)
{
    esp_eth_mediator_t *eth = dp83640->phy_802_3.eth;
    uint32_t addr = dp83640->phy_802_3.addr;

    DP83640_SET_PAGE(PTP1588_BASE_PAGE);

    dp83640_ptp_ctl_reg_t ptp_ctl = {};
    DP83640_RD_PAGE_REG(PTP1588_BASE_PAGE, ptp_ctl);
    ptp_ctl.reset = 1;
    DP83640_WR_PAGE_REG(PTP1588_BASE_PAGE, ptp_ctl);
    ptp_ctl.reset = 0;
    DP83640_WR_PAGE_REG(PTP1588_BASE_PAGE, ptp_ctl);

    return ESP_OK;
}

esp_err_t dp83640_ptp_set_time(dp83640_handle_t dp83640, uint32_t sec, uint32_t nano_sec)
{
    esp_eth_mediator_t *eth = dp83640->phy_802_3.eth;
    uint32_t addr = dp83640->phy_802_3.addr;

    /* Select register page for the next register writing or reading */
    DP83640_SET_PAGE(PTP1588_BASE_PAGE);

    /* Set PTP times */
    DP83640_WR_PAGE_REG_VAL(PTP1588_BASE_PAGE, ptp_tdr, nano_sec & 0xFFFF);
    DP83640_WR_PAGE_REG_VAL(PTP1588_BASE_PAGE, ptp_tdr, nano_sec >> 16);
    DP83640_WR_PAGE_REG_VAL(PTP1588_BASE_PAGE, ptp_tdr, sec & 0xFFFF);
    DP83640_WR_PAGE_REG_VAL(PTP1588_BASE_PAGE, ptp_tdr, sec >> 16);

    /* Set load_clk bit to synchronize the time that set just now */
    dp83640_ptp_ctl_reg_t ptp_ctl = {};
    DP83640_RD_PAGE_REG(PTP1588_BASE_PAGE, ptp_ctl);
    ptp_ctl.load_clk = 1;
    DP83640_WR_PAGE_REG(PTP1588_BASE_PAGE, ptp_ctl);

    return ESP_OK;
}

esp_err_t dp83640_ptp_get_time(dp83640_handle_t dp83640, uint32_t *sec, uint32_t *nano_sec)
{
    esp_eth_mediator_t *eth = dp83640->phy_802_3.eth;
    uint32_t addr = dp83640->phy_802_3.addr;

    DP83640_SET_PAGE(PTP1588_BASE_PAGE);
    dp83640_ptp_ctl_reg_t ptp_ctl = {};
    DP83640_RD_PAGE_REG(PTP1588_BASE_PAGE, ptp_ctl);
    ptp_ctl.rd_clk = 1;
    DP83640_WR_PAGE_REG(PTP1588_BASE_PAGE, ptp_ctl);

    uint32_t nsec_l = 0;
    uint32_t nsec_h = 0;
    uint32_t sec_l = 0;
    uint32_t sec_h = 0;
    DP83640_RD_PAGE_REG_VAL(PTP1588_BASE_PAGE, ptp_tdr, nsec_l);
    DP83640_RD_PAGE_REG_VAL(PTP1588_BASE_PAGE, ptp_tdr, nsec_h);
    DP83640_RD_PAGE_REG_VAL(PTP1588_BASE_PAGE, ptp_tdr, sec_l);
    DP83640_RD_PAGE_REG_VAL(PTP1588_BASE_PAGE, ptp_tdr, sec_h);

    if (sec) {
        *sec = sec_h << 16 | sec_l;
    }
    if (nano_sec) {
        *nano_sec = nsec_h << 16 | nsec_l;
    }

    return ESP_OK;
}

#define DP83640_ADJUSTMENT_COMPENSATION_NS      16

esp_err_t dp83640_ptp_adjust_time(dp83640_handle_t dp83640, int32_t sec, int32_t nano_sec)
{
    esp_eth_mediator_t *eth = dp83640->phy_802_3.eth;
    uint32_t addr = dp83640->phy_802_3.addr;

    // Both seconds and nanoseconds fields should be 32-bit 2’s complement representations
    //  The addition process is a pipelined process that takes two clock cycles at 8 ns each at the default clock rate
    nano_sec += DP83640_ADJUSTMENT_COMPENSATION_NS;

    DP83640_SET_PAGE(PTP1588_BASE_PAGE);

    DP83640_WR_PAGE_REG_VAL(PTP1588_BASE_PAGE, ptp_tdr, nano_sec & 0xFFFF);
    DP83640_WR_PAGE_REG_VAL(PTP1588_BASE_PAGE, ptp_tdr, nano_sec >> 16);
    DP83640_WR_PAGE_REG_VAL(PTP1588_BASE_PAGE, ptp_tdr, sec & 0xFFFF);
    DP83640_WR_PAGE_REG_VAL(PTP1588_BASE_PAGE, ptp_tdr, sec >> 16);

    dp83640_ptp_ctl_reg_t ptp_ctl = {};
    DP83640_RD_PAGE_REG(PTP1588_BASE_PAGE, ptp_ctl);
    ptp_ctl.step_clk = 1;
    DP83640_WR_PAGE_REG(PTP1588_BASE_PAGE, ptp_ctl);

    return ESP_OK;
}

static esp_err_t s_dp83640_ptp_set_rate(dp83640_handle_t dp83640, uint32_t rate, bool is_temp, bool dir)
{
    esp_eth_mediator_t *eth = dp83640->phy_802_3.eth;
    uint32_t addr = dp83640->phy_802_3.addr;

    DP83640_SET_PAGE(PTP1588_BASE_PAGE);

    dp83640_ptp_rateh_reg_t ptp_rateh = {};
    DP83640_RD_PAGE_REG(PTP1588_BASE_PAGE, ptp_rateh);
    ptp_rateh.rate_high = rate >> 16;
    ptp_rateh.rate_dir = dir;
    ptp_rateh.is_tmp_rate = is_temp;
    DP83640_WR_PAGE_REG(PTP1588_BASE_PAGE, ptp_rateh);

    dp83640_ptp_ratel_reg_t ptp_ratel = {
        .rate_low = rate & 0xFF,
    };
    DP83640_WR_PAGE_REG(PTP1588_BASE_PAGE, ptp_ratel);

    return ESP_OK;
}

esp_err_t dp83640_ptp_set_normal_rate(dp83640_handle_t dp83640, uint32_t rate, bool dir)
{
    return s_dp83640_ptp_set_rate(dp83640, rate, false, dir);
}

esp_err_t dp83640_ptp_set_tmp_rate(dp83640_handle_t dp83640, uint32_t rate, uint32_t duration, bool dir)
{
    esp_eth_mediator_t *eth = dp83640->phy_802_3.eth;
    uint32_t addr = dp83640->phy_802_3.addr;

    if (dp83640->last_duration != duration) {
        DP83640_SET_PAGE(PTP1588_CFG1_PAGE);
        DP83640_WR_PAGE_REG_VAL(PTP1588_CFG1_PAGE, ptp_trdh, duration >> 16);
        DP83640_WR_PAGE_REG_VAL(PTP1588_CFG1_PAGE, ptp_trdl, duration & 0xFF);
        dp83640->last_duration = duration;
    }

    return s_dp83640_ptp_set_rate(dp83640, rate, true, dir);
}

esp_err_t dp83640_ptp_get_tx_timestamp(dp83640_handle_t dp83640, uint32_t *sec, uint32_t *nano_sec, uint32_t *ovf_cnt)
{
    esp_eth_mediator_t *eth = dp83640->phy_802_3.eth;
    uint32_t addr = dp83640->phy_802_3.addr;

    DP83640_SET_PAGE(PTP1588_BASE_PAGE);

    uint32_t nsec_l = 0;
    uint32_t nsec_h = 0;
    uint32_t sec_l = 0;
    uint32_t sec_h = 0;
    DP83640_RD_PAGE_REG_VAL(PTP1588_BASE_PAGE, ptp_txts, nsec_l);
    DP83640_RD_PAGE_REG_VAL(PTP1588_BASE_PAGE, ptp_txts, nsec_h);
    DP83640_RD_PAGE_REG_VAL(PTP1588_BASE_PAGE, ptp_txts, sec_l);
    DP83640_RD_PAGE_REG_VAL(PTP1588_BASE_PAGE, ptp_txts, sec_h);
    // The highest 2 bits indicate overflow count of tx timestamp queue, will stick to 3 if more timestamps overflowed
    if (ovf_cnt) {
        *ovf_cnt = nsec_h >> 14;
    }
    if (sec) {
        *sec = sec_h << 16 | sec_l;
    }
    if (nano_sec) {
        *nano_sec = (nsec_h & 0x3FFF) << 16 | nsec_l;
    }

    return ESP_OK;
}

esp_err_t dp83640_ptp_get_rx_timestamp(dp83640_handle_t dp83640, uint32_t *sec, uint32_t *nano_sec, uint32_t *ovf_cnt,
                                       uint32_t *sequence_id, uint8_t *msg_type, uint32_t *src_hash)
{
    esp_eth_mediator_t *eth = dp83640->phy_802_3.eth;
    uint32_t addr = dp83640->phy_802_3.addr;

    DP83640_SET_PAGE(PTP1588_BASE_PAGE);

    uint32_t nsec_l = 0;
    uint32_t nsec_h = 0;
    uint32_t sec_l = 0;
    uint32_t sec_h = 0;
    uint32_t seq_id = 0;
    uint32_t msg_info = 0;
    DP83640_RD_PAGE_REG_VAL(PTP1588_BASE_PAGE, ptp_rxts, nsec_l);
    DP83640_RD_PAGE_REG_VAL(PTP1588_BASE_PAGE, ptp_rxts, nsec_h);
    DP83640_RD_PAGE_REG_VAL(PTP1588_BASE_PAGE, ptp_rxts, sec_l);
    DP83640_RD_PAGE_REG_VAL(PTP1588_BASE_PAGE, ptp_rxts, sec_h);
    DP83640_RD_PAGE_REG_VAL(PTP1588_BASE_PAGE, ptp_rxts, seq_id);
    DP83640_RD_PAGE_REG_VAL(PTP1588_BASE_PAGE, ptp_rxts, msg_info);

    if (ovf_cnt) {
        *ovf_cnt = nsec_h >> 14;
    }
    if (sec) {
        *sec = sec_h << 16 | sec_l;
    }
    if (nano_sec) {
        *nano_sec = (nsec_h & 0x3FFF) << 16 | nsec_l;
    }
    if (sequence_id) {
        *sequence_id = seq_id;
    }
    if (msg_type) {
        *msg_type = msg_info >> 12;
    }
    if (src_hash) {
        *src_hash = msg_info & 0x0FFF;
    }
    return ESP_OK;
}

esp_err_t dp83640_ptp_set_tx_config (dp83640_handle_t dp83640, const dp83640_tx_config_t *tx_cfg)
{
    esp_eth_mediator_t *eth = dp83640->phy_802_3.eth;
    uint32_t addr = dp83640->phy_802_3.addr;

    DP83640_SET_PAGE(PTP1588_CFG1_PAGE);

    dp83640_ptp_txcfg0_reg_t ptp_txcfg0 = {
        .tx_ts_en = tx_cfg->flags.timestamp,
        .tx_ptp_ver = tx_cfg->ptp_version,
        .tx_ipv4_en = tx_cfg->flags.ipv4_ts,
        .tx_ipv6_en = tx_cfg->flags.ipv6_ts,
        .tx_l2_en = tx_cfg->flags.l2_ts,
        .ip1588_en = tx_cfg->flags.ip1588_filter,
        .chk_1step = tx_cfg->flags.chk_1step,
        .crc_1step = tx_cfg->flags.crc_1step,
        .ignore_2step = tx_cfg->flags.ignore_2step,
        .ntp_ts_en = tx_cfg->flags.ntp_ts,
        .dr_insert = tx_cfg->flags.dr_insert,
        .sync_1step = tx_cfg->flags.sync_1step,
    };
    DP83640_WR_PAGE_REG(PTP1588_CFG1_PAGE, ptp_txcfg0);

    return ESP_OK;
}

esp_err_t dp83640_ptp_set_tx_first_byte_filter(dp83640_handle_t dp83640, uint8_t mask, uint8_t data)
{
    esp_eth_mediator_t *eth = dp83640->phy_802_3.eth;
    uint32_t addr = dp83640->phy_802_3.addr;

    DP83640_SET_PAGE(PTP1588_CFG1_PAGE);

    dp83640_ptp_txcfg1_reg_t ptp_txcfg1 = {
        .byte0_mask = mask,
        .byte0_data = data,
    };
    DP83640_WR_PAGE_REG(PTP1588_CFG1_PAGE, ptp_txcfg1);

    return ESP_OK;
}

esp_err_t dp83640_ptp_set_rx_config(dp83640_handle_t dp83640, const dp83640_ptp_rx_config_t *rx_cfg)
{
    esp_eth_mediator_t *eth = dp83640->phy_802_3.eth;
    uint32_t addr = dp83640->phy_802_3.addr;

    DP83640_SET_PAGE(PTP1588_CFG1_PAGE);

    dp83640_ptp_rxcfg0_reg_t ptp_rxcfg0 = {
        .rx_ts_en = rx_cfg->flags.timestamp,
        .rx_ptp_ver = rx_cfg->ptp_version,
        .rx_ipv4_en = rx_cfg->flags.ipv4_ts,
        .rx_ipv6_en = rx_cfg->flags.ipv6_ts,
        .rx_l2_en = rx_cfg->flags.l2_ts,
        .ip1588_en = rx_cfg->ptp_ip_filter_mask,
        .rx_slave = rx_cfg->flags.slave,
        .alt_mast_dis = rx_cfg->flags.no_alt_mst,
        .domain_en = rx_cfg->flags.domain,
    };
    DP83640_WR_PAGE_REG(PTP1588_CFG1_PAGE, ptp_rxcfg0);

    dp83640_ptp_rxcfg3_reg_t ptp_rxcfg3 = {};
    DP83640_RD_PAGE_REG(PTP1588_CFG1_PAGE, ptp_rxcfg3);
    ptp_rxcfg3.ptp_domain = rx_cfg->ptp_domain;
    DP83640_WR_PAGE_REG(PTP1588_CFG1_PAGE, ptp_rxcfg3);

    return ESP_OK;
}

esp_err_t dp83640_ptp_set_rx_usr_ip_filter(dp83640_handle_t dp83640, uint32_t usr_ip)
{
    esp_eth_mediator_t *eth = dp83640->phy_802_3.eth;
    uint32_t addr = dp83640->phy_802_3.addr;

    DP83640_SET_PAGE(PTP1588_CFG1_PAGE);

    dp83640_ptp_rxcfg0_reg_t ptp_rxcfg0 = {};
    DP83640_RD_PAGE_REG(PTP1588_CFG1_PAGE, ptp_rxcfg0);
    ptp_rxcfg0.user_ip_en = 1;
    ptp_rxcfg0.user_ip_sel = 0;
    DP83640_WR_PAGE_REG(PTP1588_CFG1_PAGE, ptp_rxcfg0);
    DP83640_WR_PAGE_REG_VAL(PTP1588_CFG1_PAGE, ptp_rxcfg2, usr_ip >> 16);
    ptp_rxcfg0.user_ip_sel = 1;
    DP83640_WR_PAGE_REG(PTP1588_CFG1_PAGE, ptp_rxcfg0);
    DP83640_WR_PAGE_REG_VAL(PTP1588_CFG1_PAGE, ptp_rxcfg2, usr_ip & 0xFFFF);

    return ESP_OK;
}

esp_err_t dp83640_ptp_set_rx_first_byte_filter(dp83640_handle_t dp83640, uint8_t mask, uint8_t data)
{
    esp_eth_mediator_t *eth = dp83640->phy_802_3.eth;
    uint32_t addr = dp83640->phy_802_3.addr;

    DP83640_SET_PAGE(PTP1588_CFG1_PAGE);

    dp83640_ptp_rxcfg1_reg_t ptp_rxcfg1 = {
        .byte0_mask = mask,
        .byte0_data = data,
    };
    DP83640_WR_PAGE_REG(PTP1588_CFG1_PAGE, ptp_rxcfg1);

    return ESP_OK;
}

esp_err_t dp83640_ptp_enable_rx_timestamp_insertion(dp83640_handle_t dp83640, const dp83640_rxts_insert_config_t *insert_cfg)
{
    esp_eth_mediator_t *eth = dp83640->phy_802_3.eth;
    uint32_t addr = dp83640->phy_802_3.addr;

    DP83640_SET_PAGE(PTP1588_CFG1_PAGE);

    dp83640_ptp_rxcfg3_reg_t ptp_rxcfg3 = {};
    DP83640_RD_PAGE_REG(PTP1588_CFG1_PAGE, ptp_rxcfg3);
    ptp_rxcfg3.ts_insert = 1;
    ptp_rxcfg3.ts_append = insert_cfg->flags.append_l2_ts;
    ptp_rxcfg3.acc_crc = insert_cfg->flags.rec_crc_err_ts;
    ptp_rxcfg3.acc_udp = insert_cfg->flags.rec_udp_err_checksum_ts;
    ptp_rxcfg3.ts_min_cfg = insert_cfg->ts_min_ifg;
    DP83640_WR_PAGE_REG(PTP1588_CFG1_PAGE, ptp_rxcfg3);

    dp83640_ptp_rxcfg4_reg_t ptp_rxcfg4 = {
        .ipv4_udp_mod = insert_cfg->flags.udp_checksum_update,
        .ts_sec_en = insert_cfg->flags.insert_sec_en,
        .ts_sec_len = insert_cfg->sec_len,
        .rxts_sec_offset = insert_cfg->ts_sec_offset,
        .rxts_nsec_offset = insert_cfg->ts_nsec_offset,
    };
    DP83640_WR_PAGE_REG(PTP1588_CFG1_PAGE, ptp_rxcfg4);

    return ESP_OK;
}

esp_err_t dp83640_ptp_disable_rx_timestamp_insertion(dp83640_handle_t dp83640)
{
    esp_eth_mediator_t *eth = dp83640->phy_802_3.eth;
    uint32_t addr = dp83640->phy_802_3.addr;

    DP83640_SET_PAGE(PTP1588_CFG1_PAGE);

    dp83640_ptp_rxcfg3_reg_t ptp_rxcfg3 = {};
    DP83640_RD_PAGE_REG(PTP1588_CFG1_PAGE, ptp_rxcfg3);
    ptp_rxcfg3.ts_insert = 0;
    DP83640_WR_PAGE_REG(PTP1588_CFG1_PAGE, ptp_rxcfg3);

    return ESP_OK;
}

esp_err_t dp83640_ptp_set_trigger_behavior(dp83640_handle_t dp83640, const dp83640_trig_behavior_config_t *trig_bhv_cfg)
{
    esp_eth_mediator_t *eth = dp83640->phy_802_3.eth;
    uint32_t addr = dp83640->phy_802_3.addr;

    DP83640_SET_PAGE(PTP1588_CFG1_PAGE);

    dp83640_ptp_trig_reg_t ptp_trig = {
        .trig_wr = 1,
        .trig_csel = trig_bhv_cfg->trig_id,
        .trig_gpio = trig_bhv_cfg->trig_phy_gpio,
        .trig_pulse = trig_bhv_cfg->flags.gen_pulse,
        .trig_per = trig_bhv_cfg->flags.periodic,
        .trig_if_late = trig_bhv_cfg->flags.if_late,
        .trig_notify = trig_bhv_cfg->flags.notify,
        .trig_toggle = trig_bhv_cfg->flags.toggle,
    };
    DP83640_WR_PAGE_REG(PTP1588_CFG1_PAGE, ptp_trig);

    return ESP_OK;
}

esp_err_t dp83640_ptp_config_event(dp83640_handle_t dp83640, const dp83640_evt_config_t * evt_cfg)
{
    esp_eth_mediator_t *eth = dp83640->phy_802_3.eth;
    uint32_t addr = dp83640->phy_802_3.addr;

    DP83640_SET_PAGE(PTP1588_CFG1_PAGE);

    dp83640_ptp_evnt_reg_t ptp_evnt = {
        .evnt_wr = 1,
        .evnt_sel = evt_cfg->evt_id,
        .evnt_gpio = evt_cfg->evt_phy_gpio,
        .evnt_single = evt_cfg->flags.single_ent,
        .evnt_fall = evt_cfg->flags.fall_evt,
        .evnt_rise = evt_cfg->flags.rise_evt,
    };
    DP83640_WR_PAGE_REG(PTP1588_CFG1_PAGE, ptp_evnt);

    return ESP_OK;
}

esp_err_t dp83640_ptp_config_misc(dp83640_handle_t dp83640, const dp83640_misc_config_t *misc_cfg)
{
    esp_eth_mediator_t *eth = dp83640->phy_802_3.eth;
    uint32_t addr = dp83640->phy_802_3.addr;

    DP83640_SET_PAGE(PTP1588_CFG2_PAGE);

    DP83640_WR_PAGE_REG_VAL(PTP1588_CFG2_PAGE, ptp_etr, misc_cfg->ptp_eth_type);
    DP83640_WR_PAGE_REG_VAL(PTP1588_CFG2_PAGE, ptp_off, misc_cfg->ptp_offset);

    dp83640_ptp_sfdcfg_reg_t ptp_sfdcfg = {
        .rx_sfd_gpio = misc_cfg->rx_sfd_gpio,
        .tx_sfd_gpio = misc_cfg->tx_sfd_gpio,
    };
    DP83640_WR_PAGE_REG(PTP1588_CFG2_PAGE, ptp_sfdcfg);

    return ESP_OK;
}

esp_err_t dp83640_ptp_set_clk_src(dp83640_handle_t dp83640, dp83640_clk_src_t clk_src, uint32_t period)
{
    esp_eth_mediator_t *eth = dp83640->phy_802_3.eth;
    uint32_t addr = dp83640->phy_802_3.addr;

    DP83640_SET_PAGE(PTP1588_CFG2_PAGE);

    dp83640_ptp_clksrc_reg_t ptp_clksrc = {
        .clk_src = clk_src,
        .clk_src_period = period,
    };
    DP83640_WR_PAGE_REG(PTP1588_CFG2_PAGE, ptp_clksrc);

    return ESP_OK;
}

esp_err_t dp83640_ptp_enable_clock_output(dp83640_handle_t dp83640, const dp83640_out_clk_config_t * out_clk_cfg)
{
    esp_eth_mediator_t *eth = dp83640->phy_802_3.eth;
    uint32_t addr = dp83640->phy_802_3.addr;

    DP83640_SET_PAGE(PTP1588_CFG2_PAGE);

    dp83640_ptp_coc_reg_t ptp_coc = {
        .ptp_clk_div = out_clk_cfg->clk_div,
        .ptp_clk_out_speed_sel = out_clk_cfg->faster_edge_en,
        .ptp_clk_out_sel = out_clk_cfg->out_clk_src,
        .ptp_clk_out_en = 1,
    };
    DP83640_WR_PAGE_REG(PTP1588_CFG2_PAGE, ptp_coc);

    DP83640_SET_PAGE(EXTEND_PAGE);
    dp83640_phycr2_reg_t phycr2 = {};
    DP83640_RD_PAGE_REG(EXTEND_PAGE, phycr2);
    phycr2.clk_out_dis = 0;
    DP83640_WR_PAGE_REG(EXTEND_PAGE, phycr2);

    return ESP_OK;
}

esp_err_t dp83640_ptp_disable_clock_output(dp83640_handle_t dp83640)
{
    esp_eth_mediator_t *eth = dp83640->phy_802_3.eth;
    uint32_t addr = dp83640->phy_802_3.addr;

    DP83640_SET_PAGE(PTP1588_CFG2_PAGE);

    dp83640_ptp_coc_reg_t ptp_coc = {
        .ptp_clk_out_en = 0,
    };
    DP83640_WR_PAGE_REG(PTP1588_CFG2_PAGE, ptp_coc);

    DP83640_SET_PAGE(EXTEND_PAGE);

    dp83640_phycr2_reg_t phycr2 = {};
    DP83640_RD_PAGE_REG(EXTEND_PAGE, phycr2);
    phycr2.clk_out_dis = 1;
    DP83640_WR_PAGE_REG(EXTEND_PAGE, phycr2);
    return ESP_OK;
}


esp_err_t dp83640_ptp_config_intr_gpio(dp83640_handle_t dp83640, uint16_t int_gpio)
{
    esp_eth_mediator_t *eth = dp83640->phy_802_3.eth;
    uint32_t addr = dp83640->phy_802_3.addr;

    DP83640_SET_PAGE(PTP1588_CFG2_PAGE);

    DP83640_WR_PAGE_REG_VAL(PTP1588_CFG2_PAGE, ptp_intctl, int_gpio);

    return ESP_OK;
}

esp_err_t dp83640_ptp_config_psf(dp83640_handle_t dp83640, const dp83640_psf_config_t * psf_cfg)
{
    esp_eth_mediator_t *eth = dp83640->phy_802_3.eth;
    uint32_t addr = dp83640->phy_802_3.addr;
    DP83640_SET_PAGE(PTP1588_CFG1_PAGE);

    // default config
    dp83640_psf_cfg0_reg_t psf_cfg0 = {
        .psf_evnt_en = psf_cfg->flags.event,
        .psf_trig_en =  psf_cfg->flags.trigger,
        .psf_rxts_en =  psf_cfg->flags.rx_ts,
        .psf_txts_en =  psf_cfg->flags.tx_ts,
        .psf_err_en =  psf_cfg->flags.err_en,
        .psf_pcf_rd =  0,
        .psf_ipv4 =  psf_cfg->flags.ipv4_en, // Layer 2 packet
        .psf_endian =  psf_cfg->flags.psf_endian,
        .min_pre =  psf_cfg->min_preamble,
        .mac_src_add = psf_cfg->ptp_mac_addr,
    };
    DP83640_WR_PAGE_REG(PTP1588_CFG1_PAGE, psf_cfg0);

    return ESP_OK;
}

esp_err_t dp83640_ptp_specify_psf_ip(dp83640_handle_t dp83640, uint32_t ip_addr)
{
    esp_eth_mediator_t *eth = dp83640->phy_802_3.eth;
    uint32_t addr = dp83640->phy_802_3.addr;

    DP83640_SET_PAGE(PTP1588_CFG2_PAGE);

    dp83640_psf_cfg2_reg_t psf_cfg2 = {
        .ip_sa_byte0 = ip_addr >> 24,
        .ip_sa_byte1 = (ip_addr >> 16) & 0x00FF,
    };
    DP83640_WR_PAGE_REG(PTP1588_CFG2_PAGE, psf_cfg2);
    dp83640_psf_cfg3_reg_t psf_cfg3 = {
        .ip_sa_byte2 = (ip_addr >> 8) & 0x00FF,
        .ip_sa_byte3 = ip_addr & 0x00FF,
    };
    DP83640_WR_PAGE_REG(PTP1588_CFG2_PAGE, psf_cfg3);

    uint16_t oc_list[6] = { 0x4500, 0x0111, 0xE000, 0x0181, 0x0000, 0x0000 }; // Little Endian
    oc_list[4] = (uint16_t)((psf_cfg3.ip_sa_byte3 << 8) | psf_cfg3.ip_sa_byte2);
    oc_list[5] = (uint16_t)((psf_cfg2.ip_sa_byte1 << 8) | psf_cfg2.ip_sa_byte0);
    uint32_t checksum = 0;
    uint32_t rollover = 0;
    for(int i = 0; i < 6; i++ )
        checksum += oc_list[i];
    while( checksum > 0xFFFF ) {
        rollover = (checksum >> 16);
        checksum = (checksum & 0xFFFF) + rollover;
    }
    DP83640_WR_PAGE_REG_VAL(PTP1588_CFG2_PAGE, psf_cfg4, checksum);

    return ESP_OK;
}

esp_err_t dp83640_ptp_set_ptp_frame_header(dp83640_handle_t dp83640, const dp83640_ptp_frame_header_t *header)
{
    esp_eth_mediator_t *eth = dp83640->phy_802_3.eth;
    uint32_t addr = dp83640->phy_802_3.addr;

    DP83640_SET_PAGE(PTP1588_CFG2_PAGE);
    dp83640_psf_cfg1_reg_t psf_cfg1 = {
        .msg_type = header->msg_type,
        .trans_specific = header->transport_specific,
        .ptp_version = header->ptp_version,
        .ptp_reserved = header->ptp_reserved,
    };
    DP83640_WR_PAGE_REG(PTP1588_CFG2_PAGE, psf_cfg1);

    return ESP_OK;
}

esp_err_t dp83640_ptp_register_trigger(dp83640_handle_t dp83640, const dp83640_trigger_config_t *trig_cfg)
{
    esp_eth_mediator_t *eth = dp83640->phy_802_3.eth;
    uint32_t addr = dp83640->phy_802_3.addr;

    DP83640_SET_PAGE(PTP1588_BASE_PAGE);

    /* Select the trigger in ptp_ctl register */
    dp83640_ptp_ctl_reg_t ptp_ctl = {};
    DP83640_RD_PAGE_REG(PTP1588_BASE_PAGE, ptp_ctl);
    ptp_ctl.trig_sel = trig_cfg->trig_id;
    ptp_ctl.trig_load = 1;
    DP83640_WR_PAGE_REG(PTP1588_BASE_PAGE, ptp_ctl);

    /* Set the time configuration of the trigger */
    DP83640_WR_PAGE_REG_VAL(PTP1588_BASE_PAGE, ptp_tdr, trig_cfg->expire_time_nsec & 0xFFFF);
    uint32_t val = (trig_cfg->expire_time_nsec >> 16) | (trig_cfg->is_init ? 0x80000000 : 0) |
                   (trig_cfg->wait_rollover ? 0x40000000 : 0);
    DP83640_WR_PAGE_REG_VAL(PTP1588_BASE_PAGE, ptp_tdr, val);
    DP83640_WR_PAGE_REG_VAL(PTP1588_BASE_PAGE, ptp_tdr, trig_cfg->expire_time_sec & 0xFFFF);
    DP83640_WR_PAGE_REG_VAL(PTP1588_BASE_PAGE, ptp_tdr, trig_cfg->expire_time_sec >> 16);
    DP83640_WR_PAGE_REG_VAL(PTP1588_BASE_PAGE, ptp_tdr, trig_cfg->pulse_width & 0xFFFF);
    DP83640_WR_PAGE_REG_VAL(PTP1588_BASE_PAGE, ptp_tdr, trig_cfg->pulse_width >> 16);

    if (trig_cfg->trig_id <= 1) {
        DP83640_WR_PAGE_REG_VAL(PTP1588_BASE_PAGE, ptp_tdr, trig_cfg->pulse_width2 & 0xFFFF);
        DP83640_WR_PAGE_REG_VAL(PTP1588_BASE_PAGE, ptp_tdr, trig_cfg->pulse_width2 >> 16);
    }

    /* Update the configuration to the trigger */
    ptp_ctl.trig_en = 1;
    ptp_ctl.trig_load = 0;
    DP83640_WR_PAGE_REG(PTP1588_BASE_PAGE, ptp_ctl);

    return ESP_OK;
}

esp_err_t dp83640_ptp_has_trigger_expired(dp83640_handle_t dp83640, uint32_t trig_id, bool *has_expired)
{
    esp_eth_mediator_t *eth = dp83640->phy_802_3.eth;
    uint32_t addr = dp83640->phy_802_3.addr;

    DP83640_SET_PAGE(PTP1588_BASE_PAGE);

    dp83640_ptp_tsts_reg_t ptp_tsts = {};
    DP83640_RD_PAGE_REG(PTP1588_BASE_PAGE, ptp_tsts);

    if (ptp_tsts.trig[trig_id].error) {
        return ESP_ERR_INVALID_ARG;  // The trigger was registered too late and expired
    }
    if (ptp_tsts.trig[trig_id].active) {
        *has_expired = false;
    } else {
        *has_expired = true;
    }

    return ESP_OK;
}

esp_err_t dp83640_ptp_unregister_trigger(dp83640_handle_t dp83640, uint32_t trig_id)
{
    esp_eth_mediator_t *eth = dp83640->phy_802_3.eth;
    uint32_t addr = dp83640->phy_802_3.addr;

    DP83640_SET_PAGE(PTP1588_BASE_PAGE);

    dp83640_ptp_ctl_reg_t ptp_ctl = {};
    DP83640_RD_PAGE_REG(PTP1588_BASE_PAGE, ptp_ctl);
    ptp_ctl.trig_sel = trig_id;
    ptp_ctl.trig_dis = 1;
    DP83640_WR_PAGE_REG(PTP1588_BASE_PAGE, ptp_ctl);

    return ESP_OK;
}

esp_err_t dp83640_ptp_get_event_status(dp83640_handle_t dp83640, dp83640_event_status_t *evt_status)
{
    esp_eth_mediator_t *eth = dp83640->phy_802_3.eth;
    uint32_t addr = dp83640->phy_802_3.addr;

    DP83640_SET_PAGE(PTP1588_BASE_PAGE);

    dp83640_ptp_sts_reg_t ptp_sts = {};
    DP83640_RD_PAGE_REG(PTP1588_BASE_PAGE, ptp_sts);

    *evt_status = (ptp_sts.val >> 16) & PTP_EVENT_MAX;

    return ESP_OK;
}
esp_err_t dp83640_ptp_get_event(dp83640_handle_t dp83640, uint32_t *event_bits, uint32_t *rise_flags,
                                uint32_t *evt_time_sec, uint32_t *evt_time_nsec, uint32_t *evt_missed)
{
    esp_eth_mediator_t *eth = dp83640->phy_802_3.eth;
    uint32_t addr = dp83640->phy_802_3.addr;

    DP83640_SET_PAGE(PTP1588_BASE_PAGE);

    dp83640_ptp_ests_reg_t ptp_ests = {};
    DP83640_RD_PAGE_REG(PTP1588_BASE_PAGE, ptp_ests);

    *evt_missed = ptp_ests.events_missed;

    /* No event detected */
    if (!ptp_ests.event_det) {
        return ESP_ERR_INVALID_STATE;
    }
    dp83640_ptp_edata_reg_t ptp_edata = {};
    if (ptp_ests.mult_event) {
        /* If multiple events detected, read edata register first to get the events */
        DP83640_RD_PAGE_REG(PTP1588_BASE_PAGE, ptp_edata);
        for (int i = 0; i < 8; i++) {
            if (ptp_edata.evt[i].det) {
                *event_bits |= 1 << i;
                *rise_flags |= ptp_edata.evt[i].rise ? (1 << i) : 0;
            }
        }
    } else {
        /* Otherwise read event_num field directly to get the event id */
        *event_bits |= 1 << ptp_ests.event_num;
        *rise_flags |= ptp_ests.event_rf ? (1 << ptp_ests.event_num) : 0;
    }

    /* Read the edata register four times to get the event timestamp */
    DP83640_RD_PAGE_REG(PTP1588_BASE_PAGE, ptp_edata);
    *evt_time_nsec = ptp_edata.val;
    DP83640_RD_PAGE_REG(PTP1588_BASE_PAGE, ptp_edata);
    *evt_time_nsec |= ptp_edata.val << 16;
    DP83640_RD_PAGE_REG(PTP1588_BASE_PAGE, ptp_edata);
    *evt_time_sec = ptp_edata.val;
    DP83640_RD_PAGE_REG(PTP1588_BASE_PAGE, ptp_edata);
    *evt_time_sec |= ptp_edata.val << 16;

    /* Adjust for pin input delay and edge detection time */
    if ( *evt_time_nsec < PIN_INPUT_DELAY ) {
        if ( *evt_time_sec > 0 ) {
            *evt_time_sec -= 1;
            *evt_time_nsec += ((uint32_t)1e9 - PIN_INPUT_DELAY);
        } else {
            *evt_time_sec = *evt_time_nsec = 0;
        }
    } else {
        *evt_time_nsec -= PIN_INPUT_DELAY;
    }

    return ESP_OK;
}
