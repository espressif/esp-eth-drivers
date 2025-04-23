/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include "esp_log.h"
#include "esp_event.h"
#include "esp_err.h"
#include "esp_check.h"

#include "esp_console.h"
#include "esp_vfs_fat.h"
#include "cmd_system.h"
#include "cmd_ethernet.h"

void app_main(void)
{
    // Increase logging level of Ethernet related
    esp_log_level_set("*", ESP_LOG_VERBOSE);

    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    esp_console_dev_uart_config_t uart_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    /*#if CONFIG_EXAMPLE_STORE_HISTORY // TODO add history
        initialize_filesystem();
        repl_config.history_save_path = HISTORY_PATH;
    #endif*/
    repl_config.prompt = "eth_phy>";
    // init console REPL environment
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&uart_config, &repl_config, &repl));

    /* Register commands */
    register_system_common();
    register_ethernet();

    printf("\n =======================================================\n");
    printf(" |          Steps to Test Ethernet PHY                  |\n");
    printf(" |                                                      |\n");
    printf(" |  1. Enter 'help', check all supported commands       |\n");
    printf(" |  2. Connect DUT Ethernet directly to test PC         |\n");
    printf(" |  3. Execute any command                              |\n");
    printf(" |                                                      |\n");
    printf(" =======================================================\n\n");

    // start console REPL
    ESP_ERROR_CHECK(esp_console_start_repl(repl));
}
