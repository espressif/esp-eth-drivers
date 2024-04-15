/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include <string.h>
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_event.h"
#include "esp_console.h"
#include "esp_log.h"
#include "ethernet_init.h"
#include "iperf_cmd.h"
#include "sdkconfig.h"

static const char *TAG = "iperf_example";

static void start_dhcp_server_after_connection(void *arg, esp_event_base_t base, int32_t id, void *event_data)
{
    esp_netif_t *eth_netif = esp_netif_next_unsafe(NULL);
    esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;
    while (eth_netif != NULL) {
        esp_eth_handle_t eth_handle_for_current_netif = esp_netif_get_io_driver(eth_netif);
        if (memcmp(&eth_handle, &eth_handle_for_current_netif, sizeof(esp_eth_handle_t)) == 0) {
            esp_netif_dhcpc_stop(eth_netif);
            esp_netif_dhcps_start(eth_netif);
        }
        eth_netif = esp_netif_next_unsafe(eth_netif);
    }
}

void app_main(void)
{
    uint8_t eth_port_cnt = 0;
    char if_key_str[10];
    char if_desc_str[10];
    esp_eth_handle_t *eth_handles;
    esp_netif_config_t cfg;
    esp_netif_inherent_config_t eth_netif_cfg;
    esp_netif_init();
    esp_event_loop_create_default();
    ethernet_init_all(&eth_handles, &eth_port_cnt);

#if CONFIG_EXAMPLE_ACT_AS_DHCP_SERVER
    esp_netif_ip_info_t *ip_infos;

    ip_infos = calloc(eth_port_cnt, sizeof(esp_netif_ip_info_t));

    eth_netif_cfg = (esp_netif_inherent_config_t) {
        .get_ip_event = IP_EVENT_ETH_GOT_IP,
        .lost_ip_event = 0,
        .flags = ESP_NETIF_DHCP_SERVER,
        .route_prio = 50
    };
    cfg = (esp_netif_config_t) {
        .base = &eth_netif_cfg,
        .stack = ESP_NETIF_NETSTACK_DEFAULT_ETH
    };

    for (uint8_t i = 0; i < eth_port_cnt; i++) {
        sprintf(if_key_str, "ETH_S%d", i);
        sprintf(if_desc_str, "eth%d", i);

        esp_netif_ip_info_t ip_info_i = {
            .ip = {.addr = ESP_IP4TOADDR(192, 168, i, 1)},
            .netmask = {.addr =  ESP_IP4TOADDR(255, 255, 255, 0)},
            .gw = {.addr = ESP_IP4TOADDR(192, 168, i, 1)}
        };
        ip_infos[i] = ip_info_i;

        eth_netif_cfg.if_key = if_key_str;
        eth_netif_cfg.if_desc = if_desc_str;
        eth_netif_cfg.route_prio -= i * 5;
        eth_netif_cfg.ip_info = &(ip_infos[i]);
        esp_netif_t *eth_netif = esp_netif_new(&cfg);
        ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handles[i])));
    }
    esp_event_handler_register(ETH_EVENT, ETHERNET_EVENT_CONNECTED, start_dhcp_server_after_connection, NULL);
    ESP_LOGI(TAG, "--------");
    for (uint8_t i = 0; i < eth_port_cnt; i++) {
        esp_eth_start(eth_handles[i]);
        ESP_LOGI(TAG, "Network Interface %d: " IPSTR, i, IP2STR(&ip_infos[i].ip));
    }
    ESP_LOGI(TAG, "--------");
#else
    if (eth_port_cnt == 1) {
        // Use default config when using one interface
        eth_netif_cfg = *(ESP_NETIF_BASE_DEFAULT_ETH);
    } else {
        // Set config to support multiple interfaces
        eth_netif_cfg = (esp_netif_inherent_config_t) ESP_NETIF_INHERENT_DEFAULT_ETH();
    }
    cfg = (esp_netif_config_t) {
        .base = &eth_netif_cfg,
        .stack = ESP_NETIF_NETSTACK_DEFAULT_ETH
    };
    for (int i = 0; i < eth_port_cnt; i++) {
        sprintf(if_key_str, "ETH_%d", i);
        sprintf(if_desc_str, "eth%d", i);
        eth_netif_cfg.if_key = if_key_str;
        eth_netif_cfg.if_desc = if_desc_str;
        eth_netif_cfg.route_prio -= i * 5;
        esp_netif_t *eth_netif = esp_netif_new(&cfg);
        ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handles[i])));
        esp_eth_start(eth_handles[i]);
    }
#endif
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    esp_console_dev_uart_config_t uart_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    esp_console_new_repl_uart(&uart_config, &repl_config, &repl);
    app_register_iperf_commands();
    esp_console_start_repl(repl);
}
