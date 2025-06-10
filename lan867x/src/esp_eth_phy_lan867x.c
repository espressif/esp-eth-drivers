/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_eth_driver.h"
#include "esp_eth_phy_lan867x.h"

esp_eth_phy_t *esp_eth_phy_new_lan867x(const eth_phy_config_t *config)
{
    return esp_eth_phy_new_lan86xx(config);
}
