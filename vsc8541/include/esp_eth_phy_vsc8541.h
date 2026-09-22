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

/**
 * @brief VSC8541 MAC interface selection (Extended PHY Control register 23, bits 12:11)
 *
 * @note Values match the PHY register encoding. Reserved encoding is rejected by the ioctl.
 */
typedef enum {
    VSC8541_MAC_INTF_SEL_GMII_MII = 0x0, /*!< GMII/MII */
    VSC8541_MAC_INTF_SEL_RMII     = 0x1, /*!< RMII */
    VSC8541_MAC_INTF_SEL_RGMII    = 0x2, /*!< RGMII */
    VSC8541_MAC_INTF_SEL_RESERVED = 0x3, /*!< Reserved */
} vsc8541_mac_interface_sel_t;

typedef enum {
    VSC8541_ETH_CMD_S_RGMII_CLK_DELAY = ETH_CMD_CUSTOM_PHY_CMDS_OFFSET, /*!< Configure RGMII RX/TX clock delay and clear bit 11 in register 20E2 (page 2) */
    VSC8541_ETH_CMD_S_MAC_IFACE, /*!< Override bootstrap MAC interface (`vsc8541_mac_interface_sel_t *`). Call only while the Ethernet driver is stopped. Sticky across PHY reset. RMII disables 1000BASE-T advertisement. */
    VSC8541_ETH_CMD_S_RMII_CLKOUT, /*!< Enable or disable RMII CLKOUT on RX_CLK (`bool *`). Writes register 27E2 bit 4. Disable in RMII Mode 1 when the MAC supplies the 50 MHz clock. RMII mode is required. */
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
