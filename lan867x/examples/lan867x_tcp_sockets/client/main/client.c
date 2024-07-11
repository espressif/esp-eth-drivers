#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_eth_phy_lan867x.h"
#include "esp_event.h"
#include "esp_log.h"
#include "ethernet_init.h"
#include "lwip/sockets.h"
#include "sdkconfig.h"

#define SOCKET_ADDRESS      "192.168.1.1"
#define SOCKET_PORT         5000
#define SOCKET_MAX_LENGTH   128

static const char *TAG = "lan867x_client";
static SemaphoreHandle_t xGotIpSemaphore;

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
    xSemaphoreGive(xGotIpSemaphore);
}

void app_main(void)
{
    // Create default event loop that running in background
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    // Initialize semaphore
    xGotIpSemaphore = xSemaphoreCreateBinary();
    // Initialize Ethernet driver
    uint8_t eth_port_cnt = 0;
    esp_eth_handle_t *eth_handles;
    ESP_ERROR_CHECK(ethernet_init_all(&eth_handles, &eth_port_cnt));
    // Initialize TCP/IP network interface aka the esp-netif (should be called only once in application)
    ESP_ERROR_CHECK(esp_netif_init());
    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
    esp_netif_t *eth_netif = esp_netif_new(&cfg);
    // Attach Ethernet driver to TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handles[0])));
    // Register user defined event handers
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_event_handler, NULL));
    // Configure PLCA
#if CONFIG_EXAMPLE_LAN867X_USE_PLCA
    // Configure PLCA with node number from config
    uint8_t plca_id = CONFIG_EXAMPLE_LAN867X_PLCA_ID;
    esp_eth_ioctl(eth_handles[0], LAN867X_ETH_CMD_S_PLCA_ID, &plca_id);
    bool plca_en = true;
    esp_eth_ioctl(eth_handles[0], LAN867X_ETH_CMD_S_EN_PLCA, &plca_en);
#endif // otherwise rely on CSMA/CD
    // Start Ethernet driver
    esp_eth_start(eth_handles[0]);
    // Get mac address and save it as a string
    char mac_str[18];
    uint8_t mac_data[6];
    esp_eth_ioctl(eth_handles[0], ETH_CMD_G_MAC_ADDR, &mac_data);
    sprintf(mac_str, "%02x:%02x:%02x:%02x:%02x:%02x", mac_data[0], mac_data[1], mac_data[2], mac_data[3], mac_data[4], mac_data[5]);
    // Initialize Berkley socket
    char txbuffer[SOCKET_MAX_LENGTH] = {0};
    int client_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    inet_pton(AF_INET, SOCKET_ADDRESS, &serv_addr.sin_addr);
    serv_addr.sin_port = htons(SOCKET_PORT);
    // Wait until IP address is assigned to this device
    xSemaphoreTake(xGotIpSemaphore, portMAX_DELAY);
    int transmission_cnt = 0;
    connect(client_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    while (1) {
        snprintf(txbuffer, SOCKET_MAX_LENGTH, "Transmission #%d. Hello from ESP32 (%s) via LAN867x", ++transmission_cnt, mac_str);
        int bytesSent = send(client_fd, txbuffer, SOCKET_MAX_LENGTH, 0);
        ESP_LOGI(TAG, "Sent tranmission #%d which was %d bytes long.", transmission_cnt, bytesSent);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    close(client_fd);
}
