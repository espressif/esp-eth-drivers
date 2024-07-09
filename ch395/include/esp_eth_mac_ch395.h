/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileContributor: 2024 Sergey Kharenko
 * SPDX-FileContributor: 2024 Espressif Systems (Shanghai) CO LTD
 */

#pragma once

#include "esp_eth_com.h"
#include "esp_eth_mac.h"

#include "sdkconfig.h"

#if CONFIG_ETH_CH395_INTERFACE_SPI
#include "driver/spi_master.h"
#endif

#if CONFIG_ETH_CH395_INTERFACE_UART
#include "driver/uart.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief CH395 specific configuration
 *
 */
typedef struct {
    int int_gpio_num;                                   /*!< Interrupt GPIO number */
    uint32_t poll_period_ms;                            /*!< Period in ms to poll rx status when interrupt mode is not used */

#if CONFIG_ETH_CH395_INTERFACE_SPI
    int spi_cs_gpio_num;                                /*!< SPI CS pin number(CS pin is directly controlled by eth driver)*/
    spi_host_device_t spi_host_id;                      /*!< SPI peripheral (this field is invalid when custom SPI driver is defined) */
    spi_device_interface_config_t *spi_devcfg;          /*!< SPI device configuration (this field is invalid when custom SPI driver is defined) */
#endif

#if CONFIG_ETH_CH395_INTERFACE_UART
    int uart_tx_gpio_num;
    int uart_rx_gpio_num;
    uart_port_t uart_port_id;
    uart_config_t *uart_devcfg;
#endif
} eth_ch395_config_t;

/**
 * @brief Default CH395 specific configuration
 *
 */
#if CONFIG_ETH_CH395_INTERFACE_SPI
#define ETH_CH395_DEFAULT_CONFIG(spi_host, spi_devcfg_p) \
    {                                           \
        .int_gpio_num = 0,                      \
        .spi_host_id = spi_host,                \
        .spi_devcfg = spi_devcfg_p,             \
    }
#endif

#if CONFIG_ETH_CH395_INTERFACE_UART
#define ETH_CH395_DEFAULT_CONFIG(uart_port, uart_cfg_p) \
    {                                           \
        .int_gpio_num = 0,                      \
        .uart_tx_gpio_num = 0,                  \
        .uart_rx_gpio_num = 0,                  \
        .uart_port_id = uart_port,              \
        .uart_devcfg = uart_cfg_p,              \
    }
#endif

/**
* @brief Create CH395 Ethernet MAC instance
*
* @param ch395_config: CH395 specific configuration
* @param mac_config: Ethernet MAC configuration
*
* @return
*      - instance: create MAC instance successfully
*      - NULL: create MAC instance failed because some error occurred
*/
esp_eth_mac_t *esp_eth_mac_new_ch395(const eth_ch395_config_t *ch395_config, const eth_mac_config_t *mac_config);

#ifdef __cplusplus
}
#endif

