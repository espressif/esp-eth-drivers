/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_netif.h"
#include "esp_netif_net_stack.h"
#include "esp_eth.h"
#include "esp_eth_phy_lan867x.h"
#include "esp_event.h"
#include "esp_log.h"
#include "ethernet_init.h"
#include "lwip/sockets.h"

#define SOCKET_PORT         5000
#define LISTENER_MAX_QUEUE  8
#define SOCKET_MAX_LENGTH   128

static const char *TAG = "lan867x_server";

/* Structure to store information about individual connection */
struct connection_info {
    int fd;
    struct sockaddr_in address;
};

/* Event handler for IP_EVENT_ETH_GOT_IP */
static void got_ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *) data;
    const esp_netif_ip_info_t *ip_info = &event->ip_info;

    ESP_LOGI(TAG, "Ethernet Got IP Address");
    ESP_LOGI(TAG, "~~~~~~~~~~~");
    ESP_LOGI(TAG, "ETHIP:" IPSTR, IP2STR(&ip_info->ip));
    ESP_LOGI(TAG, "ETHMASK:" IPSTR, IP2STR(&ip_info->netmask));
    ESP_LOGI(TAG, "ETHGW:" IPSTR, IP2STR(&ip_info->gw));
    ESP_LOGI(TAG, "~~~~~~~~~~~");
}

static void my_event_connected_handler(void *esp_netif, esp_event_base_t base, int32_t event_id, void *data)
{
    esp_netif_dhcps_start(esp_netif);
}

void app_main(void)
{
    // Create default event loop that running in background
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    // Initialize Ethernet driver
    uint8_t eth_port_cnt = 0;
    esp_eth_handle_t *eth_handles;
    ESP_ERROR_CHECK(ethernet_init_all(&eth_handles, &eth_port_cnt));
    // Initialize TCP/IP network interface aka the esp-netif (should be called only once in application)
    ESP_ERROR_CHECK(esp_netif_init());
    esp_netif_ip_info_t ip_info = {
        .ip = {.addr = ESP_IP4TOADDR(192, 168, 1, 1)},
        .netmask = {.addr =  ESP_IP4TOADDR(255, 255, 255, 0)},
        .gw = {.addr = ESP_IP4TOADDR(192, 168, 1, 255)}
    };
    const esp_netif_inherent_config_t eth_behav_cfg = {
        .get_ip_event = IP_EVENT_ETH_GOT_IP,
        .lost_ip_event = 0,
        .flags = ESP_NETIF_DHCP_SERVER,
        .ip_info = &ip_info,
        .if_key = "ETH_DHCPS",
        .if_desc = "eth",
        .route_prio = 50
    };
    esp_netif_config_t eth_as_dhcps_cfg = { .base = &eth_behav_cfg, .stack = ESP_NETIF_NETSTACK_DEFAULT_ETH };
    esp_netif_t *eth_netif = esp_netif_new(&eth_as_dhcps_cfg);
    // Attach Ethernet driver to TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handles[0])));
    // Register user defined event handers
    esp_event_handler_register(ETH_EVENT, ETHERNET_EVENT_CONNECTED, my_event_connected_handler, eth_netif);
    esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, got_ip_event_handler, NULL);
    // Stop dhcp client and set a static IP address
    esp_netif_dhcpc_stop(eth_netif);
    esp_netif_set_ip_info(eth_netif, &ip_info);
#if CONFIG_EXAMPLE_LAN867X_USE_PLCA
    // Configure PLCA as coordinator
    uint8_t plca_nodes_count = CONFIG_EXAMPLE_LAN867X_PLCA_NODE_COUNT;
    esp_eth_ioctl(eth_handles[0], LAN867X_ETH_CMD_S_PLCA_NCNT, &plca_nodes_count);
    uint8_t plca_id = 0;
    esp_eth_ioctl(eth_handles[0], LAN867X_ETH_CMD_S_PLCA_ID, &plca_id);
    uint8_t plca_max_burst_count = 0;
    esp_eth_ioctl(eth_handles[0], LAN867X_ETH_CMD_S_MAX_BURST_COUNT, &plca_max_burst_count);
    bool plca_en = true;
    esp_eth_ioctl(eth_handles[0], LAN867X_ETH_CMD_S_EN_PLCA, &plca_en);
#endif // otherwise rely on CSMA/CD
    // Start Ethernet driver state machine
    ESP_ERROR_CHECK(esp_eth_start(eth_handles[0]));
    // Initialize Berkeley socket which will listen on port SOCKET_PORT for transmission from client
    char rxbuffer[SOCKET_MAX_LENGTH] = {0};
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);
    fd_set ready;
    struct timeval check_pending_connections_timeout = {.tv_sec = 0, .tv_usec = 500000};
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(SOCKET_PORT);
    bind(server_fd, (struct sockaddr *)&address, sizeof(address));
    // listen and wait for transmission to come
    struct connection_info connections[LISTENER_MAX_QUEUE];
    int active_connections_count = 0;
    listen(server_fd, LISTENER_MAX_QUEUE);
    while (1) {
        // Check if any connections are pending
        FD_ZERO(&ready);
        FD_SET(server_fd, &ready);
        select(server_fd + 1, &ready, NULL, NULL, &check_pending_connections_timeout);
        if (FD_ISSET(server_fd, &ready) && active_connections_count < LISTENER_MAX_QUEUE) {
            // accept new connection
            struct sockaddr_in *current_address_ptr = &connections[active_connections_count].address;
            connections[active_connections_count].fd = accept(server_fd, (struct sockaddr *) current_address_ptr, &addrlen);
            active_connections_count++;
        }
        for (int i = 0; i < active_connections_count; i++) {
            // when transmission comes - print it
            memset(rxbuffer, 0, SOCKET_MAX_LENGTH);
            read(connections[i].fd, rxbuffer, SOCKET_MAX_LENGTH);
            printf("Received: \"%s\" from %s.\n", rxbuffer, inet_ntoa(connections[i].address.sin_addr));
        }
    }
}
