/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
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
#include "sdkconfig.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "esp_eth_driver.h"
#include "esp_eth_phy_dummy.h"

#include "lwip/inet.h"
#include "ping/ping_sock.h"

#define STARTUP_DELAY_MS 500

#define EMAC_CLK_OUT_180_GPIO 17
#define EMAC_CLK_IN_GPIO 0

static const char *TAG = "emac2emac";

#if !CONFIG_EXAMPLE_DHCP_SERVER_EN
static void cmd_ping_on_ping_success(esp_ping_handle_t hdl, void *args)
{
    uint8_t ttl;
    uint16_t seqno;
    uint32_t elapsed_time, recv_len;
    ip_addr_t target_addr;
    esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
    esp_ping_get_profile(hdl, ESP_PING_PROF_TTL, &ttl, sizeof(ttl));
    esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr, sizeof(target_addr));
    esp_ping_get_profile(hdl, ESP_PING_PROF_SIZE, &recv_len, sizeof(recv_len));
    esp_ping_get_profile(hdl, ESP_PING_PROF_TIMEGAP, &elapsed_time, sizeof(elapsed_time));
    printf("%" PRIu32 " bytes from %s icmp_seq=%" PRIu16 " ttl=%" PRIu16 " time=%" PRIu32 " ms\n",
           recv_len, ipaddr_ntoa((ip_addr_t *)&target_addr), seqno, ttl, elapsed_time);
}

static void cmd_ping_on_ping_timeout(esp_ping_handle_t hdl, void *args)
{
    uint16_t seqno;
    ip_addr_t target_addr;
    esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
    esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr, sizeof(target_addr));
    printf("From %s icmp_seq=%d timeout\n", ipaddr_ntoa((ip_addr_t *)&target_addr), seqno);
}

static void cmd_ping_on_ping_end(esp_ping_handle_t hdl, void *args)
{
    ip_addr_t target_addr;
    uint32_t transmitted;
    uint32_t received;
    uint32_t total_time_ms;
    uint32_t loss;

    esp_ping_get_profile(hdl, ESP_PING_PROF_REQUEST, &transmitted, sizeof(transmitted));
    esp_ping_get_profile(hdl, ESP_PING_PROF_REPLY, &received, sizeof(received));
    esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr, sizeof(target_addr));
    esp_ping_get_profile(hdl, ESP_PING_PROF_DURATION, &total_time_ms, sizeof(total_time_ms));

    if (transmitted > 0) {
        loss = (uint32_t)((1 - ((float)received) / transmitted) * 100);
    } else {
        loss = 0;
    }
    if (IP_IS_V4(&target_addr)) {
        printf("\n--- %s ping statistics ---\n", inet_ntoa(*ip_2_ip4(&target_addr)));
    } else {
        printf("\n--- %s ping statistics ---\n", inet6_ntoa(*ip_2_ip6(&target_addr)));
    }
    printf("%" PRIu32 " packets transmitted, %" PRIu32 " received, %" PRIu32 "%% packet loss, time %" PRIu32 "ms\n",
           transmitted, received, loss, total_time_ms);

    esp_ping_delete_session(hdl);
}

static void ping_start(const esp_ip4_addr_t *ip)
{
    esp_ping_config_t config = ESP_PING_DEFAULT_CONFIG();

    ip4_addr_set_u32(ip_2_ip4(&config.target_addr), ip->addr);
    config.target_addr.type = IPADDR_TYPE_V4; // currently only IPv4 for this example

    /* set callback functions */
    esp_ping_callbacks_t cbs = {
        .cb_args = NULL,
        .on_ping_success = cmd_ping_on_ping_success,
        .on_ping_timeout = cmd_ping_on_ping_timeout,
        .on_ping_end = cmd_ping_on_ping_end
    };
    esp_ping_handle_t ping;
    esp_ping_new_session(&config, &cbs, &ping);
    esp_ping_start(ping);
}
#endif // !CONFIG_EXAMPLE_DHCP_SERVER_EN

/** Event handler for Ethernet events */
static void eth_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
    uint8_t mac_addr[6] = {0};
    /* we can get the ethernet driver handle from event data */
    esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;

    switch (event_id) {
    case ETHERNET_EVENT_CONNECTED:
        esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
        ESP_LOGI(TAG, "Ethernet Link Up");
        ESP_LOGI(TAG, "Ethernet HW Addr %02x:%02x:%02x:%02x:%02x:%02x",
                 mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
#if CONFIG_EXAMPLE_DHCP_SERVER_EN
        esp_netif_dhcps_start(esp_netif_get_handle_from_ifkey("ETH_DEF"));
#endif
        break;
    case ETHERNET_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "Ethernet Link Down");
        break;
    case ETHERNET_EVENT_START:
        ESP_LOGI(TAG, "Ethernet Started");
        break;
    case ETHERNET_EVENT_STOP:
        ESP_LOGI(TAG, "Ethernet Stopped");
        break;
    default:
        break;
    }
}

/** Event handler for IP_EVENT_ETH_GOT_IP */
static void got_ip_event_handler(void *arg, esp_event_base_t event_base,
                                 int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
    const esp_netif_ip_info_t *ip_info = &event->ip_info;

    ESP_LOGI(TAG, "Ethernet Got IP Address");
    ESP_LOGI(TAG, "~~~~~~~~~~~");
    ESP_LOGI(TAG, "ETHIP:" IPSTR, IP2STR(&ip_info->ip));
    ESP_LOGI(TAG, "ETHMASK:" IPSTR, IP2STR(&ip_info->netmask));
    ESP_LOGI(TAG, "ETHGW:" IPSTR, IP2STR(&ip_info->gw));
    ESP_LOGI(TAG, "~~~~~~~~~~~");

#if !CONFIG_EXAMPLE_DHCP_SERVER_EN
    ping_start(&ip_info->gw);
#endif
}

#if CONFIG_EXAMPLE_RMII_CLK_SOURCE_DEV
IRAM_ATTR static void gpio_isr_handler(void *arg)
{
    BaseType_t high_task_wakeup = pdFALSE;
    TaskHandle_t task_handle = (TaskHandle_t)arg;

    vTaskNotifyGiveFromISR(task_handle, &high_task_wakeup);
    if (high_task_wakeup != pdFALSE) {
        portYIELD_FROM_ISR();
    }
}
#endif // CONFIG_EXAMPLE_RMII_CLK_SOURCE_DEV

void app_main(void)
{
    // ESP32 device which is RMII CLK source needs to wait with its Ethernet initialization for the "RMII CLK Sink Device"
    // since the RMII CLK input pin (GPIO0) is also used as a boot strap pin. If the "RMII CLK Source Device" didn't wait,
    // the "RMII CLK Sink Device" could boot into incorrect mode.
#if CONFIG_EXAMPLE_RMII_CLK_SOURCE_DEV
    esp_rom_gpio_pad_select_gpio(EMAC_CLK_OUT_180_GPIO);
    gpio_set_pull_mode(EMAC_CLK_OUT_180_GPIO, GPIO_FLOATING); // to not affect GPIO0 (so the Sink Device could be flashed)
    gpio_install_isr_service(0);
    gpio_config_t gpio_source_cfg = {
        .pin_bit_mask = (1ULL << CONFIG_EXAMPLE_CLK_SINK_READY_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_ANYEDGE
    };
    gpio_config(&gpio_source_cfg);
    TaskHandle_t task_handle = xTaskGetHandle(pcTaskGetName(NULL));
    gpio_isr_handler_add(CONFIG_EXAMPLE_CLK_SINK_READY_GPIO, gpio_isr_handler, task_handle);
    ESP_LOGW(TAG, "waiting for RMII CLK sink device interrupt");
    ESP_LOGW(TAG, "if RMII CLK sink device is already running, reset it by `EN` button");
    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (gpio_get_level(CONFIG_EXAMPLE_CLK_SINK_READY_GPIO) == 1) {
            break;
        }
    }
    ESP_LOGI(TAG, "starting Ethernet initialization");
#else
    gpio_config_t gpio_sink_cfg = {
        .pin_bit_mask = (1ULL << CONFIG_EXAMPLE_CLK_SINK_READY_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&gpio_sink_cfg);
    gpio_set_level(CONFIG_EXAMPLE_CLK_SINK_READY_GPIO, 0);
    vTaskDelay(pdMS_TO_TICKS(STARTUP_DELAY_MS));
    gpio_set_level(CONFIG_EXAMPLE_CLK_SINK_READY_GPIO, 1);
#endif // CONFIG_EXAMPLE_RMII_CLK_SOURCE_DEV

    // --- Initialize Ethernet driver ---
    // Init common MAC and PHY configs to default
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();

    // Update PHY config based on board specific configuration
    phy_config.reset_gpio_num = -1; // no HW reset

    // Init vendor specific MAC config to default
    eth_esp32_emac_config_t esp32_emac_config = ETH_ESP32_EMAC_DEFAULT_CONFIG();
    // Update vendor specific MAC config based on board configuration
    // No SMI, speed/duplex must be statically configured the same in both devices
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
    esp32_emac_config.smi_gpio.mdc_num = -1;
    esp32_emac_config.smi_gpio.mdio_num = -1;
#else
    esp32_emac_config.smi_mdc_gpio_num = -1;
    esp32_emac_config.smi_mdio_gpio_num = -1;
#endif
#if CONFIG_EXAMPLE_RMII_CLK_SOURCE_DEV
    esp32_emac_config.clock_config.rmii.clock_mode = EMAC_CLK_OUT;
    esp32_emac_config.clock_config.rmii.clock_gpio = EMAC_CLK_OUT_180_GPIO;
#else
    esp32_emac_config.clock_config.rmii.clock_mode = EMAC_CLK_EXT_IN;
    esp32_emac_config.clock_config.rmii.clock_gpio = EMAC_CLK_IN_GPIO;
#endif // CONFIG_EXAMPLE_RMII_CLK_SOURCE_DEV

    // Create new ESP32 Ethernet MAC instance
    esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&esp32_emac_config, &mac_config);
    // Create dummy PHY instance
    esp_eth_phy_t *phy = esp_eth_phy_new_dummy(&phy_config);

    // Init Ethernet driver to default and install it
    esp_eth_handle_t eth_handle = NULL;
    esp_eth_config_t config = ETH_DEFAULT_CONFIG(mac, phy);
#if CONFIG_EXAMPLE_RMII_CLK_SINK_DEV
    // REF RMII CLK sink device performs multiple EMAC init attempts since RMII CLK source device may not be ready yet
    int i;
    for (i = 1; i <= 5; i++) {
        ESP_LOGI(TAG, "Ethernet driver install attempt: %i", i);
        if (esp_eth_driver_install(&config, &eth_handle) == ESP_OK) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    if (i > 5) {
        ESP_LOGE(TAG, "Ethernet driver install failed");
        abort();
        return;
    }
#else
    ESP_ERROR_CHECK(esp_eth_driver_install(&config, &eth_handle));
#endif // CONFIG_EXAMPLE_RMII_CLK_SOURCE_DEV

    // Initialize TCP/IP network interface aka the esp-netif
    ESP_ERROR_CHECK(esp_netif_init());
    // Create default event loop that running in background
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Create instance of esp-netif for Ethernet
    esp_netif_inherent_config_t esp_netif_config = ESP_NETIF_INHERENT_DEFAULT_ETH();
#if CONFIG_EXAMPLE_DHCP_SERVER_EN
    esp_netif_config.flags = (esp_netif_flags_t)(ESP_NETIF_IPV4_ONLY_FLAGS(ESP_NETIF_DHCP_SERVER));
    esp_netif_config.ip_info = &_g_esp_netif_soft_ap_ip; // Use the same IP ranges as IDF's soft AP
    esp_netif_config.get_ip_event = 0;
    esp_netif_config.lost_ip_event = 0;
#endif
    esp_netif_config_t cfg = {
        .base = &esp_netif_config,
        .stack = ESP_NETIF_NETSTACK_DEFAULT_ETH
    };
    esp_netif_t *eth_netif = esp_netif_new(&cfg);
    // Attach Ethernet driver to TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handle)));

    // Register user defined event handers
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_event_handler, NULL));

    ESP_ERROR_CHECK(esp_eth_start(eth_handle));

#if CONFIG_EXAMPLE_RMII_CLK_SOURCE_DEV
    // Wait indefinitely or reset when "RMII CLK Sink Device" resets
    // We reset the "RMII CLK Source Device" to ensure there is no CLK at GPIO0 of the
    // "RMII CLK Sink Device" during startup
    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (gpio_get_level(CONFIG_EXAMPLE_CLK_SINK_READY_GPIO) == 0) {
            break;
        }
    }
    ESP_LOGW(TAG, "RMII CLK Sink device reset, I'm going to reset too!");
    esp_restart();
#endif // CONFIG_EXAMPLE_RMII_CLK_SOURCE_DEV
}
