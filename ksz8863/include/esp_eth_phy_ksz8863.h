/*
 * SPDX-FileCopyrightText: 2019-2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_eth_com.h"
#include "esp_eth_phy.h"

#ifdef __cplusplus
extern "C" {
#endif

// TODO: function description
/**
 * @brief
 *
 * @param config
 * @return esp_eth_phy_t*
 */
esp_eth_phy_t *esp_eth_phy_new_ksz8863(const eth_phy_config_t *config);

#ifdef __cplusplus
}
#endif
