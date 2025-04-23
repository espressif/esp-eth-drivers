/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "arpa/inet.h" // for ntohs, etc.
#include "esp_log.h"
#include "esp_event.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_eth_driver.h"
#include "sdkconfig.h"
#include "eth_common.h"
#include "test_functions.h"
#include "freertos/queue.h"
#include <time.h> // TODO: use different source for seed?

#define TX_TASK_PRIO                (8)
#define TX_TASK_STACK_SIZE          (4096)
#define DEFAULT_TX_MESSAGE          "ESP32 HELLO"

#define TX_QUEUE_SIZE               (10)
#define RX_QUEUE_SIZE               (10)

#define ETH_TYPE                    (0x3300)

typedef struct {
    QueueHandle_t rx_frame_queue;
    uint16_t eth_type_filter;
    atomic_bool reset_rx_cnt;
    bool verbose;
} eth_recv_config_t;

typedef struct {
    esp_eth_handle_t eth_handle;
    TaskHandle_t calling_task;
    uint16_t frame_len;
    uint32_t count;
    uint32_t period_us;
    QueueHandle_t control_frame_queue;
    bool verbose;
    bool randomize;
} tx_task_config_t;

typedef struct {
    uint16_t frame_len;
    uint8_t *frame;
} frame_info_t;

static const char *TAG = "eth_phy_test_fncs";
static SemaphoreHandle_t s_verbose_mutex;

static esp_err_t print_frame(emac_frame_t *eth_frame, uint32_t length, uint32_t seq, bool is_recv)
{
    esp_err_t ret = ESP_OK;

    if (!s_verbose_mutex) {
        s_verbose_mutex = xSemaphoreCreateMutex();
        ESP_GOTO_ON_FALSE(s_verbose_mutex, ESP_ERR_NO_MEM, err, TAG, "create frame print lock failed");
    }
    ESP_GOTO_ON_FALSE(xSemaphoreTake(s_verbose_mutex, pdMS_TO_TICKS(100)) == pdTRUE, ESP_ERR_TIMEOUT, err, TAG,
                      "print frame timeout");
    if (is_recv) {
        printf("Received frame #%lu:\n", seq);
    } else {
        printf("Transmitted frame #%lu:\n", seq);
    }
    ESP_LOG_BUFFER_HEXDUMP("", eth_frame, length, ESP_LOG_INFO);
    xSemaphoreGive(s_verbose_mutex);
err:
    return ret;
}

static esp_err_t eth_input_cb(esp_eth_handle_t hdl, uint8_t *buffer, uint32_t length, void *priv)
{
    eth_recv_config_t *recv_config = (eth_recv_config_t *)priv;
    emac_frame_t *rx_eth_frame = (emac_frame_t *)buffer;
    static uint32_t recv_cnt = 0;

    bool exp_reset_rx_cnt = true;
    if (atomic_compare_exchange_strong(&recv_config->reset_rx_cnt, &exp_reset_rx_cnt, false)) {
        recv_cnt = 0;
    }

    if (recv_config->rx_frame_queue && (recv_config->eth_type_filter == 0xFFFF ||
                                        recv_config->eth_type_filter == ntohs(rx_eth_frame->proto))) {
        recv_cnt++;
        if (recv_config->verbose == true) {
            print_frame(rx_eth_frame, length, recv_cnt, true);
        }
        frame_info_t frame_info = {
            .frame_len = length,
            .frame = buffer
        };
        if (xQueueSend(recv_config->rx_frame_queue, &frame_info, pdMS_TO_TICKS(50)) != pdTRUE) {
            ESP_LOGE(TAG, "Rx queue full");
            free(buffer);
        }
    } else {
        free(buffer);
    }
    return ESP_OK;
}

static void free_queue(QueueHandle_t frame_queue)
{
    frame_info_t frame_info;
    while (xQueueReceive(frame_queue, &frame_info, pdMS_TO_TICKS(10)) == pdTRUE) {
        free(frame_info.frame);
    }
    vQueueDelete(frame_queue);
}

static void randomize_frame_payload(uint8_t *frame, uint32_t frame_length)
{
    srand(time(NULL));
    for (int i = ETH_HEADER_LEN + 1; i < frame_length; i++) { // +1 for sequence number offset
        frame[i] = rand() & 0xff;
    }
}

static void tx_task(void *arg)
{
    tx_task_config_t *tx_task_config = (tx_task_config_t *)arg;

    uint8_t *tx_buffer = calloc(sizeof(uint8_t), tx_task_config->frame_len);
    if (!tx_buffer) {
        ESP_LOGE(TAG, "no memory for TX frame buffer");
        goto err;
    }

    // prepare header of Ethernet frame
    emac_frame_t *tx_eth_frame = (emac_frame_t *)tx_buffer;
    memset(tx_eth_frame->dest, 0xFF, ETH_ADDR_LEN); // broadcast
    esp_eth_ioctl(tx_task_config->eth_handle, ETH_CMD_G_MAC_ADDR, tx_eth_frame->src);
    tx_eth_frame->proto = htons(ETH_TYPE);
    if ((tx_task_config->frame_len - ETH_HEADER_LEN) > sizeof(DEFAULT_TX_MESSAGE)) {
        strcpy((char *)&tx_eth_frame->data[1], DEFAULT_TX_MESSAGE);
    } else {
        ESP_LOGW(TAG, "Ethernet frame len is too small to fit default Tx message");
    }

    ESP_LOGI(TAG, "starting ETH broadcast transmissions with Ethertype: 0x%x", ETH_TYPE);
    for (uint32_t frame_id = 0; frame_id < tx_task_config->count; frame_id++) {
        tx_eth_frame->data[0] = frame_id & 0xff;
        // Randomize frame content if enabled
        if (tx_task_config->randomize) {
            randomize_frame_payload(tx_buffer, tx_task_config->frame_len);
            // TODO randomize length too??
        }
        // Queue control sample frame if enabled
        if (tx_task_config->control_frame_queue) {
            uint8_t *ctrl_sample_frame = malloc(tx_task_config->frame_len);
            if (!tx_buffer) {
                ESP_LOGE(TAG, "no memory for TX control sample frame");
                goto err; // it doesn't make sense to continue since the control samples are missing and so control check will fail
            }
            memcpy(ctrl_sample_frame, tx_eth_frame, tx_task_config->frame_len);
            frame_info_t frame_info = {
                .frame_len = tx_task_config->frame_len,
                .frame = ctrl_sample_frame
            };
            if (xQueueSend(tx_task_config->control_frame_queue, &frame_info, pdMS_TO_TICKS(50)) != pdTRUE) {
                ESP_LOGE(TAG, "control Tx queue full");
                free(ctrl_sample_frame);
                goto err; // it doesn't make sense to continue since the control samples are missing and so control check will fail
            }
        }
        // Transmit the frame
        if (esp_eth_transmit(tx_task_config->eth_handle, tx_eth_frame, tx_task_config->frame_len) != ESP_OK) {
            ESP_LOGE(TAG, "transmit failed");
        } else if (tx_task_config->verbose) {
            print_frame(tx_eth_frame, tx_task_config->frame_len, frame_id, false);
        }
        vTaskDelay(pdMS_TO_TICKS(tx_task_config->period_us / 1000));
    }
err:
    free(tx_buffer);
    // notify calling task that transmitting has been finished
    if (tx_task_config->calling_task) {
        xTaskNotifyGive(tx_task_config->calling_task);
    }
    vTaskDelete(NULL);
}

esp_err_t loop_server(
    esp_eth_handle_t *eth_handle,
    bool verbose,
    uint16_t eth_type,
    uint32_t timeout_ms)
{
    esp_err_t ret = ESP_OK;
    EventGroupHandle_t eth_event_group = NULL;
    QueueHandle_t rx_frame_queue = NULL;

    ESP_GOTO_ON_FALSE(eth_handle, ESP_ERR_INVALID_ARG, err, TAG, "invalid Ethernet handle");

    eth_event_group = create_eth_event_group();
    ESP_GOTO_ON_FALSE(eth_event_group != NULL, ESP_FAIL, err, TAG, "event init failed");

    rx_frame_queue = xQueueCreate(RX_QUEUE_SIZE, sizeof(frame_info_t));
    ESP_GOTO_ON_FALSE(rx_frame_queue, ESP_FAIL, err, TAG, "create rx queue failed");
    eth_recv_config_t recv_config = {
        .rx_frame_queue = rx_frame_queue,
        .eth_type_filter = eth_type,
        .verbose = verbose
    };
    atomic_store(&recv_config.reset_rx_cnt, true);
    ESP_GOTO_ON_ERROR(esp_eth_update_input_path(eth_handle, eth_input_cb, &recv_config), err, TAG, "ethernet input function configuration failed");

    ESP_GOTO_ON_ERROR(esp_eth_start(eth_handle), err, TAG, "failed to start Ethernet");
    EventBits_t bits = xEventGroupWaitBits(eth_event_group, ETH_CONNECT_BIT, true, true, pdMS_TO_TICKS(ETH_CONNECT_TIMEOUT_MS));
    ESP_GOTO_ON_FALSE(bits & ETH_CONNECT_BIT, ESP_ERR_TIMEOUT, err_stop, TAG, "link connect timeout");

    frame_info_t rx_frame_info;
    while (xQueueReceive(rx_frame_queue, &rx_frame_info, pdMS_TO_TICKS(timeout_ms)) == pdTRUE) {
        emac_frame_t *eth_frame = (emac_frame_t *)rx_frame_info.frame;
        memcpy(eth_frame->dest, eth_frame->src, ETH_ADDR_LEN);
        esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, eth_frame->src);
        if (esp_eth_transmit(eth_handle, eth_frame, rx_frame_info.frame_len) != ESP_OK) {
            ESP_LOGE(TAG, "transmit failed");
        }
    }
err_stop:
    ESP_GOTO_ON_ERROR(esp_eth_stop(eth_handle), err, TAG, "failed to stop Ethernet");
err:
    esp_eth_update_input_path(eth_handle, NULL, NULL);
    delete_eth_event_group(eth_event_group);
    return ret;
}

esp_err_t transmit_to_host(
    esp_eth_handle_t *eth_handle,
    bool verbose,
    uint32_t frame_length,
    uint32_t count,
    uint32_t period_us)
{
    esp_err_t ret = ESP_OK;
    EventGroupHandle_t eth_event_group = NULL;
    ESP_GOTO_ON_FALSE(eth_handle, ESP_ERR_INVALID_ARG, err, TAG, "invalid Ethernet handle");

    eth_event_group = create_eth_event_group();
    ESP_GOTO_ON_FALSE(eth_event_group != NULL, ESP_FAIL, err, TAG, "event init failed");

    ESP_GOTO_ON_ERROR(esp_eth_start(eth_handle), err, TAG, "failed to start Ethernet");
    EventBits_t bits = xEventGroupWaitBits(eth_event_group, ETH_CONNECT_BIT, true, true, pdMS_TO_TICKS(ETH_CONNECT_TIMEOUT_MS));
    ESP_GOTO_ON_FALSE(bits & ETH_CONNECT_BIT, ESP_ERR_TIMEOUT, err_stop, TAG, "link connect timeout");

    tx_task_config_t tx_task_config = {
        .eth_handle = eth_handle,
        .calling_task = xTaskGetCurrentTaskHandle(),
        .frame_len = frame_length,
        .count = count,
        .period_us = period_us,
        .control_frame_queue = NULL,
        .verbose = verbose,
        .randomize = false
    };

    TaskHandle_t task_hndl;
    BaseType_t xReturned = xTaskCreate(tx_task, "eth_tx_task", TX_TASK_STACK_SIZE, &tx_task_config, TX_TASK_PRIO, &task_hndl);
    ESP_GOTO_ON_FALSE(xReturned == pdPASS, ESP_FAIL, err_stop, TAG, "create emac_rx task failed");

    if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(period_us / 1000 * count * 120 / 100)) == 0) { // + 20%
        ESP_LOGE(TAG, "transmit task hasn't finished in expected timeout");
    };
err_stop:
    ESP_GOTO_ON_ERROR(esp_eth_stop(eth_handle), err, TAG, "failed to stop Ethernet");
err:
    delete_eth_event_group(eth_event_group);
    return ret;
}

esp_err_t loopback_near_end_test(
    esp_eth_handle_t *eth_handle,
    bool verbose,
    uint32_t frame_length,
    uint32_t count,
    uint32_t period_us)
{
    esp_err_t ret = ESP_OK;
    EventGroupHandle_t eth_event_group = NULL;
    QueueHandle_t rx_frame_queue = NULL;
    QueueHandle_t tx_frame_queue = NULL;

    ESP_GOTO_ON_FALSE(eth_handle, ESP_ERR_INVALID_ARG, err, TAG, "invalid Ethernet handle");

    eth_event_group = create_eth_event_group();
    ESP_GOTO_ON_FALSE(eth_event_group != NULL, ESP_FAIL, err, TAG, "event init failed");

    // Enable PHY near end loopback
    loopback_near_end_en(eth_handle, 0, true);

    rx_frame_queue = xQueueCreate(RX_QUEUE_SIZE, sizeof(frame_info_t));
    ESP_GOTO_ON_FALSE(rx_frame_queue, ESP_FAIL, err, TAG, "create rx queue failed");
    eth_recv_config_t recv_config = {
        .rx_frame_queue = rx_frame_queue,
        .eth_type_filter = 0xFFFF, // don't filter
        .verbose = verbose
    };
    atomic_store(&recv_config.reset_rx_cnt, true);
    ESP_GOTO_ON_ERROR(esp_eth_update_input_path(eth_handle, eth_input_cb, &recv_config), err, TAG, "ethernet input function configuration failed");

    ESP_GOTO_ON_ERROR(esp_eth_start(eth_handle), err, TAG, "failed to start Ethernet");
    EventBits_t bits = xEventGroupWaitBits(eth_event_group, ETH_CONNECT_BIT, true, true, pdMS_TO_TICKS(ETH_CONNECT_TIMEOUT_MS));
    ESP_GOTO_ON_FALSE(bits & ETH_CONNECT_BIT, ESP_ERR_TIMEOUT, err_stop, TAG, "link connect timeout");

    tx_frame_queue = xQueueCreate(TX_QUEUE_SIZE, sizeof(frame_info_t));
    ESP_GOTO_ON_FALSE(tx_frame_queue, ESP_FAIL, err, TAG, "create tx queue failed");

    tx_task_config_t tx_task_config = {
        .eth_handle = eth_handle,
        .calling_task = NULL,
        .frame_len = frame_length,
        .count = count,
        .period_us = period_us,
        .control_frame_queue = tx_frame_queue,
        .verbose = verbose,
        .randomize = true
    };

    TaskHandle_t task_hndl;
    BaseType_t xReturned = xTaskCreate(tx_task, "eth_tx_task", TX_TASK_STACK_SIZE, &tx_task_config, TX_TASK_PRIO, &task_hndl);
    ESP_GOTO_ON_FALSE(xReturned == pdPASS, ESP_FAIL, err_stop, TAG, "create emac_rx task failed");

    uint32_t rx_err_cnt = 0;
    uint32_t rx_cnt = 0;
    frame_info_t rx_frame_info;
    frame_info_t tx_frame_info;
    // go over received frames and compare them with control samples
    while (xQueueReceive(rx_frame_queue, &rx_frame_info, pdMS_TO_TICKS(period_us / 1000 * 2)) == pdTRUE) {
        if (xQueueReceive(tx_frame_queue, &tx_frame_info, pdMS_TO_TICKS(10)) == pdTRUE) {
            if (rx_frame_info.frame_len == tx_frame_info.frame_len) {
                if (memcmp(rx_frame_info.frame, tx_frame_info.frame, tx_frame_info.frame_len) == 0) {
                    rx_cnt++;
                } else {
                    ESP_LOGE(TAG, "unexpected content of received frame");
                    rx_err_cnt++;
                    // TODO: try to find the same frame => traverse over Tx queue
                }
            } else {
                ESP_LOGE(TAG, "unexpected length of received frame");
                rx_err_cnt++;
            }
            free(tx_frame_info.frame);
        }
        free(rx_frame_info.frame);
    }
    // TODO add wait for Tx task end
    ESP_LOGI(TAG, "looped frames: %lu, rx errors: %lu", rx_cnt, rx_err_cnt);
err_stop:
    ESP_GOTO_ON_ERROR(esp_eth_stop(eth_handle), err, TAG, "failed to stop Ethernet");
err:
    esp_eth_update_input_path(eth_handle, NULL, NULL);
    if (rx_frame_queue) {
        free_queue(rx_frame_queue);
    }
    if (tx_frame_queue) {
        free_queue(tx_frame_queue);
    }
    if (eth_handle) {
        loopback_near_end_en(eth_handle, 0, false);
    }
    delete_eth_event_group(eth_event_group);
    return ret;
}
