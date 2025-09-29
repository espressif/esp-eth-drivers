/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_eth_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ETH_DEV_NAME_MAX_LEN 12

typedef enum {
    ETH_DEV_TYPE_UNKNOWN,
    ETH_DEV_TYPE_INTERNAL_ETH,
    ETH_DEV_TYPE_SPI,
    ETH_DEV_TYPE_OPENETH,
} eth_dev_type_t;

typedef struct {
    char name[ETH_DEV_NAME_MAX_LEN];
    eth_dev_type_t type;
    union {
        struct {
            uint8_t eth_internal_mdc;   // MDC gpio of internal ethernet
            uint8_t eth_internal_mdio;  // MDIO gpio of internal ethernet
        };
        struct {
            uint8_t eth_spi_cs;     // CS gpio of spi ethernet
            uint8_t eth_spi_int;    // INT gpio of spi ethernet
        };
    } pin;
} eth_dev_info_t;

/**
 * @brief Initialize Ethernet driver based on Espressif IoT Development Framework Configuration
 *
 * @param[out] eth_handles_out array of initialized Ethernet driver handles
 * @param[out] eth_cnt_out number of initialized Ethernets
 * @return
 *          - ESP_OK on success
 *          - ESP_ERR_INVALID_ARG when passed invalid pointers
 *          - ESP_ERR_NO_MEM when there is no memory to allocate for Ethernet driver handles array
 *          - ESP_FAIL on any other failure
 */
esp_err_t ethernet_init_all(esp_eth_handle_t *eth_handles_out[], uint8_t *eth_cnt_out);

/**
 * @brief Deinitialize Ethernet driver
 *
 * @param[in] eth_handles Ethernet driver handles
 * @return
 *          - ESP_OK on success
 *          - ESP_ERR_INVALID_ARG when passed invalid pointers
 *          - ESP_FAIL on any other failure
 */
esp_err_t ethernet_deinit_all(esp_eth_handle_t *eth_handles);

/**
 * @brief Returns the device type of the ethernet handle
 *
 * @param[out] eth_handle Initialized Ethernet driver handle
 * @return
 *          - eth_dev_info_t device information of the ethernet handle
 */
eth_dev_info_t ethernet_init_get_dev_info(esp_eth_handle_t eth_handle);

#ifdef __cplusplus
}
#endif
