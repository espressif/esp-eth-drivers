/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_eth_phy.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LAN86XX_ETH_CMD_S_EN_PLCA = ETH_CMD_CUSTOM_PHY_CMDS, /*!< Enable or disable PLCA */
    LAN86XX_ETH_CMD_G_EN_PLCA,                           /*!< Get status if PLCA is disabled or enabled*/
    LAN86XX_ETH_CMD_S_PLCA_NCNT,                         /*!< Set PLCA node count */
    LAN86XX_ETH_CMD_G_PLCA_NCNT,                         /*!< Get PLCA node count */
    LAN86XX_ETH_CMD_S_PLCA_ID,                           /*!< Set PLCA ID */
    LAN86XX_ETH_CMD_G_PLCA_ID,                           /*!< Get PLCA ID */
    LAN86XX_ETH_CMD_S_PLCA_TOT,                          /*!< Set PLCA Transmit Opportunity Timer in incriments of 100ns */
    LAN86XX_ETH_CMD_G_PLCA_TOT,                          /*!< Get PLCA Transmit Opportunity Timer in incriments of 100ns */
    LAN86XX_ETH_CMD_ADD_TX_OPPORTUNITY,                  /*!< Add additional transmit opportunity for chosen node */
    LAN86XX_ETH_CMD_RM_TX_OPPORTUNITY,                   /*!< Remove additional transmit opportunity for chosen node */
    LAN86XX_ETH_CMD_S_MAX_BURST_COUNT,                   /*!< Set max count of additional packets, set to 0 to disable */
    LAN86XX_ETH_CMD_G_MAX_BURST_COUNT,                   /*!< Get max count of additional packets, set to 0 to disable */
    LAN86XX_ETH_CMD_S_BURST_TIMER,                       /*!< Set time after transmission during which node is allowed to transmit more packets in incriments of 100ns */
    LAN86XX_ETH_CMD_G_BURST_TIMER,                       /*!< Get time after transmission during which node is allowed to transmit more packets in incriments of 100ns */
    LAN86XX_ETH_CMD_PLCA_RST                             /*!< Reset PLCA*/
} phy_lan86xx_custom_io_cmd_t;

/**
* @brief Create a PHY instance of LAN86xx
*
* @param[in] config: configuration of PHY
*
* @return
*      - instance: create PHY instance successfully
*      - NULL: create PHY instance failed because some error occurred
*/
esp_eth_phy_t *esp_eth_phy_new_lan86xx(const eth_phy_config_t *config);

#ifdef __cplusplus
}
#endif
