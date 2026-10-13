/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Delay for at least delay_us
 *
 * Busy-waits for delays shorter than one FreeRTOS tick, otherwise blocks
 * with vTaskDelay() rounded up plus one tick. Input is in microseconds.
 *
 * @param delay_us Delay in microseconds
 */
void wiznet_delay_us(uint32_t delay_us);

/**
 * @brief Delay for at least delay_ms
 *
 * Busy-waits for delays shorter than one FreeRTOS tick, otherwise blocks
 * with vTaskDelay() rounded up plus one tick. Input is in milliseconds.
 *
 * @param delay_ms Delay in milliseconds
 */
void wiznet_delay_ms(uint32_t delay_ms);

#ifdef __cplusplus
}
#endif
