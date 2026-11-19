/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>
#include "esp_eth.h"

#ifdef __cplusplus
extern "C" {
#endif

// Register KSZ8863 configuration commands
void register_ksz8863_config_commands(esp_eth_handle_t h_handle, esp_eth_handle_t p1_handle, esp_eth_handle_t p2_handle);

#ifdef __cplusplus
}
#endif
