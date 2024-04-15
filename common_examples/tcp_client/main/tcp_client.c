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
#include "sdkconfig.h"

#define SOCKET_PORT         5000
#define SOCKET_MAX_LENGTH   128

static const char *TAG = "tcp_client";
static SemaphoreHandle_t x_got_ip_semaphore;

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
    xSemaphoreGive(x_got_ip_semaphore);
}

void app_main(void)
{
    // Create default event loop that running in background
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    // Initialize semaphore
    x_got_ip_semaphore = xSemaphoreCreateBinary();
    // Initialize Ethernet driver
    uint8_t eth_port_cnt = 0;
    esp_eth_handle_t *eth_handles;
    char if_key_str[10];
    char if_desc_str[10];
    ESP_ERROR_CHECK(ethernet_init_all(&eth_handles, &eth_port_cnt));
    esp_netif_init();
    // Register user defined event handers
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_event_handler, NULL));
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

        for (int i = 0; i < eth_port_cnt; i++) {
            sprintf(if_key_str, "ETH_%d", i);
            sprintf(if_desc_str, "eth%d", i);
            esp_netif_config.if_key = if_key_str;
            esp_netif_config.if_desc = if_desc_str;
            esp_netif_config.route_prio -= i * 5;
            esp_netif_t *eth_netif = esp_netif_new(&cfg_spi);

            // Attach Ethernet driver to TCP/IP stack
            ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handles[i])));
            esp_eth_start(eth_handles[i]);
        }
    }
    // Wait until IP address is assigned to this device
    xSemaphoreTake(x_got_ip_semaphore, portMAX_DELAY);
    ESP_LOGI(TAG, "TCP client has started, waiting for the server to accept a connection.");
    int client_fd, ret;
    struct sockaddr_in server;
    char rxbuffer[SOCKET_MAX_LENGTH] = {0};
    char txbuffer[SOCKET_MAX_LENGTH] = {0};
    client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_fd == -1) {
        ESP_LOGE(TAG, "Could not create the socket (errno: %d)", errno);
        goto err;
    }
    server.sin_family = AF_INET;
    server.sin_port = htons(SOCKET_PORT);
    server.sin_addr.s_addr = inet_addr(CONFIG_EXAMPLE_SERVER_IP_ADDRESS);
    ret = connect(client_fd, (struct sockaddr *)&server, sizeof(struct sockaddr));
    if (ret == -1) {
        ESP_LOGE(TAG, "An error has occurred while connecting to the server (errno: %d)", errno);
        goto err;
    }
    int transmission_cnt = 0;
    while (1) {
        snprintf(txbuffer, SOCKET_MAX_LENGTH, "Transmission #%d. Hello from ESP32 TCP client", ++transmission_cnt);
        ESP_LOGI(TAG, "Transmitting: \"%s\"", txbuffer);
        ret = send(client_fd, txbuffer, SOCKET_MAX_LENGTH, 0);
        if (ret == -1) {
            ESP_LOGE(TAG, "An error has occurred while sending data (errno: %d)", errno);
            break;
        }
        ret = recv(client_fd, rxbuffer, SOCKET_MAX_LENGTH, 0);
        if (ret == -1) {
            ESP_LOGE(TAG, "An error has occurred while receiving data (errno: %d)", errno);
        } else if (ret == 0) {
            break;  // done reading
        }
        ESP_LOGI(TAG, "Received \"%s\"", rxbuffer);
        memset(txbuffer, 0, SOCKET_MAX_LENGTH);
        memset(rxbuffer, 0, SOCKET_MAX_LENGTH);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    return;
err:
    close(client_fd);
    ESP_LOGI(TAG, "Program was stopped because an error occured");
}
