/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_eth_phy_lan86xx.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
* @brief Create a PHY instance of LAN865x
*
* @param[in] config: configuration of PHY
*
* @return
*      - instance: create PHY instance successfully
*      - NULL: create PHY instance failed because some error occurred
*/
#define esp_eth_phy_new_lan865x(config) esp_eth_phy_new_lan86xx(config)

#ifdef __cplusplus
}
#endif
