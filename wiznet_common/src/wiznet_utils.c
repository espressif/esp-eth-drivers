/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_rom_sys.h"
#include "wiznet_utils.h"

#define US_PER_MS (1000)

/* Delay for at least delay_us. Delays shorter than a FreeRTOS tick must be busy
 * waited. For longer delays, request one extra tick because vTaskDelay() starts
 * partway through the current tick. */
void wiznet_delay_us(uint32_t delay_us)
{
    uint32_t tick_period_us = portTICK_PERIOD_MS * US_PER_MS;
    if (delay_us < tick_period_us) {
        esp_rom_delay_us(delay_us);
    } else {
        vTaskDelay((delay_us + tick_period_us - 1) / tick_period_us + 1);
    }
}

void wiznet_delay_ms(uint32_t delay_ms)
{
    uint32_t tick_period_ms = portTICK_PERIOD_MS;
    if (delay_ms < tick_period_ms) {
        esp_rom_delay_us(delay_ms * US_PER_MS);
    } else {
        vTaskDelay((delay_ms + tick_period_ms - 1) / tick_period_ms + 1);
    }
}
