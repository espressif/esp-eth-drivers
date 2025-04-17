/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
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
#include "sdkconfig.h"
#include "errno.h"

#define INVALID_SOCKET      -1
#define SOCKET_MAX_LENGTH   1440 // at least equal to MSS
#define MAX_MSG_LENGTH      128

static const char *TAG = "tcp_client";
static SemaphoreHandle_t got_ip_sem;

/** Event handler for IP_EVENT_ETH_GOT_IP */
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
    xSemaphoreGive(got_ip_sem);
}

void app_main(void)
{
    // Create default event loop that running in background
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    // Initialize semaphore
    got_ip_sem = xSemaphoreCreateBinary();
    if (got_ip_sem == NULL) {
        ESP_LOGE(TAG, "Failed to create semaphore");
        return;
    }

    // Initialize Ethernet driver
    uint8_t eth_port_cnt = 0;
    esp_eth_handle_t *eth_handles;
    ESP_ERROR_CHECK(ethernet_init_all(&eth_handles, &eth_port_cnt));
    // Initialize TCP/IP network interface aka the esp-netif (should be called only once in application)
    ESP_ERROR_CHECK(esp_netif_init());

    // Create instance(s) of esp-netif for Ethernet(s)
    if (eth_port_cnt == 1) {
        // Use ESP_NETIF_DEFAULT_ETH when just one Ethernet interface is used and you don't need to modify
        // default esp-netif configuration parameters.
        esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
        esp_netif_t *eth_netif = esp_netif_new(&cfg);
        // Attach Ethernet driver to TCP/IP stack
        ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handles[0])));
    } else {
        // Use ESP_NETIF_INHERENT_DEFAULT_ETH when multiple Ethernet interfaces are used and so you need to modify
        // esp-netif configuration parameters for each interface (name, priority, etc.).
        esp_netif_inherent_config_t esp_netif_config = ESP_NETIF_INHERENT_DEFAULT_ETH();
        esp_netif_config_t cfg_spi = {
            .base = &esp_netif_config,
            .stack = ESP_NETIF_NETSTACK_DEFAULT_ETH
        };

        char if_key_str[10];
        char if_desc_str[10];
        for (int i = 0; i < eth_port_cnt; i++) {
            sprintf(if_key_str, "ETH_%d", i);
            sprintf(if_desc_str, "eth%d", i);
            esp_netif_config.if_key = if_key_str;
            esp_netif_config.if_desc = if_desc_str;
            esp_netif_config.route_prio -= i * 5;
            esp_netif_t *eth_netif = esp_netif_new(&cfg_spi);

            // Attach Ethernet driver to TCP/IP stack
            ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handles[i])));
        }
    }

    // Register user defined event handers
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_event_handler, NULL));

    // Start Ethernet driver state machine
    for (int i = 0; i < eth_port_cnt; i++) {
        ESP_ERROR_CHECK(esp_eth_start(eth_handles[i]));
    }

    int transmission_cnt = 0;
    int client_fd = INVALID_SOCKET;

    // Initialize server address
    char txbuffer[MAX_MSG_LENGTH] = {0};
    char rxbuffer[SOCKET_MAX_LENGTH] = {0};
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    if (inet_pton(AF_INET, CONFIG_EXAMPLE_SERVER_IP_ADDRESS, &serv_addr.sin_addr) <= 0) {
        ESP_LOGE(TAG, "Invalid address or address not supported: errno %d", errno);
        goto err;
    }
    serv_addr.sin_port = htons(CONFIG_EXAMPLE_SERVER_PORT);

    // Wait until IP address is assigned to this device
    ESP_LOGI(TAG, "Waiting for IP address...");
    if (xSemaphoreTake(got_ip_sem, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to get IP address");
        goto err;
    }

    // Main connection loop
    while (1) {
        ESP_LOGI(TAG, "Trying to connect to server...");
        // Create socket if needed
        if (client_fd < 0) {
            client_fd = socket(AF_INET, SOCK_STREAM, 0);
            if (client_fd < 0) {
                ESP_LOGE(TAG, "Failed to create socket: errno %d", errno);
                goto err;
            }
        }

        // Try to connect to server
        ESP_LOGI(TAG, "Connecting to server %s:%d", CONFIG_EXAMPLE_SERVER_IP_ADDRESS, CONFIG_EXAMPLE_SERVER_PORT);
        if (connect(client_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            ESP_LOGE(TAG, "Failed to connect to server: errno %d", errno);
        } else {
            ESP_LOGI(TAG, "Connected to server");

            // Communication loop - runs until disconnection
            while (1) {
                snprintf(txbuffer, MAX_MSG_LENGTH, "Transmission #%d. Hello from ESP32 TCP client!\n", ++transmission_cnt);
                int bytesSent = send(client_fd, txbuffer, strlen(txbuffer), 0);

                if (bytesSent < 0) {
                    ESP_LOGE(TAG, "Failed to send data: errno %d", errno);
                    break; // Exit inner loop to reconnect
                }

                ESP_LOGI(TAG, "Sent transmission #%d, %d bytes", transmission_cnt, bytesSent);

                // Receive response from server
                int bytesRead = recv(client_fd, rxbuffer, SOCKET_MAX_LENGTH, 0);
                if (bytesRead < 0) {
                    ESP_LOGE(TAG, "Error reading from socket: errno %d", errno);
                    break; // Exit inner loop to reconnect
                } else if (bytesRead == 0) {
                    ESP_LOGW(TAG, "Server closed connection");
                    break; // Exit inner loop to reconnect
                } else {
                    rxbuffer[bytesRead] = '\0'; // Null-terminate the received data
                    ESP_LOGI(TAG, "Received %d bytes: %s", bytesRead, rxbuffer);
                }
                // Delay between transmissions
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
        }
        // If we get here, connection was lost, close socket and wait before reconnecting
        if (client_fd != INVALID_SOCKET) {
            ESP_LOGE(TAG, "Shutting down socket and restarting...");
            shutdown(client_fd, 0);
            close(client_fd);
            client_fd = INVALID_SOCKET;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
err:
    if (got_ip_sem) {
        vSemaphoreDelete(got_ip_sem);
    }
    if (client_fd != INVALID_SOCKET) {
        shutdown(client_fd, 0);
        close(client_fd);
    }
}
