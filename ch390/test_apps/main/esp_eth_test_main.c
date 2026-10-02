/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "unity.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "esp_eth_phy_ch390.h"
#include "esp_eth_test_utils.h"

esp_eth_phy_t *esp_eth_test_phy_new(const eth_phy_config_t *config)
{
    return esp_eth_phy_new_ch390(config);
}

void test_task(void *pvParameters)
{
    unity_run_menu();
}

void app_main(void)
{
    xTaskCreatePinnedToCore(test_task, "testTask", CONFIG_ETH_TEST_UNITY_TEST_TASK_STACK, NULL, CONFIG_ETH_TEST_UNITY_TEST_TASK_PRIO, NULL, tskNO_AFFINITY);
}
