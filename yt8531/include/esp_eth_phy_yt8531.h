/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include "esp_eth_phy.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Custom IOCTL commands for YT8531 PHY
 */
typedef enum {
    YT8531_ETH_CMD_S_RGMII_CLK_DELAY = ETH_CMD_CUSTOM_PHY_CMDS_OFFSET, /*!< Configure RGMII RX/TX clock delays */
    YT8531_ETH_CMD_S_FAREND_LOOPBACK,                                  /*!< Enable or disable remote UTP loopback (far-end loopback) */
} phy_yt8531_custom_io_cmd_t;

/**
 * @brief RGMII clock delay configuration for YT8531
 *
 * The RX clock delay is a combination of a coarse ~2 ns stage (rxc_dly_en) and
 * a fine delay train (~150 ps per step, 0-15 steps).
 *
 * The TX clock delay is a 4-bit delay train (~150 ps per step, 0-15 steps),
 * with separate values for 1000 Mbps and 100/10 Mbps.
 */
typedef struct {
    bool    rxc_dly_en;      /*!< Enable ~2 ns RX clock delay (Chip_Config EXT_0xA001 bit[8]) */
    uint8_t rx_delay_sel;    /*!< RX clock fine delay: 0-15 steps, ~150 ps/step
                                  (RGMII_Config1 EXT_0xA003 bits[13:10]) */
    uint8_t tx_delay_sel;    /*!< TX clock delay at 1000 Mbps: 0-15 steps, ~150 ps/step
                                  (RGMII_Config1 EXT_0xA003 bits[3:0]) */
    uint8_t tx_delay_sel_fe; /*!< TX clock delay at 100/10 Mbps: 0-15 steps, ~150 ps/step
                                  (RGMII_Config1 EXT_0xA003 bits[7:4]) */
} yt8531_rgmii_clk_delay_config_t;

/**
 * @brief Create a PHY instance of YT8531
 *
 * @param[in] config: configuration of PHY
 *
 * @return
 *      - instance: create PHY instance successfully
 *      - NULL: create PHY instance failed because some error occurred
 */
esp_eth_phy_t *esp_eth_phy_new_yt8531(const eth_phy_config_t *config);

#ifdef __cplusplus
}
#endif
