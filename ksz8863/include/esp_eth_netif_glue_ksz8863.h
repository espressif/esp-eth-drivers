/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_netif.h"
#include "esp_eth_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Helper macro to init KSZ8863 switch netif glue configuration structure
 *
 */
#define KSZ8863_DEFAULT_NETIF_GLUE_CONFIG(host_eth, p1_eth, p2_eth)  \
    {                                                                \
        .host_eth_hdl = host_eth,                                    \
        .p1_eth_hndl = p1_eth,                                       \
        .p2_eth_hndl = p2_eth,                                       \
    }                                                                \

/**
 * @brief Handle of KSZ8863 switch netif glue - an intermediate layer between netif and switch host port Ethernet driver
 *
 */
typedef void *ksz8863_esp_eth_netif_glue_handle_t;

typedef struct {
    esp_eth_handle_t host_eth_hdl;
    esp_eth_handle_t p1_eth_hndl;
    esp_eth_handle_t p2_eth_hndl;
} ksz8863_esp_eth_netif_glue_config_t;

/**
 * @brief Create a KSZ8863 switch netif glue for Host Ethernet driver
 * @note switch netif glue is used to configure io host driver port forwarding and to attach Host driver to TCP/IP netif
 *
 * @param config KSZ8863 switch netif glue configuration
 * @return glue object, which inherits esp_netif_driver_base_t
 */
ksz8863_esp_eth_netif_glue_handle_t ksz8863_esp_eth_new_netif_glue_switch(ksz8863_esp_eth_netif_glue_config_t *config);

/**
 * @brief Delete netif glue of Ethernet driver
 *
 * @param sw_esp_netif_glue netif glue
 * @return -ESP_OK: delete netif glue successfully
 */
esp_err_t ksz8863_esp_eth_del_netif_glue_switch(ksz8863_esp_eth_netif_glue_handle_t esp_netif_glue);

#ifdef __cplusplus
}
#endif
