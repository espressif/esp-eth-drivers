/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include "esp_console.h"

esp_netif_t host_netif = NULL;
esp_netif_t port1_netif = NULL;
esp_netif_t port2_netif = NULL;
esp_eth_handle_t host_eth_handle = NULL;
esp_eth_handle_t port1_eth_handle = NULL;
esp_eth_handle_t port2_eth_handle = NULL;

void app_main(void)
{
    // install console REPL environment
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.prompt = "ksz8863>";
    esp_console_dev_uart_config_t uart_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&uart_config, &repl_config, &repl));
    register_ksz8863_config_commands(&port1_eth_handle, &port2_eth_handle);
    // start console REPL
    ESP_ERROR_CHECK(esp_console_start_repl(repl));

}
