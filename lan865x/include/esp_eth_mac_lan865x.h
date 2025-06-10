/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_eth_mac.h"
#include "esp_idf_version.h"
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
#include "esp_eth_mac_spi.h"
#endif
#include "driver/spi_master.h"

#ifdef __cplusplus
extern "C" {
#endif
/**
 * @brief LAN865x specific configuration
 *
 */
typedef struct {
    spi_host_device_t spi_host_id;                      /*!< SPI host id */
    spi_device_interface_config_t *spi_devcfg;          /*!< SPI device configuration */
    int int_gpio_num;                                   /*!< Interrupt GPIO number, set to -1 if no interrupt */
    uint32_t poll_period_ms;                            /*!< Polling period in milliseconds if no interrupt */
    eth_spi_custom_driver_config_t custom_spi_driver;   /*!< Custom SPI driver configuration, optional */
} eth_lan865x_config_t;

/**
 * @brief Default LAN865x specific configuration
 *
 */
#define ETH_LAN865X_DEFAULT_CONFIG(spi_host, spi_devcfg_p) \
    {                                           \
        .int_gpio_num = 4,                      \
        .poll_period_ms = 0,                    \
        .spi_host_id = spi_host,                \
        .spi_devcfg = spi_devcfg_p,             \
        .custom_spi_driver = ETH_DEFAULT_SPI,   \
    }

/**
 * @brief Create a new LAN865x Ethernet MAC driver
 *
 * @param lan865x_config: LAN865x specific configuration
 * @param mac_config: Ethernet MAC configuration
 *
 * @return
 *      - esp_eth_mac_t: handle of Ethernet MAC driver
 *      - NULL: failed to create MAC driver
 */
esp_eth_mac_t *esp_eth_mac_new_lan865x(const eth_lan865x_config_t *lan865x_config, const eth_mac_config_t *mac_config);

#ifdef __cplusplus
}
#endif
