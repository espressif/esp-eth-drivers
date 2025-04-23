/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdlib.h>
#include <sys/cdefs.h>
#include <sys/queue.h>
#include "esp_log.h"
#include "esp_check.h"
#include "driver/gpio.h"
#include "esp_rom_gpio.h"

#include "esp_eth_ksz8863.h"
#include "ksz8863.h" // registers

static const char *TAG = "ksz8863_eth";

struct ksz8863_port_tbl_s {
    esp_eth_handle_t eth_handle;
    int32_t port_num;
    SLIST_ENTRY(ksz8863_port_tbl_s) next;
};

static SLIST_HEAD(slisthead, ksz8863_port_tbl_s) s_port_tbls_head;
static esp_eth_handle_t s_host_eth_hndl = NULL;

/* ----------------- Functions to control receive/transmit flow ------------------ */

esp_err_t ksz8863_register_tail_tag_port(esp_eth_handle_t port_eth_handle, int32_t port_num)
{
    esp_err_t ret = ESP_OK;
    struct ksz8863_port_tbl_s *item = malloc(sizeof(struct ksz8863_port_tbl_s));
    ESP_GOTO_ON_FALSE(item, ESP_ERR_NO_MEM, err, TAG, "no memory for Tail Tag port info");
    item->eth_handle = port_eth_handle;
    item->port_num = port_num;
    SLIST_INSERT_HEAD(&s_port_tbls_head, item, next);

err:
    return ret;
}

esp_err_t ksz8863_unregister_tail_tag_port(esp_eth_handle_t port_eth_handle)
{
    struct ksz8863_port_tbl_s *item;
    while (!SLIST_EMPTY(&s_port_tbls_head)) {
        item = SLIST_FIRST(&s_port_tbls_head);
        SLIST_REMOVE_HEAD(&s_port_tbls_head, next);
        free(item);
    }
    return ESP_OK;
}

esp_err_t ksz8863_eth_tail_tag_port_forward(esp_eth_handle_t eth_handle, uint8_t *buffer, uint32_t length, void *priv)
{
    struct ksz8863_port_tbl_s *item;
    SLIST_FOREACH(item, &s_port_tbls_head, next) {
        if (item->port_num == buffer[length - 1]) {
            esp_eth_mediator_t *eth = item->eth_handle;
            eth->stack_input(eth, buffer, length - 1);
            return ESP_OK;
        }
    }

    free(buffer);
    return ESP_OK;
}

esp_err_t ksz8863_register_host_eth_hndl(esp_eth_handle_t host_eth_handle)
{
    esp_err_t ret = ESP_OK;

    ESP_GOTO_ON_FALSE(s_host_eth_hndl == NULL, ESP_ERR_INVALID_STATE, err, TAG, "host Ethernet handle has been already registered");
    s_host_eth_hndl = host_eth_handle;
err:
    return ret;
}

// TODO: is some other clean-up needed?
esp_err_t ksz8863_unregister_host_eth_hndl(void)
{
    esp_err_t ret = ESP_OK;
    s_host_eth_hndl = NULL;
    return ret;
}

static esp_err_t ksz8863_eth_transmit_tag(esp_eth_handle_t host_eth_handle, void *buf, size_t length, uint8_t tail_tag)
{
    esp_err_t ret = ESP_OK;
    // Add padding bytes for frames shorter than 60 bytes since Tail tag needs to be placed at the end of the frame
    if (length < ETH_HEADER_LEN + ETH_MIN_PAYLOAD_LEN) {
        uint32_t tail_len = ETH_HEADER_LEN + ETH_MIN_PAYLOAD_LEN - length + 1;
        uint8_t *buff_tail = calloc(1, tail_len);
        ESP_GOTO_ON_FALSE(buff_tail, ESP_ERR_NO_MEM, err, TAG, "no memory");
        buff_tail[tail_len - 1] = tail_tag;
        ret = esp_eth_transmit_vargs(host_eth_handle, 2, buf, length, buff_tail, tail_len);
        free(buff_tail);
    } else {
        ret = esp_eth_transmit_vargs(host_eth_handle, 2, buf, length, &tail_tag, 1);
    }
err:
    return ret;
}

// Transmits with tail tag 0 (normal address lookup)
esp_err_t ksz8863_eth_transmit_normal_lookup(esp_eth_handle_t host_eth_handle, void *buf, size_t length)
{
    esp_err_t ret = ESP_OK;

    ESP_GOTO_ON_FALSE(buf, ESP_ERR_INVALID_ARG, err, TAG, "can't set buf to null");
    ESP_GOTO_ON_FALSE(length, ESP_ERR_INVALID_ARG, err, TAG, "buf length can't be zero");
    ESP_GOTO_ON_FALSE(host_eth_handle, ESP_ERR_INVALID_ARG, err, TAG, "ethernet driver handle can't be null");

    ret = ksz8863_eth_transmit_tag(host_eth_handle, buf, length, 0);
err:
    return ret;
}

// this is abstraction function to not pollute KSZ8863 MAC layer with Host Ethernet layer via which is needed to perform transmit
esp_err_t ksz8863_eth_transmit_via_host(void *buf, size_t length, uint8_t tail_tag)
{
    esp_err_t ret = ESP_OK;
    ESP_GOTO_ON_FALSE(s_host_eth_hndl, ESP_ERR_INVALID_STATE, err, TAG, "host Ethernet handle was not registered");
    ret = ksz8863_eth_transmit_tag(s_host_eth_hndl, buf, length, tail_tag);
err:
    return ret;
}

/* ----------------- Functions to control KSZ8863 global behavior ------------------ */

esp_err_t ksz8863_sw_reset(esp_eth_handle_t port_eth_handle)
{
    esp_err_t ret = ESP_OK;
    esp_eth_mediator_t *eth = (esp_eth_mediator_t *)port_eth_handle;

    ESP_LOGW(TAG, "SW reset resets all Global, MAC and PHY registers!");
    ksz8863_reset_reg_t reset;
    reset.sw_reset = 1;
    reset.pcs_reset = 1; // Physical coding sublayer

    ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, 0, KSZ8863_RESET_ADDR, reset.val), err, TAG, "write reset failed");
    //TODO: same timeout and check the reset was performed??
    return ESP_OK;
err:
    return ret;
}

esp_err_t ksz8863_hw_reset(int reset_gpio_num)
{
    if (reset_gpio_num >= 0) {
        esp_rom_gpio_pad_select_gpio(reset_gpio_num);
        gpio_set_direction(reset_gpio_num, GPIO_MODE_OUTPUT);
        gpio_set_level(reset_gpio_num, 0);
        esp_rom_delay_us(150); // insert little more than min input assert time
        gpio_set_level(reset_gpio_num, 1);
    }
    return ESP_OK;
}

esp_err_t ksz8863_p3_rmii_internal_clk(esp_eth_handle_t port_eth_handle, bool rmii_internal_clk)
{
    esp_err_t ret = ESP_OK;
    esp_eth_mediator_t *eth = (esp_eth_mediator_t *)port_eth_handle;

    // Set P3 RMII Clock as Internal
    ksz8863_fwdfrmhostm_reg_t mode;
    ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, 0, KSZ8863_FWDFRM_HOSTM_ADDR, &(mode.val)), err, TAG, "read FWDFRM_HOSTM failed");
    mode.p3_rmii_clk = rmii_internal_clk;
    ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, 0, KSZ8863_FWDFRM_HOSTM_ADDR, mode.val), err, TAG, "write FWDFRM_HOSTM failed");

err:
    return ret;
}

esp_err_t ksz8863_p3_rmii_clk_invert(esp_eth_handle_t port_eth_handle, bool rmii_clk_invert)
{
    esp_err_t ret = ESP_OK;
    esp_eth_mediator_t *eth = (esp_eth_mediator_t *)port_eth_handle;

    ksz8863_idrlq0_reg_t idrlq0;
    ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, 0, KSZ8863_P3IDRLQ0_ADDR, &(idrlq0.val)), err, TAG, "read P3IDRLQ0 failed");
    idrlq0.rmii_reflck_invert = rmii_clk_invert;
    ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, 0, KSZ8863_P3IDRLQ0_ADDR, idrlq0.val), err, TAG, "write P3IDRLQ0 failed");

err:
    return ret;
}
