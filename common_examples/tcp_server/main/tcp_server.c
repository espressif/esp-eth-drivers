/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_netif.h"
#include "esp_netif_net_stack.h"
#include "esp_eth.h"
#include "esp_event.h"
#include "esp_log.h"
#include "ethernet_init.h"
#include "lwip/sockets.h"
#include "dhcpserver/dhcpserver_options.h"

#define INVALID_SOCKET      -1
#define LISTENER_MAX_QUEUE  8
#define SOCKET_MAX_LENGTH   1440 // at least equal to MSS
#define MAX_MSG_LENGTH      128

static const char *TAG = "tcp_server";

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

#if CONFIG_EXAMPLE_ACT_AS_DHCP_SERVER
static void start_dhcp_server_after_connection(void *arg, esp_event_base_t base, int32_t id, void *event_data)
{
    // We have manipulation with esp_netifs under our control in the example, so we can use unsafe functions
    esp_netif_t *eth_netif = esp_netif_next_unsafe(NULL);
    esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;
    while (eth_netif != NULL) {
        esp_eth_handle_t eth_handle_for_current_netif = esp_netif_get_io_driver(eth_netif);
        if (memcmp(&eth_handle, &eth_handle_for_current_netif, sizeof(esp_eth_handle_t)) == 0) {
            esp_netif_dhcps_start(eth_netif);
            ESP_LOGI(TAG, "DHCP server started on %s\n", esp_netif_get_desc(eth_netif));
        }
        eth_netif = esp_netif_next_unsafe(eth_netif);
    }
}
#endif

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

    char if_key_str[10];
    char if_desc_str[10];
    esp_netif_config_t cfg;
    esp_netif_inherent_config_t eth_netif_cfg;
#if CONFIG_EXAMPLE_ACT_AS_DHCP_SERVER
    ESP_LOGI(TAG, "Example will act as DHCP server");
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
        // Set DHCP lease time
        uint32_t lease_opt = CONFIG_EXAMPLE_DHCP_LEASE_TIME;
        ESP_ERROR_CHECK(esp_netif_dhcps_option(eth_netif, ESP_NETIF_OP_SET, IP_ADDRESS_LEASE_TIME, &lease_opt, sizeof(lease_opt)));
        // Attach Ethernet driver to TCP/IP stack
        ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handles[i])));
    }
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ETHERNET_EVENT_CONNECTED, start_dhcp_server_after_connection, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, got_ip_event_handler, NULL));
    ESP_LOGI(TAG, "--------");
    // Start Ethernet driver state machine
    for (uint8_t i = 0; i < eth_port_cnt; i++) {
        ESP_ERROR_CHECK(esp_eth_start(eth_handles[i]));
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
        // Attach Ethernet driver to TCP/IP stack
        ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handles[i])));
    }
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, got_ip_event_handler, NULL));
    // Start Ethernet driver state machine
    for (int i = 0; i < eth_port_cnt; i++) {
        ESP_ERROR_CHECK(esp_eth_start(eth_handles[i]));
    }
#endif

    char *rxbuffer = NULL;
    char *txbuffer = NULL;

    // Initialize Berkeley socket which will listen on port EXAMPLE_TCP_SERVER_PORT for transmission from client
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        ESP_LOGE(TAG, "Failed to create socket: errno %d", errno);
        return;
    }

    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);
    fd_set ready;
    int enable = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0) {
        ESP_LOGE(TAG, "Failed to set socket option reuseaddr: errno %d", errno);
        goto err;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(CONFIG_EXAMPLE_TCP_SERVER_PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        ESP_LOGE(TAG, "Failed to bind socket: errno %d", errno);
        goto err;
    }

    // Listen and wait for transmission to come
    if (listen(server_fd, LISTENER_MAX_QUEUE) < 0) {
        ESP_LOGE(TAG, "Failed to listen on socket: errno %d", errno);
        goto err;
    }
    ESP_LOGI(TAG, "Server listening on port %d", CONFIG_EXAMPLE_TCP_SERVER_PORT);

    rxbuffer = (char *)malloc(SOCKET_MAX_LENGTH);
    if (rxbuffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate rxbuffer");
        goto err;
    }
    txbuffer = (char *)malloc(MAX_MSG_LENGTH);
    if (txbuffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate txbuffer");
        goto err;
    }

    struct connection_info connections[LISTENER_MAX_QUEUE];
    int active_connections_count = 0;
    int max_fd = server_fd;
    int transmission_cnt = 0;

    // Initialize connections array
    for (int i = 0; i < LISTENER_MAX_QUEUE; i++) {
        connections[i].fd = INVALID_SOCKET;
    }

    while (1) {
        // Clear and add server to set
        FD_ZERO(&ready);
        FD_SET(server_fd, &ready);

        // Add client(s) to set
        for (int i = 0; i < active_connections_count; i++) {
            int conn_fd = connections[i].fd;
            if (conn_fd > 0) {
                FD_SET(conn_fd, &ready);
                if (conn_fd > max_fd) {
                    max_fd = conn_fd;
                }
            }
        }

        // Wait for activity
        int activity = select(max_fd + 1, &ready, NULL, NULL, NULL);
        if (activity < 0) {
            ESP_LOGE(TAG, "Select error: errno %d", errno);
            continue;
        }

        // Check if new connections are pending
        if (FD_ISSET(server_fd, &ready) && active_connections_count < LISTENER_MAX_QUEUE) {
            // Accept new connection
            struct sockaddr_in *current_address_ptr = &connections[active_connections_count].address;
            int new_fd = accept(server_fd, (struct sockaddr *)current_address_ptr, &addrlen);

            if (new_fd < 0) {
                ESP_LOGE(TAG, "Failed to accept connection: errno %d", errno);
            } else {
                connections[active_connections_count].fd = new_fd;
                ESP_LOGI(TAG, "New connection accepted from %s:%d, socket fd: %d",
                         inet_ntoa(current_address_ptr->sin_addr),
                         ntohs(current_address_ptr->sin_port),
                         new_fd);
                active_connections_count++;
            }
        }

        // Check existing connections for data
        for (int i = 0; i < active_connections_count; i++) {
            int fd = connections[i].fd;

            // Check if client sent message
            if (fd > 0 && FD_ISSET(fd, &ready)) {
                memset(rxbuffer, 0, SOCKET_MAX_LENGTH);
                int n = read(fd, rxbuffer, SOCKET_MAX_LENGTH);

                if (n < 0) {
                    ESP_LOGE(TAG, "Error reading from socket: errno %d", errno);
                    close(fd);
                    connections[i].fd = INVALID_SOCKET;
                } else if (n == 0) {
                    // Connection closed by client
                    ESP_LOGI(TAG, "Client disconnected, socket fd: %d", fd);
                    close(fd);
                    connections[i].fd = INVALID_SOCKET;
                } else {
                    ESP_LOGI(TAG, "Received %d bytes from %s", n, inet_ntoa(connections[i].address.sin_addr));
                    for (int j = 0; j < n; j++) {
                        // ignore nulls
                        if (rxbuffer[j] != '\0') {
                            printf("%c", rxbuffer[j]);
                        }
                    }
                    // Send response back to client
                    snprintf(txbuffer, MAX_MSG_LENGTH, "Transmission #%d. Hello from ESP32 TCP server\n", ++transmission_cnt);
                    if (send(fd, txbuffer, strlen(txbuffer), 0) < 0) {
                        ESP_LOGE(TAG, "Failed to send response: errno %d", errno);
                    }
                }
            }
        }

        // Clean up disconnected clients and compact the array
        for (int i = 0; i < active_connections_count; i++) {
            if (connections[i].fd == INVALID_SOCKET) {
                // Move the last connection to this slot and reduce count
                if (i < active_connections_count - 1) {
                    connections[i] = connections[active_connections_count - 1];
                }
                active_connections_count--;
                i--; // Recheck this position
            }
        }
    }
err:
    if (rxbuffer) {
        free(rxbuffer);
    }
    if (txbuffer) {
        free(txbuffer);
    }
    if (server_fd != INVALID_SOCKET) {
        close(server_fd);
    }
}
