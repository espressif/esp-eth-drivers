/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_eth.h"
#include "esp_console.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_idf_version.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_netif_br_glue.h"
#include "ping/ping_sock.h"
#include "iperf_cmd.h"
#include "ethernet_init.h"
#include "sdkconfig.h"

static const char *TAG = "lan86xx_bridge";

typedef struct {
    SemaphoreHandle_t done;
    uint32_t transmitted;
    uint32_t received;
} ping_ctx_t;

static void eth_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    uint8_t mac_addr[6] = {0};
    esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;

    switch (event_id) {
    case ETHERNET_EVENT_CONNECTED:
        esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
        ESP_LOGI(TAG, "Ethernet (%p) Link Up", eth_handle);
        ESP_LOGI(TAG, "Ethernet HW Addr %02x:%02x:%02x:%02x:%02x:%02x",
                 mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
        break;
    case ETHERNET_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "Ethernet (%p) Link Down", eth_handle);
        break;
    case ETHERNET_EVENT_START:
        ESP_LOGI(TAG, "Ethernet (%p) Started", eth_handle);
        break;
    case ETHERNET_EVENT_STOP:
        ESP_LOGI(TAG, "Ethernet (%p) Stopped", eth_handle);
        break;
    default:
        break;
    }
}

static void got_ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
    const esp_netif_ip_info_t *ip_info = &event->ip_info;

    ESP_LOGI(TAG, "Bridge Got IP Address");
    ESP_LOGI(TAG, "~~~~~~~~~~~");
    ESP_LOGI(TAG, "ETHIP:" IPSTR, IP2STR(&ip_info->ip));
    ESP_LOGI(TAG, "ETHMASK:" IPSTR, IP2STR(&ip_info->netmask));
    ESP_LOGI(TAG, "ETHGW:" IPSTR, IP2STR(&ip_info->gw));
    ESP_LOGI(TAG, "~~~~~~~~~~~");
}

static void on_ping_success(esp_ping_handle_t hdl, void *args)
{
    ping_ctx_t *ctx = (ping_ctx_t *)args;
    uint32_t transmitted = 0;
    uint32_t received = 0;
    esp_ping_get_profile(hdl, ESP_PING_PROF_REQUEST, &transmitted, sizeof(transmitted));
    esp_ping_get_profile(hdl, ESP_PING_PROF_REPLY, &received, sizeof(received));
    ctx->transmitted = transmitted;
    ctx->received = received;
}

static void on_ping_timeout(esp_ping_handle_t hdl, void *args)
{
    ping_ctx_t *ctx = (ping_ctx_t *)args;
    uint32_t transmitted = 0;
    uint32_t received = 0;
    esp_ping_get_profile(hdl, ESP_PING_PROF_REQUEST, &transmitted, sizeof(transmitted));
    esp_ping_get_profile(hdl, ESP_PING_PROF_REPLY, &received, sizeof(received));
    ctx->transmitted = transmitted;
    ctx->received = received;
}

static void on_ping_end(esp_ping_handle_t hdl, void *args)
{
    ping_ctx_t *ctx = (ping_ctx_t *)args;
    uint32_t transmitted = 0;
    uint32_t received = 0;
    esp_ping_get_profile(hdl, ESP_PING_PROF_REQUEST, &transmitted, sizeof(transmitted));
    esp_ping_get_profile(hdl, ESP_PING_PROF_REPLY, &received, sizeof(received));
    ctx->transmitted = transmitted;
    ctx->received = received;
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
    repl_config.prompt = "t1sbridge>";
    esp_console_dev_uart_config_t uart_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&uart_config, &repl_config, &repl));
    ESP_ERROR_CHECK(esp_console_start_repl(repl));
}

void app_main(void)
{
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    uint8_t eth_port_cnt = 0;
    esp_eth_handle_t *eth_handles = NULL;
    ESP_ERROR_CHECK(ethernet_init_all(&eth_handles, &eth_port_cnt));
    ESP_ERROR_CHECK(esp_netif_init());

    // The same MAC address will be used for all Ethernet ports since the bridge acts as one device
    uint8_t common_mac_addr[ETH_ADDR_LEN];
    // If internal Ethernet is not supported by ESP32x SoC, Locally Administered OUI address might be returned.
    // Note that Locally Administered OUI range should be used only when testing on a LAN under your control!
    ESP_ERROR_CHECK(esp_read_mac(common_mac_addr, ESP_MAC_ETH));
    for (int i = 0; i < eth_port_cnt; i++) {
        ESP_ERROR_CHECK(esp_eth_ioctl(eth_handles[i], ETH_CMD_S_MAC_ADDR, common_mac_addr));
    }

    esp_netif_t **eth_netifs = calloc(eth_port_cnt, sizeof(esp_netif_t *));
    if (eth_netifs == NULL) {
        ESP_LOGE(TAG, "Failed to allocate Ethernet netif array");
        return;
    }

    esp_netif_inherent_config_t port_cfg = ESP_NETIF_INHERENT_DEFAULT_ETH();
    esp_netif_config_t netif_cfg = {
        .base = &port_cfg,
        .stack = ESP_NETIF_NETSTACK_DEFAULT_ETH,
    };

    char if_key_str[10];
    char if_desc_str[10];
    for (int i = 0; i < eth_port_cnt; i++) {
        snprintf(if_key_str, sizeof(if_key_str), "ETH_%d", i);
        snprintf(if_desc_str, sizeof(if_desc_str), "eth%d", i);
        port_cfg.if_key = if_key_str;
        port_cfg.if_desc = if_desc_str;
        port_cfg.route_prio = 50 - i;
        port_cfg.flags = 0; // zero for bridged physical ports
        eth_netifs[i] = esp_netif_new(&netif_cfg);
        ESP_ERROR_CHECK(esp_netif_attach(eth_netifs[i], esp_eth_new_netif_glue(eth_handles[i])));
    }

    esp_netif_inherent_config_t br_cfg_base = ESP_NETIF_INHERENT_DEFAULT_BR();
    esp_netif_config_t br_cfg = {
        .base = &br_cfg_base,
        .stack = ESP_NETIF_NETSTACK_DEFAULT_BR,
    };
    bridgeif_config_t bridgeif_cfg = {
        .max_fdb_dyn_entries = 32,
        .max_fdb_sta_entries = 4,
        .max_ports = eth_port_cnt,
    };
    br_cfg_base.bridge_info = &bridgeif_cfg;
    memcpy(br_cfg_base.mac, common_mac_addr, ETH_ADDR_LEN);

    esp_netif_t *br_netif = esp_netif_new(&br_cfg);
    esp_netif_br_glue_handle_t br_glue = esp_netif_br_glue_new();
    for (int i = 0; i < eth_port_cnt; i++) {
        ESP_ERROR_CHECK(esp_netif_br_glue_add_port(br_glue, eth_netifs[i]));
    }
    ESP_ERROR_CHECK(esp_netif_attach(br_netif, br_glue));

    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_event_handler, NULL));

    for (int i = 0; i < eth_port_cnt; i++) {
        bool promiscuous = true;
        ESP_ERROR_CHECK(esp_eth_ioctl(eth_handles[i], ETH_CMD_S_PROMISCUOUS, &promiscuous));
        ESP_ERROR_CHECK(esp_eth_start(eth_handles[i]));
    }

    ESP_LOGI(TAG, "Bridge up: %u Ethernet port(s)", eth_port_cnt);
    start_diag_console();
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 5, 0)
    while (1) {
        vTaskDelay(portMAX_DELAY);
    }
#endif
}
