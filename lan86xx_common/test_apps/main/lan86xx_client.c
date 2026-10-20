/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_idf_version.h"
#include "esp_console.h"
#include "ping/ping_sock.h"
#include "iperf_cmd.h"
#include "ethernet_init.h"
#include "lwip/sockets.h"
#include "sdkconfig.h"
#include "errno.h"

#define INVALID_SOCKET      -1
#define SOCKET_MAX_LENGTH   1440
#define MAX_MSG_LENGTH      128

static const char *TAG = "lan86xx_client";
static SemaphoreHandle_t got_ip_sem;

typedef struct {
    SemaphoreHandle_t done;
    uint32_t transmitted;
    uint32_t received;
} ping_ctx_t;

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

static void on_ping_success(esp_ping_handle_t hdl, void *args)
{
    ping_ctx_t *ctx = (ping_ctx_t *)args;
    esp_ping_get_profile(hdl, ESP_PING_PROF_REQUEST, &ctx->transmitted, sizeof(ctx->transmitted));
    esp_ping_get_profile(hdl, ESP_PING_PROF_REPLY, &ctx->received, sizeof(ctx->received));
}

static void on_ping_timeout(esp_ping_handle_t hdl, void *args)
{
    ping_ctx_t *ctx = (ping_ctx_t *)args;
    esp_ping_get_profile(hdl, ESP_PING_PROF_REQUEST, &ctx->transmitted, sizeof(ctx->transmitted));
    esp_ping_get_profile(hdl, ESP_PING_PROF_REPLY, &ctx->received, sizeof(ctx->received));
}

static void on_ping_end(esp_ping_handle_t hdl, void *args)
{
    ping_ctx_t *ctx = (ping_ctx_t *)args;
    esp_ping_get_profile(hdl, ESP_PING_PROF_REQUEST, &ctx->transmitted, sizeof(ctx->transmitted));
    esp_ping_get_profile(hdl, ESP_PING_PROF_REPLY, &ctx->received, sizeof(ctx->received));
    xSemaphoreGive(ctx->done);
}

static int ping_cmd(int argc, char **argv)
{
    if (argc != 2) {
        ESP_LOGE(TAG, "Usage: ping <ip>");
        return 1;
    }

    ip_addr_t target_addr = {0};
    if (!ipaddr_aton(argv[1], &target_addr)) {
        ESP_LOGE(TAG, "Invalid IP address: %s", argv[1]);
        return 1;
    }

    ping_ctx_t ctx = {
        .done = xSemaphoreCreateBinary(),
        .transmitted = 0,
        .received = 0,
    };
    if (ctx.done == NULL) {
        ESP_LOGE(TAG, "Failed to create ping semaphore");
        return 1;
    }

    esp_ping_config_t config = ESP_PING_DEFAULT_CONFIG();
    config.target_addr = target_addr;
    config.count = 3;
    config.interval_ms = 250;
    config.timeout_ms = 1000;

    esp_ping_callbacks_t cbs = {
        .cb_args = &ctx,
        .on_ping_success = on_ping_success,
        .on_ping_timeout = on_ping_timeout,
        .on_ping_end = on_ping_end,
    };

    esp_ping_handle_t ping = NULL;
    esp_err_t err = esp_ping_new_session(&config, &cbs, &ping);
    if (err != ESP_OK || ping == NULL) {
        ESP_LOGE(TAG, "Failed to create ping session: %s", esp_err_to_name(err));
        vSemaphoreDelete(ctx.done);
        return 1;
    }

    esp_ping_start(ping);
    if (xSemaphoreTake(ctx.done, pdMS_TO_TICKS(6000)) != pdTRUE) {
        ESP_LOGE(TAG, "Ping timed out waiting for completion");
        esp_ping_stop(ping);
        esp_ping_delete_session(ping);
        vSemaphoreDelete(ctx.done);
        return 1;
    }

    esp_ping_stop(ping);
    esp_ping_delete_session(ping);
    vSemaphoreDelete(ctx.done);

    if (ctx.received > 0) {
        ESP_LOGI(TAG, "PING_RESULT %s OK rx=%" PRIu32 "/%" PRIu32, argv[1], ctx.received, ctx.transmitted);
        return 0;
    }

    ESP_LOGE(TAG, "PING_RESULT %s FAIL rx=%" PRIu32 "/%" PRIu32, argv[1], ctx.received, ctx.transmitted);
    return 1;
}

static void start_diag_console(void)
{
    const esp_console_cmd_t ping = {
        .command = "ping",
        .help = "Ping IPv4 target: ping <ip>",
        .hint = NULL,
        .func = &ping_cmd,
        .argtable = NULL,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&ping));
    ESP_ERROR_CHECK(iperf_cmd_register_iperf());

    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.prompt = "lan86xx>";
    esp_console_dev_uart_config_t uart_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&uart_config, &repl_config, &repl));
    ESP_ERROR_CHECK(esp_console_start_repl(repl));
}

void app_main(void)
{
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    got_ip_sem = xSemaphoreCreateBinary();
    if (got_ip_sem == NULL) {
        ESP_LOGE(TAG, "Failed to create semaphore");
        return;
    }

    uint8_t eth_port_cnt = 0;
    esp_eth_handle_t *eth_handles;
    ESP_ERROR_CHECK(ethernet_init_all(&eth_handles, &eth_port_cnt));
    ESP_ERROR_CHECK(esp_netif_init());

    if (eth_port_cnt == 1) {
        esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
        esp_netif_t *eth_netif = esp_netif_new(&cfg);
        ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handles[0])));
    } else {
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
            ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handles[i])));
        }
    }

    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_event_handler, NULL));

    for (int i = 0; i < eth_port_cnt; i++) {
        ESP_ERROR_CHECK(esp_eth_start(eth_handles[i]));
    }

    int transmission_cnt = 0;
    int client_fd = INVALID_SOCKET;

    char txbuffer[MAX_MSG_LENGTH] = {0};
    char rxbuffer[SOCKET_MAX_LENGTH] = {0};
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    if (inet_pton(AF_INET, CONFIG_EXAMPLE_SERVER_IP_ADDRESS, &serv_addr.sin_addr) <= 0) {
        ESP_LOGE(TAG, "Invalid address or address not supported: errno %d", errno);
        goto err;
    }
    serv_addr.sin_port = htons(CONFIG_EXAMPLE_SERVER_PORT);

    ESP_LOGI(TAG, "Waiting for IP address...");
    if (xSemaphoreTake(got_ip_sem, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to get IP address");
        goto err;
    }
    ESP_LOGI(TAG, "PLCA follower ready, server is at %s:%d",
             CONFIG_EXAMPLE_SERVER_IP_ADDRESS, CONFIG_EXAMPLE_SERVER_PORT);

#if !CONFIG_EXAMPLE_CLIENT_CONNECT_TO_SERVER
    ESP_LOGI(TAG, "DHCP-only mode enabled; skipping TCP client traffic.");
    start_diag_console();
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 5, 0)
    while (1) {
        vTaskDelay(portMAX_DELAY);
    }
#endif
#endif

    while (1) {
        ESP_LOGI(TAG, "Trying to connect to server...");
        if (client_fd < 0) {
            client_fd = socket(AF_INET, SOCK_STREAM, 0);
            if (client_fd < 0) {
                ESP_LOGE(TAG, "Failed to create socket: errno %d", errno);
                goto err;
            }
        }

        ESP_LOGI(TAG, "Connecting to server %s:%d", CONFIG_EXAMPLE_SERVER_IP_ADDRESS, CONFIG_EXAMPLE_SERVER_PORT);
        if (connect(client_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            ESP_LOGE(TAG, "Failed to connect to server: errno %d", errno);
        } else {
            ESP_LOGI(TAG, "Connected to server");

            while (1) {
                snprintf(txbuffer, MAX_MSG_LENGTH, "Transmission #%d. Hello from ESP32 TCP client!\n", ++transmission_cnt);
                int bytesSent = send(client_fd, txbuffer, strlen(txbuffer), 0);

                if (bytesSent < 0) {
                    ESP_LOGE(TAG, "Failed to send data: errno %d", errno);
                    break;
                }

                ESP_LOGI(TAG, "Sent transmission #%d, %d bytes", transmission_cnt, bytesSent);

                int bytesRead = recv(client_fd, rxbuffer, SOCKET_MAX_LENGTH, 0);
                if (bytesRead < 0) {
                    ESP_LOGE(TAG, "Error reading from socket: errno %d", errno);
                    break;
                } else if (bytesRead == 0) {
                    ESP_LOGW(TAG, "Server closed connection");
                    break;
                } else {
                    rxbuffer[bytesRead] = '\0';
                    ESP_LOGI(TAG, "Received %d bytes: %s", bytesRead, rxbuffer);
                }
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
        }

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
