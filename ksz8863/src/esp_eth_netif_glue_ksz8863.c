/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdlib.h>
#include <inttypes.h>
#include "esp_eth_netif_glue_ksz8863.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_eth_ksz8863.h"
#include "esp_vfs_l2tap.h"

const static char *TAG = "ksz8863_switch_netif_glue";

typedef struct ksz8863_esp_eth_netif_glue_switch_t ksz8863_esp_eth_netif_glue_switch_t;

struct ksz8863_esp_eth_netif_glue_switch_t {
    esp_netif_driver_base_t base;
    esp_eth_handle_t host_eth_driver;
    esp_netif_t *p1_esp_netif;
    esp_netif_t *p2_esp_netif;
    esp_eth_handle_t p1_eth_driver;
    esp_eth_handle_t p2_eth_driver;
    esp_event_handler_instance_t start_ctx_handler;
    esp_event_handler_instance_t stop_ctx_handler;
    esp_event_handler_instance_t connect_ctx_handler;
    esp_event_handler_instance_t disconnect_ctx_handler;
    esp_event_handler_instance_t get_ip_ctx_handler;
};

static esp_err_t ksz88863_eth_input_to_netif(esp_eth_handle_t eth_handle, uint8_t *buffer, uint32_t length, void *priv)
{
#if CONFIG_ESP_NETIF_L2_TAP
    esp_err_t ret = ESP_OK;
    ret = esp_vfs_l2tap_eth_filter(eth_handle, buffer, (size_t *)&length);
    if (length == 0) {
        return ret;
    }
#endif
    return esp_netif_receive((esp_netif_t *)priv, buffer, length, NULL);
}

static void eth_host_l2_free(void *h, void *buffer)
{
    free(buffer);
}

static esp_err_t ksz8863_esp_eth_switch_post_attach(esp_netif_t *esp_netif, void *args)
{
    esp_err_t ret = ESP_OK;
    uint8_t eth_mac[6];
    ksz8863_esp_eth_netif_glue_switch_t *esp_netif_switch_glue = (ksz8863_esp_eth_netif_glue_switch_t *)args;
    esp_netif_switch_glue->base.netif = esp_netif;

    // Check if Tail tagging is enabled
    bool tail_tag_en = false;
    esp_eth_ioctl(esp_netif_switch_glue->p1_eth_driver, KSZ8863_ETH_CMD_G_TAIL_TAG, &tail_tag_en);
    ESP_GOTO_ON_FALSE(tail_tag_en == true, ESP_ERR_INVALID_STATE, err, TAG, "Tail Tagging must be enabled");

    // 1) Register ports which are to be forwarded from Host Ethernet to Port Ethernet interfaces (so to be accessible by L2 TAP)
    // 2) Update input path of the Port Interfaces to forward traffic to IP Stack
    if (esp_netif_switch_glue->p1_eth_driver) {
        ksz8863_register_tail_tag_port(esp_netif_switch_glue->p1_eth_driver, 0);
        esp_eth_update_input_path(esp_netif_switch_glue->p1_eth_driver, ksz88863_eth_input_to_netif, esp_netif);
        ESP_LOGD(TAG, "port 1 registered for Tail Tag forwarding");
    }
    if (esp_netif_switch_glue->p2_eth_driver) {
        ksz8863_register_tail_tag_port(esp_netif_switch_glue->p2_eth_driver, 1);
        esp_eth_update_input_path(esp_netif_switch_glue->p2_eth_driver, ksz88863_eth_input_to_netif, esp_netif);
        ESP_LOGD(TAG, "port 2 registered for Tail Tag forwarding");
    }
    // Host Ethernet input function forwards traffic to Port interfaces
    esp_eth_update_input_path(esp_netif_switch_glue->host_eth_driver, ksz8863_eth_tail_tag_port_forward, NULL);

    ESP_ERROR_CHECK(ksz8863_register_host_eth_hndl(esp_netif_switch_glue->host_eth_driver));

    // set driver related config to esp-netif
    esp_netif_driver_ifconfig_t driver_ifconfig = {
        .handle =  esp_netif_switch_glue->host_eth_driver,
        .transmit = ksz8863_eth_transmit_normal_lookup, // we want to transmit IP traffic with normal address lookup to KSZ (TatilTag = 0)
        .driver_free_rx_buffer = eth_host_l2_free
    };

    ESP_ERROR_CHECK(esp_netif_set_driver_config(esp_netif, &driver_ifconfig));
    esp_eth_ioctl(esp_netif_switch_glue->host_eth_driver, ETH_CMD_G_MAC_ADDR, eth_mac);
    ESP_LOGI(TAG, "%02x:%02x:%02x:%02x:%02x:%02x", eth_mac[0], eth_mac[1],
             eth_mac[2], eth_mac[3], eth_mac[4], eth_mac[5]);

    esp_netif_set_mac(esp_netif, eth_mac);
    ESP_LOGI(TAG, "switch interface attached to netif");

err:
    return ret;
}

static void eth_action_start(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;
    ksz8863_esp_eth_netif_glue_switch_t *esp_netif_switch_glue = handler_args;
    ESP_LOGD(TAG, "eth_action_start: %p, %p, %" PRIi32 ", %p, %p", esp_netif_switch_glue, base, event_id, event_data, *(esp_eth_handle_t *)event_data);
    if (esp_netif_switch_glue->host_eth_driver == eth_handle) {
        esp_netif_action_start(esp_netif_switch_glue->base.netif, base, event_id, event_data);
    }
}

static void eth_action_stop(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;
    ksz8863_esp_eth_netif_glue_switch_t *esp_netif_switch_glue = handler_args;
    ESP_LOGD(TAG, "eth_action_stop: %p, %p, %" PRIi32 ", %p, %p", esp_netif_switch_glue, base, event_id, event_data, *(esp_eth_handle_t *)event_data);
    if (esp_netif_switch_glue->host_eth_driver == eth_handle) {
        esp_netif_action_stop(esp_netif_switch_glue->base.netif, base, event_id, event_data);
    }
}

static void eth_action_connected(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;
    ksz8863_esp_eth_netif_glue_switch_t *esp_netif_switch_glue = handler_args;
    ESP_LOGD(TAG, "eth_action_connected: %p, %p, %" PRIi32 ", %p, %p", esp_netif_switch_glue, base, event_id, event_data, *(esp_eth_handle_t *)event_data);
    if (esp_netif_switch_glue->host_eth_driver == eth_handle) {
        esp_netif_action_connected(esp_netif_switch_glue->base.netif, base, event_id, event_data);
    }
}

static void eth_action_disconnected(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;
    ksz8863_esp_eth_netif_glue_switch_t *esp_netif_switch_glue = handler_args;
    ESP_LOGD(TAG, "eth_action_disconnected: %p, %p, %" PRIi32 ", %p, %p", esp_netif_switch_glue, base, event_id, event_data, *(esp_eth_handle_t *)event_data);
    if (esp_netif_switch_glue->host_eth_driver == eth_handle) {
        esp_netif_action_disconnected(esp_netif_switch_glue->base.netif, base, event_id, event_data);
    }
}

static void eth_action_got_ip(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *ip_event = (ip_event_got_ip_t *)event_data;
    ksz8863_esp_eth_netif_glue_switch_t *esp_netif_switch_glue = handler_args;
    ESP_LOGD(TAG, "eth_action_got_ip: %p, %p, %" PRIi32 ", %p, %p", esp_netif_switch_glue, base, event_id, event_data, *(esp_eth_handle_t *)event_data);
    if (esp_netif_switch_glue->base.netif == ip_event->esp_netif) {
        esp_netif_action_got_ip(ip_event->esp_netif, base, event_id, event_data);
    }
}

static esp_err_t esp_eth_clear_glue_instance_handlers(ksz8863_esp_eth_netif_glue_switch_t *eth_netif_glue)
{
    ESP_RETURN_ON_FALSE(eth_netif_glue, ESP_ERR_INVALID_ARG, TAG, "eth_netif_glue handle can't be null");

    if (eth_netif_glue->start_ctx_handler) {
        esp_event_handler_instance_unregister(ETH_EVENT, ETHERNET_EVENT_START, eth_netif_glue->start_ctx_handler);
        eth_netif_glue->start_ctx_handler = NULL;
    }

    if (eth_netif_glue->stop_ctx_handler) {
        esp_event_handler_instance_unregister(ETH_EVENT, ETHERNET_EVENT_STOP, eth_netif_glue->stop_ctx_handler);
        eth_netif_glue->stop_ctx_handler = NULL;
    }

    if (eth_netif_glue->connect_ctx_handler) {
        esp_event_handler_instance_unregister(ETH_EVENT, ETHERNET_EVENT_CONNECTED, eth_netif_glue->connect_ctx_handler);
        eth_netif_glue->connect_ctx_handler = NULL;
    }

    if (eth_netif_glue->disconnect_ctx_handler) {
        esp_event_handler_instance_unregister(ETH_EVENT, ETHERNET_EVENT_DISCONNECTED, eth_netif_glue->disconnect_ctx_handler);
        eth_netif_glue->disconnect_ctx_handler = NULL;
    }

    if (eth_netif_glue->get_ip_ctx_handler) {
        esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_ETH_GOT_IP, eth_netif_glue->get_ip_ctx_handler);
        eth_netif_glue->get_ip_ctx_handler = NULL;
    }

    return ESP_OK;
}

static esp_err_t esp_eth_set_glue_instance_handlers(ksz8863_esp_eth_netif_glue_switch_t *eth_netif_glue)
{
    ESP_RETURN_ON_FALSE(eth_netif_glue, ESP_ERR_INVALID_ARG, TAG, "eth_netif_glue handle can't be null");

    esp_err_t ret = esp_event_handler_instance_register(ETH_EVENT, ETHERNET_EVENT_START, eth_action_start, eth_netif_glue, &eth_netif_glue->start_ctx_handler);
    if (ret != ESP_OK) {
        goto fail;
    }

    ret = esp_event_handler_instance_register(ETH_EVENT, ETHERNET_EVENT_STOP, eth_action_stop, eth_netif_glue, &eth_netif_glue->stop_ctx_handler);
    if (ret != ESP_OK) {
        goto fail;
    }

    ret = esp_event_handler_instance_register(ETH_EVENT, ETHERNET_EVENT_CONNECTED, eth_action_connected, eth_netif_glue, &eth_netif_glue->connect_ctx_handler);
    if (ret != ESP_OK) {
        goto fail;
    }

    ret = esp_event_handler_instance_register(ETH_EVENT, ETHERNET_EVENT_DISCONNECTED, eth_action_disconnected, eth_netif_glue, &eth_netif_glue->disconnect_ctx_handler);
    if (ret != ESP_OK) {
        goto fail;
    }

    ret = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, eth_action_got_ip, eth_netif_glue, &eth_netif_glue->get_ip_ctx_handler);
    if (ret != ESP_OK) {
        goto fail;
    }

    return ESP_OK;

fail:
    esp_eth_clear_glue_instance_handlers(eth_netif_glue);
    return ret;
}

ksz8863_esp_eth_netif_glue_handle_t ksz8863_esp_eth_new_netif_glue_switch(ksz8863_esp_eth_netif_glue_config_t *config)
{
    ksz8863_esp_eth_netif_glue_switch_t *esp_netif_switch_glue = calloc(1, sizeof(ksz8863_esp_eth_netif_glue_switch_t));
    if (!esp_netif_switch_glue) {
        ESP_LOGE(TAG, "create netif glue failed");
        return NULL;
    }

    esp_netif_switch_glue->host_eth_driver = config->host_eth_hdl;
    esp_netif_switch_glue->p1_eth_driver = config->p1_eth_hndl;
    esp_netif_switch_glue->p2_eth_driver = config->p2_eth_hndl;
    esp_netif_switch_glue->base.post_attach = ksz8863_esp_eth_switch_post_attach;

    esp_eth_increase_reference(esp_netif_switch_glue->host_eth_driver);
    esp_eth_increase_reference(esp_netif_switch_glue->p1_eth_driver);
    esp_eth_increase_reference(esp_netif_switch_glue->p2_eth_driver);

    if (esp_eth_set_glue_instance_handlers(esp_netif_switch_glue) != ESP_OK) {
        ksz8863_esp_eth_del_netif_glue_switch(esp_netif_switch_glue);
        return NULL;
    }

    return esp_netif_switch_glue;
}

esp_err_t ksz8863_esp_eth_del_netif_glue_switch(ksz8863_esp_eth_netif_glue_handle_t esp_netif_glue)
{
    ksz8863_esp_eth_netif_glue_switch_t *esp_netif_switch_glue = (ksz8863_esp_eth_netif_glue_switch_t *)esp_netif_glue;

    esp_eth_clear_glue_instance_handlers(esp_netif_switch_glue);

    esp_eth_decrease_reference(esp_netif_switch_glue->host_eth_driver);
    esp_eth_decrease_reference(esp_netif_switch_glue->p1_eth_driver);
    esp_eth_decrease_reference(esp_netif_switch_glue->p2_eth_driver);
    free(esp_netif_switch_glue);
    return ESP_OK;
}
