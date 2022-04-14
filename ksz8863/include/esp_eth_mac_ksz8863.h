/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_eth_com.h"
#include "esp_eth_mac.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    KSZ8863_SWITCH_MODE,
    KSZ8863_PORT_MODE
} pmac_ksz8863_mode_t;

typedef struct {
    int port_num;
    pmac_ksz8863_mode_t pmac_mode;
} ksz8863_eth_mac_config_t;

/**
 * @brief Create a MAC instance of KSZ8863
 *
 * @param ksz8863_config KSZ8863 specific MAC configuration options
 * @param config ESP-IDF standard MAC configuration options
 * @return esp_eth_mac_t*
 *          instance: create MAC instance successfully
 *          NULL: create MAC instance failed because some error occurred
 */
esp_eth_mac_t *esp_eth_mac_new_ksz8863(const ksz8863_eth_mac_config_t *ksz8863_config, const eth_mac_config_t *config);

#ifdef __cplusplus
}
#endif
