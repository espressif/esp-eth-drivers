/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "esp_eth_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t loopback_near_end_test(esp_eth_handle_t *eth_handle, bool verbose, uint32_t frame_length, uint32_t count, uint32_t period_us);
esp_err_t transmit_to_host(esp_eth_handle_t *eth_handle, bool verbose, uint32_t frame_length, uint32_t count, uint32_t period_us);
esp_err_t loop_server(esp_eth_handle_t *eth_handle, bool verbose, uint16_t eth_type, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif
