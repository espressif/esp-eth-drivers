/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#pragma once

#include "esp_eth_com.h"
#include "esp_eth_mac_spi.h"
#include "wiznet_mac_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief W6100 specific configuration
 *
 */
typedef struct {
    eth_wiznet_config_t base;                           /*!< Common WIZnet configuration */
    // Additional W6100 specific configuration should be added here.
} eth_w6100_config_t;

/**
 * @brief Default W6100 specific configuration
 *
 */
#define ETH_W6100_DEFAULT_CONFIG(spi_host, spi_devcfg_p) \
    {                                                  \
        .base = {                                      \
            .int_gpio_num = 4,                         \
            .poll_period_ms = 0,                       \
            .spi_host_id = spi_host,                   \
            .spi_devcfg = spi_devcfg_p,                \
            .custom_spi_driver = ETH_DEFAULT_SPI,      \
        },                                             \
    }

/**
* @brief Create W6100 Ethernet MAC instance
*
* @param w6100_config: W6100 specific configuration
* @param mac_config: Ethernet MAC configuration
*
* @return
*      - instance: create MAC instance successfully
*      - NULL: create MAC instance failed because some error occurred
*/
esp_eth_mac_t *esp_eth_mac_new_w6100(const eth_w6100_config_t *w6100_config, const eth_mac_config_t *mac_config);

#ifdef __cplusplus
}
#endif
