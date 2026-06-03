/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_eth_phy.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    VSC8541_ETH_CMD_S_RGMII_CLK_DELAY = ETH_CMD_CUSTOM_PHY_CMDS_OFFSET, /*!< Configure RGMII RX/TX clock delay and clear bit 11 in register 20E2 (page 2) */
} phy_vsc8541_custom_io_cmd_t;

typedef struct {
    uint8_t rx_clk_delay; /*!< RGMII RX clock delay value, written to register 20E2 bits [6:4] */
    uint8_t tx_clk_delay; /*!< RGMII TX clock delay value, written to register 20E2 bits [2:0] */
} vsc8541_rgmii_clk_delay_config_t;

/**
* @brief Create a PHY instance of VSC8541
*
* @param[in] config: configuration of PHY
*
* @return
*      - instance: create PHY instance successfully
*      - NULL: create PHY instance failed because some error occurred
*/
esp_eth_phy_t *esp_eth_phy_new_vsc8541(const eth_phy_config_t *config);

#ifdef __cplusplus
}
#endif
