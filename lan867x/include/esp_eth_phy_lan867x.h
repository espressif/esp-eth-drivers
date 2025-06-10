/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_eth_phy_lan86xx.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
* @brief Create a PHY instance of LAN867x
*
* @param[in] config: configuration of PHY
*
* @return
*      - instance: create PHY instance successfully
*      - NULL: create PHY instance failed because some error occurred
*/
esp_eth_phy_t *esp_eth_phy_new_lan867x(const eth_phy_config_t *config);

#ifdef __cplusplus
}
#endif
