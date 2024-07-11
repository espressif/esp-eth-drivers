/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_event.h"
#include "esp_log.h"
#include "ethernet_init.h"
#include "lwip/sockets.h"

#define SOCKET_PORT         5000
#define SOCKET_MAX_LENGTH   128

static const char *TAG = "tcp_server";

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

    int server_fd, client_fd, ret;
    struct sockaddr_in server, client;
    char rxbuffer[SOCKET_MAX_LENGTH] = {0};
    char txbuffer[SOCKET_MAX_LENGTH] = {0};
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        ESP_LOGE(TAG, "Could not create the socket (errno: %d)", errno);
        goto err;
    }
    server.sin_family = AF_INET;
    server.sin_port = htons(SOCKET_PORT);
    server.sin_addr.s_addr = htonl(INADDR_ANY);

    ret = bind(server_fd, (struct sockaddr *) &server, sizeof(server));
    if (ret == -1) {
        ESP_LOGE(TAG, "Could not bind to the socket (errno: %d)", errno);
    }
    listen(server_fd, 1);
    int transmission_cnt = 0;
    while (1) {
        socklen_t client_len = sizeof(client);
        client_fd = accept(server_fd, (struct sockaddr *) &client, &client_len);
        if (client_fd == -1) {
            ESP_LOGE(TAG, "An error has occurred while accepting a connection (errno: %d)", errno);
            goto err;
        }
        while (1) {
            ret = recv(client_fd, rxbuffer, SOCKET_MAX_LENGTH, 0);
            if (ret == -1) {
                ESP_LOGE(TAG, "An error has occurred while receiving data (errno: %d)", errno);
            } else if (ret == 0) {
                break;  // done reading
            }
            ESP_LOGI(TAG, "Received \"%s\"", rxbuffer);
            snprintf(txbuffer, SOCKET_MAX_LENGTH, "Transmission #%d. Hello from ESP32 TCP server", ++transmission_cnt);
            ESP_LOGI(TAG, "Transmitting: \"%s\"", txbuffer);
            ret = send(client_fd, txbuffer, SOCKET_MAX_LENGTH, 0);
            if (ret == -1) {
                ESP_LOGE(TAG, "An error has occurred while sending data (errno: %d)", errno);
                break;
            }
        }
    }
    return;
err:
    ESP_LOGI(TAG, "Program was stopped because an error occured");
}
