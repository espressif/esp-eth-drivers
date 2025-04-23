/*
 * SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// KSZ8863 functions related to Port MAC functionality => hence the name "pmac"

#include <string.h>
#include <stdlib.h>
#include <sys/cdefs.h>
#include <sys/queue.h>
#include "esp_log.h"
#include "esp_check.h"

#include "esp_eth_ksz8863.h"
#include "ksz8863_ctrl_internal.h" // indirect read/write
#include "ksz8863.h" // registers

#define KSZ8863_GLOBAL_INIT_DONE     (1 << 0)

static const char *TAG = "ksz8863_pmac";

typedef struct {
    esp_eth_mac_t parent;
    esp_eth_mediator_t *eth;
    pmac_ksz8863_mode_t mode;
    bool flow_ctrl_enabled;
    int32_t port;
    uint8_t port_reg_offset;
    uint32_t status;
} pmac_ksz8863_t;

struct slist_mac_ksz8863_s {
    pmac_ksz8863_t *mac_ksz8863_info;
    SLIST_ENTRY(slist_mac_ksz8863_s) next;
};

static SLIST_HEAD(slisthead, slist_mac_ksz8863_s) s_mac_ksz8863_head;

/**
 * @brief verify ksz8863 chip ID
 */
static esp_err_t ksz8863_verify_id(pmac_ksz8863_t *pmac)
{
    esp_err_t ret = ESP_OK;
    esp_eth_mediator_t *eth = pmac->eth;

    /* Check PHY ID */
    ksz8863_chipid0_reg_t id0;
    ksz8863_chipid1_reg_t id1;
    ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, 0, KSZ8863_CHIPID0_REG_ADDR, &(id0.val)), err, TAG, "read ID0 failed");
    ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, 0, KSZ8863_CHIPID1_REG_ADDR, &(id1.val)), err, TAG, "read ID1 failed");
    ESP_GOTO_ON_FALSE(id0.family_id == 0x88 && id1.chip_id == 0x03, ESP_FAIL, err, TAG, "wrong chip ID");
    return ESP_OK;
err:
    return ret;
}

/**
 * @brief
 */
static esp_err_t ksz8863_setup_port_defaults(pmac_ksz8863_t *pmac)
{
    esp_err_t ret = ESP_OK;
    esp_eth_mediator_t *eth = pmac->eth;

    if (pmac->mode == KSZ8863_PORT_MODE) {
        // Filter frames with MAC addresses originating from us (typically broadcast frames "looped" back by other switch)
        ksz8863_pcr5_reg_t pcr5;
        ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, 0, KSZ8863_PCR5_BASE_ADDR + pmac->port_reg_offset,
                                            &(pcr5.val)), err, TAG, "read Port Control 5 failed");
        pcr5.filter_maca1_en = 1;
        pcr5.filter_maca2_en = 1;
        ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, 0, KSZ8863_PCR5_BASE_ADDR + pmac->port_reg_offset,
                                             pcr5.val), err, TAG, "write Port Control 5 failed");
    }

    return ESP_OK;
err:
    return ret;
}

static esp_err_t ksz8863_setup_global_defaults(pmac_ksz8863_t *pmac)
{
    esp_err_t ret = ESP_OK;
    esp_eth_mediator_t *eth = pmac->eth;

    // Initialize Global registers only once
    struct slist_mac_ksz8863_s *pmac_instance;
    bool global_init_done = false;
    SLIST_FOREACH(pmac_instance, &s_mac_ksz8863_head, next) {
        if ((pmac_instance->mac_ksz8863_info->status & KSZ8863_GLOBAL_INIT_DONE) == KSZ8863_GLOBAL_INIT_DONE) {
            global_init_done = true;
            break;
        }
    }
    if (global_init_done != true) {
        pmac->status |= KSZ8863_GLOBAL_INIT_DONE;
        // TODO: test if below is true!! It appears like that (base on KSZ status regs.) but should be tested
        // Disable Flow control globally to be able to force it locally on port basis
        ksz8863_gcr1_reg_t gcr1;
        ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, 0, KSZ8863_GCR1_ADDR, &(gcr1.val)), err, TAG, "read GC1 failed");
        gcr1.rx_flow_ctrl_en = 0;
        gcr1.tx_flow_ctrl_en = 0;
        ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, 0, KSZ8863_GCR1_ADDR, gcr1.val), err, TAG, "write GC1 failed");

        // Forward IGMP packets directly to P3 (Host) port
        ksz8863_gcr3_reg_t gcr3;
        ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, 0, KSZ8863_GCR3_ADDR, &(gcr3.val)), err, TAG, "read GC3 failed");
        gcr3.igmp_snoop_en = 1;
        ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, 0, KSZ8863_GCR3_ADDR, gcr3.val), err, TAG, "write GC3 failed");

        if (pmac->mode == KSZ8863_PORT_MODE) {
            // Enable forwarding of frames with unknown DA but do NOT specify any port to forward (it can be set later by "set_promiscuous").
            // This ensures that multicast frames are not forwarded directly between P1 and P2 and so these ports act as endpoints. Otherwise,
            // multicast frames could be looped between P1 and P2 and flood the network if P1 and P2 were connected the same switch
            // (or in presence of redundant path in used network).
            ksz8863_gcr12_reg_t gcr12;
            ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, 0, KSZ8863_GCR12_ADDR, &(gcr12.val)), err, TAG, "read GC12 failed");
            gcr12.unknown_da_to_port_en = 1;
            gcr12.unknown_da_to_port = 0;
            ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, 0, KSZ8863_GCR12_ADDR, gcr12.val), err, TAG, "write GC12 failed");

            // Enable Tail tagging
            ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, 0, KSZ8863_GCR1_ADDR, &(gcr1.val)), err, TAG, "read GC1 failed");
            gcr1.tail_tag_en = 1;
            ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, 0, KSZ8863_GCR1_ADDR, gcr1.val), err, TAG, "write GC1 failed");

            // Broadcast needs to be forwarded to P3 and so P1/P2 act as endpoints (no traffic exchanged between them directly)
            ksz8863_sta_mac_table_t stat_mac_table;
            memset(stat_mac_table.data, 0, sizeof(stat_mac_table));
            stat_mac_table.fwd_ports = KSZ8863_TO_PORT3;
            stat_mac_table.entry_val = 1;
            memset(stat_mac_table.mac_addr, 0xFF, ETH_ADDR_LEN);
            ksz8863_indirect_write(KSZ8863_STA_MAC_TABLE, 0x0, stat_mac_table.data, sizeof(stat_mac_table));
        }
    }

    return ESP_OK;
err:
    return ret;
}

// TODO: investigate if needed (currently seems it no needed, since it is started automatically)
static esp_err_t pmac_ksz8863_start(esp_eth_mac_t *mac)
{
    esp_err_t ret = ESP_OK;
    return ret;
}

/**
 * @brief stop ksz8863: disable interrupt and stop receive
 */
static esp_err_t pmac_ksz8863_stop(esp_eth_mac_t *mac)
{
    esp_err_t ret = ESP_OK;
    return ret;
}

static esp_err_t pmac_ksz8863_set_mediator(esp_eth_mac_t *mac, esp_eth_mediator_t *eth)
{
    esp_err_t ret = ESP_OK;
    ESP_GOTO_ON_FALSE(eth, ESP_ERR_INVALID_ARG, err, TAG, "can't set mac's mediator to null");
    pmac_ksz8863_t *pmac = __containerof(mac, pmac_ksz8863_t, parent);
    pmac->eth = eth;
    return ESP_OK;
err:
    return ret;
}

static esp_err_t pmac_ksz8863_set_addr(esp_eth_mac_t *mac, uint8_t *addr)
{
    esp_err_t ret = ESP_OK;
    ESP_GOTO_ON_FALSE(addr, ESP_ERR_INVALID_ARG, err, TAG, "can't set mac addr to null");
    pmac_ksz8863_t *pmac = __containerof(mac, pmac_ksz8863_t, parent);
    esp_eth_mediator_t *eth = pmac->eth;

    uint32_t base_reg_addr;
    if (pmac->port == 0) {
        base_reg_addr = KSZ8863_MACA1_MSB_ADDR;
    } else {
        base_reg_addr = KSZ8863_MACA2_MSB_ADDR;
    }
    for (int i = 0; i < ETH_ADDR_LEN; i++) {
        // MAC MSB is stored at reg. 147/153, hence is written the first
        eth->phy_reg_write(eth, 0, base_reg_addr - i, addr[i]);
    }
    return ESP_OK;
err:
    return ret;
}

static esp_err_t pmac_ksz8863_get_addr(esp_eth_mac_t *mac, uint8_t *addr)
{
    esp_err_t ret = ESP_OK;
    ESP_GOTO_ON_FALSE(addr, ESP_ERR_INVALID_ARG, err, TAG, "can't copy mac addr to null");
    pmac_ksz8863_t *pmac = __containerof(mac, pmac_ksz8863_t, parent);
    esp_eth_mediator_t *eth = pmac->eth;

    uint32_t base_reg_addr;
    if (pmac->port == 0) {
        base_reg_addr = KSZ8863_MACA1_MSB_ADDR;
    } else {
        base_reg_addr = KSZ8863_MACA2_MSB_ADDR;
    }
    for (int i = 0; i < ETH_ADDR_LEN; i++) {
        eth->phy_reg_read(eth, 0, base_reg_addr - i, (uint32_t *)&addr[i]);
    }

    return ESP_OK;
err:
    return ret;
}

static esp_err_t pmac_ksz8863_set_link(esp_eth_mac_t *mac, eth_link_t link)
{
    // Do nothing, KSZ8863 Port 1/2 MAC is started automatically when link is up
    return ESP_OK;
}

static esp_err_t pmac_ksz8863_set_speed(esp_eth_mac_t *mac, eth_speed_t speed)
{
    // Do nothing, KSZ8863 Port 1/2 MAC speed is set automatically based on its associated PHY settings
    return ESP_OK;
}

static esp_err_t pmac_ksz8863_set_duplex(esp_eth_mac_t *mac, eth_duplex_t duplex)
{
    // Do nothing, KSZ8863 Port 1/2 MAC duplex is set automatically based on its associated PHY settings
    return ESP_OK;
}

static esp_err_t pmac_ksz8863_set_promiscuous(esp_eth_mac_t *mac, bool enable)
{
    esp_err_t ret = ESP_OK;
    pmac_ksz8863_t *pmac = __containerof(mac, pmac_ksz8863_t, parent);
    esp_eth_mediator_t *eth = pmac->eth;

    ESP_GOTO_ON_FALSE(pmac->mode == KSZ8863_PORT_MODE, ESP_ERR_INVALID_STATE, err, TAG, "promiscuous is available only in Port Mode");

    // Forward frames with unknown DA to Port 3 ("promiscuous" as such is not mentioned in datasheet)
    ksz8863_gcr12_reg_t gcr12;
    ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, 0, KSZ8863_GCR12_ADDR, &(gcr12.val)), err, TAG, "read GC12 failed");
    if (enable == true) {
        gcr12.unknown_da_to_port_en = 1;
        gcr12.unknown_da_to_port = KSZ8863_TO_PORT3;
    } else {
        gcr12.unknown_da_to_port_en = 1;
        gcr12.unknown_da_to_port = 0;
    }
    ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, 0, KSZ8863_GCR12_ADDR, gcr12.val), err, TAG, "write GC12 failed");

    ESP_LOGW(TAG, "forwarding frames with unknown DA applies for both P1 and P2 ingress ports"); // TODO: consider better formulation
err:
    return ret;
}

static esp_err_t pmac_ksz8863_enable_flow_ctrl(esp_eth_mac_t *mac, bool enable)
{
    pmac_ksz8863_t *pmac = __containerof(mac, pmac_ksz8863_t, parent);
    pmac->flow_ctrl_enabled = enable;
    return ESP_OK;
}

// TODO: it is not possible to enable Pause when autonegotiation is disabled (because PHY does not check for peer's
// ability). This is probably an issue for other EMAC drivers, investigate how it could be fixed.
static esp_err_t pmac_ksz8863_set_peer_pause_ability(esp_eth_mac_t *mac, uint32_t ability)
{
    esp_err_t ret = ESP_OK;
    pmac_ksz8863_t *pmac = __containerof(mac, pmac_ksz8863_t, parent);
    esp_eth_mediator_t *eth = pmac->eth;
    ksz8863_pcr2_reg_t pcr2;

    if (pmac->port > KSZ8863_PORT_2) {
        ESP_LOGE(TAG, "flow control configuration is not available for Port 3 at MAC");
        return ESP_ERR_INVALID_ARG;
    }

    // When user wants to enable flow control and peer does support the pause function
    // then configure the MAC layer to enable flow control feature
    if (pmac->flow_ctrl_enabled && ability) {
        ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, 0, KSZ8863_PCR2_BASE_ADDR + pmac->port_reg_offset, &(pcr2.val)), err, TAG, "read PCR 2 failed");
        pcr2.force_flow_ctrl = 1;
        ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, 0, KSZ8863_PCR2_BASE_ADDR + pmac->port_reg_offset, pcr2.val), err, TAG, "write PCR 2 failed");
        ESP_LOGD(TAG, "flow control forced for the link");
    } else {
        ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, 0, KSZ8863_PCR2_BASE_ADDR + pmac->port_reg_offset, &(pcr2.val)), err, TAG, "read PCR 2 failed");
        pcr2.force_flow_ctrl = 0;
        ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, 0, KSZ8863_PCR2_BASE_ADDR + pmac->port_reg_offset, pcr2.val), err, TAG, "write PCR 2 failed");
        ESP_LOGD(TAG, "flow control disabled for the link");
    }
    /*
        printf("en %d, ability %d\n", pmac->flow_ctrl_enabled, ability);
        ksz8863_psr1_reg_t psr1;
        ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, 0, KSZ8863_PSR1_BASE_ADDR + pmac->port_reg_offset, &(psr1.val)), err, TAG, "read PSR 1 failed");
        printf("P%d, rx %d, tx %d\n", pmac->port+1, psr1.rx_flow_ctrl_en, psr1.tx_flow_ctrl_en);
        ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, 0, KSZ8863_PSR1_BASE_ADDR + KSZ8863_PORT3_ADDR_OFFSET, &(psr1.val)), err, TAG, "read PSR 1 failed");
        printf("P3, rx %d, tx %d\n", psr1.rx_flow_ctrl_en, psr1.tx_flow_ctrl_en);*/
    return ESP_OK;
err:
    return ret;
}

static esp_err_t pmac_ksz8863_set_mac_tbl(pmac_ksz8863_t *pmac, ksz8863_mac_tbl_info_t *tbls_info)
{
    esp_err_t ret = ESP_OK;
    ESP_GOTO_ON_FALSE(!(pmac->mode == KSZ8863_PORT_MODE && tbls_info->start_entry == 0), ESP_ERR_INVALID_STATE, err, TAG,
                      "static MAC tbl entry 0 cannot be changed in Multi-port Mode");
    for (int i = 0; i < tbls_info->etries_num; i++) {
        ESP_GOTO_ON_ERROR(ksz8863_indirect_write(KSZ8863_STA_MAC_TABLE, tbls_info->start_entry + i, &tbls_info->sta_tbls[i],
                                                 sizeof(ksz8863_sta_mac_table_t)), err, TAG, "failed to write MAC table");
    }
err:
    return ret;
}

static esp_err_t pmac_ksz8863_get_mac_tbl(pmac_ksz8863_t *pmac, ksz8863_indir_access_tbls_t tbl, ksz8863_mac_tbl_info_t *tbls_info)
{
    esp_err_t ret = ESP_OK;
    for (int i = 0; i < tbls_info->etries_num; i++) {
        ESP_GOTO_ON_ERROR(ksz8863_indirect_read(tbl, tbls_info->start_entry + i,
                                                tbl == KSZ8863_STA_MAC_TABLE ? (void *)&tbls_info->sta_tbls[i] : (void *)&tbls_info->dyn_tbls[i],
                                                tbl == KSZ8863_STA_MAC_TABLE ? sizeof(ksz8863_sta_mac_table_t) : sizeof(ksz8863_dyn_mac_table_t)),
                          err, TAG, "failed to read MAC table");
    }
err:
    return ret;
}

static esp_err_t pmac_ksz8863_custom_ioctl(esp_eth_mac_t *mac, int cmd, void *data)
{
    esp_err_t ret = ESP_OK;
    pmac_ksz8863_t *pmac = __containerof(mac, pmac_ksz8863_t, parent);
    esp_eth_mediator_t *eth = pmac->eth;
    uint8_t last_state;
    ksz8863_chipid1_reg_t start_sw;
    ksz8863_gcr0_reg_t gcr0;
    ksz8863_gcr1_reg_t gcr1;
    ksz8863_pcr2_reg_t pcr2;

    switch (cmd) {
    case KSZ8863_ETH_CMD_S_START_SWITCH:
        ESP_GOTO_ON_FALSE(data, ESP_ERR_INVALID_ARG, err, TAG, "switch start/stop can't be NULL");
        ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, 0, KSZ8863_CHIPID1_REG_ADDR, &(start_sw.val)), err, TAG, "read Start Switch failed");
        start_sw.start_switch = *(bool *)data;
        ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, 0, KSZ8863_CHIPID1_REG_ADDR, start_sw.val), err, TAG, "write Start Switch failed");
        break;
    case KSZ8863_ETH_CMD_G_START_SWITCH:
        ESP_GOTO_ON_FALSE(data, ESP_ERR_INVALID_ARG, err, TAG, "no mem to store switch start/stop status");
        ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, 0, KSZ8863_CHIPID1_REG_ADDR, &(start_sw.val)), err, TAG, "read Start Switch failed");
        *(bool *)data = start_sw.start_switch;
        break;
    case KSZ8863_ETH_CMD_S_FLUSH_MAC_DYN:
        // Learning needs to be disabled prior flush
        ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, 0, KSZ8863_PCR2_BASE_ADDR + pmac->port_reg_offset, &(pcr2.val)), err, TAG, "read PC2 failed");
        last_state = pcr2.learn_dis;
        if (pcr2.learn_dis == 0) {
            pcr2.learn_dis = 1;
            ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, 0, KSZ8863_PCR2_BASE_ADDR + pmac->port_reg_offset, pcr2.val), err, TAG, "write PC2 failed");
        }

        ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, 0, KSZ8863_GCR0_ADDR, &(gcr0.val)), err, TAG, "read GC0 failed");
        gcr0.flush_dyn_mac_tbl = 1;
        ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, 0, KSZ8863_GCR0_ADDR, gcr0.val), err, TAG, "write GC0 failed");

        // Configure learning back to original state
        if (last_state == 0) {
            ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, 0, KSZ8863_PCR2_BASE_ADDR + pmac->port_reg_offset, &(pcr2.val)), err, TAG, "read PC2 failed");
            pcr2.learn_dis = last_state;
            ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, 0, KSZ8863_PCR2_BASE_ADDR + pmac->port_reg_offset, pcr2.val), err, TAG, "write PC2 failed");
        }
        break;
    case KSZ8863_ETH_CMD_S_RX_EN:
        ESP_GOTO_ON_FALSE(data, ESP_ERR_INVALID_ARG, err, TAG, "port rx enable can't be NULL");
        ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, 0, KSZ8863_PCR2_BASE_ADDR + pmac->port_reg_offset, &(pcr2.val)), err, TAG, "read PC2 failed");
        pcr2.rx_en = *(bool *)data;
        ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, 0, KSZ8863_PCR2_BASE_ADDR + pmac->port_reg_offset, pcr2.val), err, TAG, "write PC2 failed");
        break;
    case KSZ8863_ETH_CMD_G_RX_EN:
        ESP_GOTO_ON_FALSE(data, ESP_ERR_INVALID_ARG, err, TAG, "no mem to store port rx enable");
        ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, 0, KSZ8863_PCR2_BASE_ADDR + pmac->port_reg_offset, &(pcr2.val)), err, TAG, "read PC2 failed");
        *(bool *)data = pcr2.rx_en;
        break;
    case KSZ8863_ETH_CMD_S_TX_EN:
        ESP_GOTO_ON_FALSE(data, ESP_ERR_INVALID_ARG, err, TAG, "port tx enable can't be NULL");
        ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, 0, KSZ8863_PCR2_BASE_ADDR + pmac->port_reg_offset, &(pcr2.val)), err, TAG, "read PC2 failed");
        pcr2.tx_en = *(bool *)data;
        ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, 0, KSZ8863_PCR2_BASE_ADDR + pmac->port_reg_offset, pcr2.val), err, TAG, "write PC2 failed");
        break;
    case KSZ8863_ETH_CMD_G_TX_EN:
        ESP_GOTO_ON_FALSE(data, ESP_ERR_INVALID_ARG, err, TAG, "no mem to store port tx enable");
        ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, 0, KSZ8863_PCR2_BASE_ADDR + pmac->port_reg_offset, &(pcr2.val)), err, TAG, "read PC2 failed");
        *(bool *)data = pcr2.tx_en;
        break;
    case KSZ8863_ETH_CMD_S_LEARN_DIS:
        ESP_GOTO_ON_FALSE(data, ESP_ERR_INVALID_ARG, err, TAG, "learning disable can't be NULL");
        ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, 0, KSZ8863_PCR2_BASE_ADDR + pmac->port_reg_offset, &(pcr2.val)), err, TAG, "read PC2 failed");
        pcr2.learn_dis = *(bool *)data;
        ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, 0, KSZ8863_PCR2_BASE_ADDR + pmac->port_reg_offset, pcr2.val), err, TAG, "write PC2 failed");
        break;
    case KSZ8863_ETH_CMD_G_LEARN_DIS:
        ESP_GOTO_ON_FALSE(data, ESP_ERR_INVALID_ARG, err, TAG, "no mem to store port learning disable");
        ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, 0, KSZ8863_PCR2_BASE_ADDR + pmac->port_reg_offset, &(pcr2.val)), err, TAG, "read PC2 failed");
        *(bool *)data = pcr2.learn_dis;
        break;
    case KSZ8863_ETH_CMD_S_MAC_STA_TBL:
        ESP_GOTO_ON_FALSE(data, ESP_ERR_INVALID_ARG, err, TAG, "MAC tbl info can't be NULL");
        ESP_GOTO_ON_ERROR(pmac_ksz8863_set_mac_tbl(pmac, (ksz8863_mac_tbl_info_t *)data), err, TAG, "static MAC table write failed");
        break;
    case KSZ8863_ETH_CMD_G_MAC_STA_TBL:
        ESP_GOTO_ON_FALSE(data, ESP_ERR_INVALID_ARG, err, TAG, "no mem to store static MAC table");
        ESP_GOTO_ON_ERROR(pmac_ksz8863_get_mac_tbl(pmac, KSZ8863_STA_MAC_TABLE, (ksz8863_mac_tbl_info_t *)data),
                          err, TAG, "static MAC table read failed");
        break;
    case KSZ8863_ETH_CMD_G_MAC_DYN_TBL:
        ESP_GOTO_ON_FALSE(data, ESP_ERR_INVALID_ARG, err, TAG, "no mem to store dynamic MAC table");
        ESP_GOTO_ON_ERROR(pmac_ksz8863_get_mac_tbl(pmac, KSZ8863_DYN_MAC_TABLE, (ksz8863_mac_tbl_info_t *)data),
                          err, TAG, "dynamic MAC table read failed");
        break;
    case KSZ8863_ETH_CMD_S_TAIL_TAG:
        ESP_GOTO_ON_FALSE(data, ESP_ERR_INVALID_ARG, err, TAG, "can't set tail tag to null");
        ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, 0, KSZ8863_GCR1_ADDR, &(gcr1.val)), err, TAG, "read GC1 failed");
        gcr1.tail_tag_en = *(bool *)data;
        ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, 0, KSZ8863_GCR1_ADDR, gcr1.val), err, TAG, "write GC1 failed");
        break;
    case KSZ8863_ETH_CMD_G_TAIL_TAG:
        ESP_GOTO_ON_FALSE(data, ESP_ERR_INVALID_ARG, err, TAG, "no mem to store tail tag status");
        ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, 0, KSZ8863_GCR1_ADDR, &(gcr1.val)), err, TAG, "read GC1 failed");
        *(bool *)data = gcr1.tail_tag_en;
        break;
    case KSZ8863_ETH_CMD_G_PORT_NUM:
        ESP_GOTO_ON_FALSE(data, ESP_ERR_INVALID_ARG, err, TAG, "no mem to store port number");
        *(int32_t *)data = pmac->port;
        break;
    default:
        ret = ESP_ERR_INVALID_ARG;
        break;
    }
err:
    return ret;
}

static esp_err_t pmac_ksz8863_transmit(esp_eth_mac_t *mac, uint8_t *buf, uint32_t length)
{
    esp_err_t ret = ESP_OK;
    pmac_ksz8863_t *pmac = __containerof(mac, pmac_ksz8863_t, parent);

    // ESP32 Ethernet Interface (host) is used to access KSZ8863
    ret = ksz8863_eth_transmit_via_host(buf, length, pmac->port + 1);
    return ret;
}

static esp_err_t pmac_ksz8863_receive(esp_eth_mac_t *mac, uint8_t *buf, uint32_t *length)
{
    esp_err_t ret = ESP_OK;
    //pmac_ksz8863_t *pmac = __containerof(mac, pmac_ksz8863_t, parent);
    return ret;
}

static esp_err_t pmac_ksz8863_init(esp_eth_mac_t *mac)
{
    esp_err_t ret = ESP_OK;
    pmac_ksz8863_t *pmac = __containerof(mac, pmac_ksz8863_t, parent);
    esp_eth_mediator_t *eth = pmac->eth;

    ESP_GOTO_ON_ERROR(eth->on_state_changed(eth, ETH_STATE_LLINIT, NULL), err, TAG, "lowlevel init failed");
    /* verify chip id */
    ESP_GOTO_ON_ERROR(ksz8863_verify_id(pmac), err, TAG, "verify chip ID failed");
    /* default setup of internal registers */
    ESP_GOTO_ON_ERROR(ksz8863_setup_port_defaults(pmac), err, TAG, "ksz8863 default port specific setup failed");
    ESP_GOTO_ON_ERROR(ksz8863_setup_global_defaults(pmac), err, TAG, "ksz8863 default global setup failed");

    return ESP_OK;
err:
    eth->on_state_changed(eth, ETH_STATE_DEINIT, NULL);
    return ret;
}

static esp_err_t pmac_ksz8863_deinit(esp_eth_mac_t *mac)
{
    pmac_ksz8863_t *pmac = __containerof(mac, pmac_ksz8863_t, parent);
    esp_eth_mediator_t *eth = pmac->eth;
    mac->stop(mac);
    // TODO
    eth->on_state_changed(eth, ETH_STATE_DEINIT, NULL);
    return ESP_OK;
}

static esp_err_t pmac_ksz8863_del(esp_eth_mac_t *mac)
{
    pmac_ksz8863_t *pmac = __containerof(mac, pmac_ksz8863_t, parent);
    free(pmac);

    struct slist_mac_ksz8863_s *pmac_instance;
    SLIST_FOREACH(pmac_instance, &s_mac_ksz8863_head, next) {
        if (pmac_instance->mac_ksz8863_info == pmac) {
            SLIST_REMOVE(&s_mac_ksz8863_head, pmac_instance, slist_mac_ksz8863_s, next);
            free(pmac_instance);
        }
    }
    return ESP_OK;
}

esp_eth_mac_t *esp_eth_mac_new_ksz8863(const ksz8863_eth_mac_config_t *ksz8863_config, const eth_mac_config_t *config)
{
    esp_eth_mac_t *ret = NULL;
    pmac_ksz8863_t *pmac = NULL;
    ESP_GOTO_ON_FALSE(ksz8863_config, NULL, err, TAG, "can't set ksz8863 specific config to null");
    ESP_GOTO_ON_FALSE(config, NULL, err, TAG, "can't set mac config to null");
    pmac = calloc(1, sizeof(pmac_ksz8863_t));
    ESP_GOTO_ON_FALSE(pmac, NULL, err, TAG, "calloc pmac failed");

    /* bind methods and attributes */
    pmac->parent.set_mediator = pmac_ksz8863_set_mediator;
    pmac->parent.init = pmac_ksz8863_init;
    pmac->parent.deinit = pmac_ksz8863_deinit;
    pmac->parent.start = pmac_ksz8863_start;
    pmac->parent.stop = pmac_ksz8863_stop;
    pmac->parent.del = pmac_ksz8863_del;
    pmac->parent.write_phy_reg = NULL;
    pmac->parent.read_phy_reg = NULL;
    pmac->parent.set_addr = pmac_ksz8863_set_addr;
    pmac->parent.get_addr = pmac_ksz8863_get_addr;
    pmac->parent.set_speed = pmac_ksz8863_set_speed;
    pmac->parent.set_duplex = pmac_ksz8863_set_duplex;
    pmac->parent.set_link = pmac_ksz8863_set_link;
    pmac->parent.set_promiscuous = pmac_ksz8863_set_promiscuous;
    pmac->parent.set_peer_pause_ability = pmac_ksz8863_set_peer_pause_ability;
    pmac->parent.enable_flow_ctrl = pmac_ksz8863_enable_flow_ctrl;
    pmac->parent.custom_ioctl = pmac_ksz8863_custom_ioctl;
    pmac->parent.transmit = pmac_ksz8863_transmit;
    pmac->parent.receive = pmac_ksz8863_receive;

    pmac->port = ksz8863_config->port_num;
    pmac->mode = ksz8863_config->pmac_mode;

    if (pmac->port == KSZ8863_PORT_1) {
        pmac->port_reg_offset = KSZ8863_PORT1_ADDR_OFFSET;
    } else if (pmac->port == KSZ8863_PORT_2) {
        pmac->port_reg_offset = KSZ8863_PORT2_ADDR_OFFSET;
    }

    struct slist_mac_ksz8863_s *pmac_instance = calloc(1, sizeof(struct slist_mac_ksz8863_s));
    ESP_GOTO_ON_FALSE(pmac_instance, NULL, err, TAG, "calloc pmac_instance failed");
    pmac_instance->mac_ksz8863_info = pmac;
    SLIST_INSERT_HEAD(&s_mac_ksz8863_head, pmac_instance, next);

    return &(pmac->parent);
err:
    if (pmac) {
        free(pmac);
    }
    return ret;
}
