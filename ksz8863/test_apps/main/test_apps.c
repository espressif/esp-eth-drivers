#include <stdio.h>
#include <string.h>
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_console.h"
#include "esp_eth_ksz8863.h"
#include "test_init_ksz8863.h"
#include "ksz8863_console_cmd.h"

static const char *TAG = "ksz8863_test_apps";

esp_eth_handle_t host_eth_handle = NULL;
esp_eth_handle_t p1_eth_handle = NULL;
esp_eth_handle_t p2_eth_handle = NULL;

/** Event handler for Ethernet events */
static void eth_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    uint8_t mac_addr[6] = {0};
    /* we can get the ethernet driver handle from event data */
    esp_eth_handle_t eth_handle = *(esp_eth_handle_t *) event_data;

    char port_str[7];
    if(eth_handle == p1_eth_handle) {
        strcpy(port_str, "Port 1");
    } else if(eth_handle == p2_eth_handle) {
        strcpy(port_str, "Port 2");
    } else if(eth_handle == host_eth_handle) {
        strcpy(port_str, "Host");
    } else {
        strcpy(port_str, "???");
    }

    switch (event_id) {
    case ETHERNET_EVENT_CONNECTED:
        esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
        ESP_LOGI(TAG, "Ethernet (%s) Link Up", port_str);
        ESP_LOGI(TAG, "Ethernet HW Addr %02x:%02x:%02x:%02x:%02x:%02x",
                 mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
        break;
    case ETHERNET_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "Ethernet (%s) Link Down", port_str);
        break;
    case ETHERNET_EVENT_START:
        ESP_LOGI(TAG, "Ethernet (%s) Started", port_str);
        break;
    case ETHERNET_EVENT_STOP:
        ESP_LOGI(TAG, "Ethernet (%s) Stopped", port_str);
        break;
    default:
        break;
    }
}

void app_main(void)
{
    esp_netif_t *eth_netif = NULL;

    esp_event_loop_create_default();
    esp_netif_init();
    init_ksz8863_auto(eth_netif, &host_eth_handle, &p1_eth_handle, &p2_eth_handle);
    esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL);
    // start Ethernet driver state machines
    esp_eth_start(p1_eth_handle);
    esp_eth_start(p2_eth_handle);
    esp_eth_start(host_eth_handle);
    // start console 
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.prompt = "ksz8863>";

    // install console REPL environment
    esp_console_dev_uart_config_t uart_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&uart_config, &repl_config, &repl));
    register_ksz8863_config_commands(p1_eth_handle, p2_eth_handle);
    // start console REPL
    ESP_ERROR_CHECK(esp_console_start_repl(repl));
}
