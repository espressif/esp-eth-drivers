/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#if __has_include("esp_idf_version.h")
#include "esp_idf_version.h"
#endif

/**
 * @brief Supported ESP-IDF features that ethernet_init may use depending on
 *        the IDF version. If esp_idf_version.h is missing, all additional
 *        features stay disabled.
 */

#ifdef ESP_IDF_VERSION

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
#define ETH_INIT_SUPPORTED_FEATURE_IDF_V6_PHY_SPI_DRIVERS
#endif

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 1, 0)
#define ETH_INIT_SUPPORTED_FEATURE_PHY_1000M
#endif

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 2, 0)
#define ETH_INIT_SUPPORTED_FEATURE_EMAC_SLEEP_MODE
#define ETH_INIT_SUPPORTED_FEATURE_PHY_HOLD_RESET_GPIO
#endif

#endif /* ESP_IDF_VERSION */
