/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_event.h"
#include "esp_netif.h"
#include "esp_eth.h"

#define ETH_START_BIT BIT(0)
#define ETH_STOP_BIT BIT(1)
#define ETH_CONNECT_BIT BIT(2)
#define ETH_GOT_IP_BIT BIT(3)

#define ETH_START_TIMEOUT_MS (1000)
#define ETH_CONNECT_TIMEOUT_MS (10000)
#define ETH_STOP_TIMEOUT_MS (1000)
#define ETH_GET_IP_TIMEOUT_MS (60000)

typedef enum {
    PHY_IP101,
    PHY_LAN87XX,
    PHY_KSZ80XX,
    PHY_RTL8201,
    PHY_DP83848,
    PHY_ID_END
} phy_id_t;

typedef struct {
    uint8_t dest[ETH_ADDR_LEN];
    uint8_t src[ETH_ADDR_LEN];
    uint16_t proto;
    uint8_t data[];
} __attribute__((__packed__)) emac_frame_t;

EventGroupHandle_t create_eth_event_group(void);
void delete_eth_event_group(EventGroupHandle_t eth_event_group);

/** Event handler for IP_EVENT_ETH_GOT_IP */
void got_ip_event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data);

esp_err_t write_phy_reg(esp_eth_handle_t *eth_handle, uint32_t addr, uint32_t data);
esp_err_t dump_phy_regs(esp_eth_handle_t *eth_handle, uint32_t start_addr, uint32_t end_addr);

esp_err_t loopback_near_end_en(esp_eth_handle_t *eth_handle, phy_id_t phy_id, bool enable);
esp_err_t loopback_far_end_en(esp_eth_handle_t *eth_handle, phy_id_t phy_id, bool enable);
