/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include "esp_log.h"
#include "esp_check.h"
#include "esp_attr.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_cpu.h"
#include "esp_idf_version.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
#include "esp_private/gpio.h"
#include "soc/io_mux_reg.h"
#else
#include "esp_rom_gpio.h"
#endif
#include "wiznet_mac_common.h"

esp_err_t emac_wiznet_set_mediator(esp_eth_mac_t *mac, esp_eth_mediator_t *eth)
{
    emac_wiznet_t *emac = __containerof(mac, emac_wiznet_t, parent);
    esp_err_t ret = ESP_OK;
    ESP_GOTO_ON_FALSE(eth, ESP_ERR_INVALID_ARG, err, emac->tag, "can't set mac's mediator to null");
    emac->eth = eth;
    return ESP_OK;
err:
    return ret;
}

esp_err_t emac_wiznet_get_addr(esp_eth_mac_t *mac, uint8_t *addr)
{
    emac_wiznet_t *emac = __containerof(mac, emac_wiznet_t, parent);
    esp_err_t ret = ESP_OK;
    ESP_GOTO_ON_FALSE(addr, ESP_ERR_INVALID_ARG, err, emac->tag, "invalid argument");
    memcpy(addr, emac->addr, 6);

err:
    return ret;
}

esp_err_t emac_wiznet_set_duplex(esp_eth_mac_t *mac, eth_duplex_t duplex)
{
    emac_wiznet_t *emac = __containerof(mac, emac_wiznet_t, parent);
    esp_err_t ret = ESP_OK;
    switch (duplex) {
    case ETH_DUPLEX_HALF:
        ESP_LOGD(emac->tag, "working in half duplex");
        break;
    case ETH_DUPLEX_FULL:
        ESP_LOGD(emac->tag, "working in full duplex");
        break;
    default:
        ESP_GOTO_ON_FALSE(false, ESP_ERR_INVALID_ARG, err, emac->tag, "unknown duplex");
        break;
    }

err:
    return ret;
}

esp_err_t emac_wiznet_enable_flow_ctrl(esp_eth_mac_t *mac, bool enable)
{
    /* WIZnet chips don't support flow control */
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t emac_wiznet_set_peer_pause_ability(esp_eth_mac_t *mac, uint32_t ability)
{
    /* WIZnet chips don't support PAUSE function */
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t emac_wiznet_set_link(esp_eth_mac_t *mac, eth_link_t link)
{
    emac_wiznet_t *emac = __containerof(mac, emac_wiznet_t, parent);
    esp_err_t ret = ESP_OK;
    switch (link) {
    case ETH_LINK_UP:
        ESP_LOGD(emac->tag, "link is up");
        ESP_GOTO_ON_ERROR(mac->start(mac), err, emac->tag, "start failed");
        if (emac->poll_timer) {
            ESP_GOTO_ON_ERROR(esp_timer_start_periodic(emac->poll_timer, emac->poll_period_ms * 1000),
                              err, emac->tag, "start poll timer failed");
        }
        break;
    case ETH_LINK_DOWN:
        ESP_LOGD(emac->tag, "link is down");
        ESP_GOTO_ON_ERROR(mac->stop(mac), err, emac->tag, "stop failed");
        if (emac->poll_timer) {
            ESP_GOTO_ON_ERROR(esp_timer_stop(emac->poll_timer),
                              err, emac->tag, "stop poll timer failed");
        }
        break;
    default:
        ESP_GOTO_ON_FALSE(false, ESP_ERR_INVALID_ARG, err, emac->tag, "unknown link status");
        break;
    }

err:
    return ret;
}

esp_err_t emac_wiznet_start(esp_eth_mac_t *mac)
{
    esp_err_t ret = ESP_OK;
    emac_wiznet_t *emac = __containerof(mac, emac_wiznet_t, parent);
    const wiznet_chip_ops_t *ops = emac->ops;
    /* open SOCK0 */
    ESP_GOTO_ON_ERROR(wiznet_send_command(emac, ops->cmd_open, 100), err, emac->tag, "issue OPEN command failed");
    /* enable interrupt for SOCK0 */
    uint8_t simr = ops->simr_sock0;
    ESP_GOTO_ON_ERROR(wiznet_write(emac, ops->reg_simr, &simr, sizeof(simr)), err, emac->tag, "write SIMR failed");
err:
    return ret;
}

esp_err_t emac_wiznet_stop(esp_eth_mac_t *mac)
{
    esp_err_t ret = ESP_OK;
    emac_wiznet_t *emac = __containerof(mac, emac_wiznet_t, parent);
    const wiznet_chip_ops_t *ops = emac->ops;
    /* disable interrupt */
    uint8_t simr = 0;
    ESP_GOTO_ON_ERROR(wiznet_write(emac, ops->reg_simr, &simr, sizeof(simr)), err, emac->tag, "write SIMR failed");
    /* close SOCK0 */
    ESP_GOTO_ON_ERROR(wiznet_send_command(emac, ops->cmd_close, 100), err, emac->tag, "issue CLOSE command failed");
err:
    return ret;
}

esp_err_t emac_wiznet_set_promiscuous(esp_eth_mac_t *mac, bool enable)
{
    esp_err_t ret = ESP_OK;
    emac_wiznet_t *emac = __containerof(mac, emac_wiznet_t, parent);
    const wiznet_chip_ops_t *ops = emac->ops;
    uint8_t smr = 0;
    ESP_GOTO_ON_ERROR(wiznet_read(emac, ops->regs[WIZNET_REG_SOCK_MR], &smr, sizeof(smr)), err, emac->tag, "read SMR failed");
    if (enable) {
        smr &= ~ops->smr_mac_filter;
    } else {
        smr |= ops->smr_mac_filter;
    }
    ESP_GOTO_ON_ERROR(wiznet_write(emac, ops->regs[WIZNET_REG_SOCK_MR], &smr, sizeof(smr)), err, emac->tag, "write SMR failed");
err:
    return ret;
}

esp_err_t emac_wiznet_set_addr(esp_eth_mac_t *mac, uint8_t *addr)
{
    esp_err_t ret = ESP_OK;
    emac_wiznet_t *emac = __containerof(mac, emac_wiznet_t, parent);
    ESP_GOTO_ON_FALSE(addr, ESP_ERR_INVALID_ARG, err, emac->tag, "invalid argument");
    memcpy(emac->addr, addr, 6);
    ESP_GOTO_ON_ERROR(wiznet_write(emac, emac->ops->regs[WIZNET_REG_MAC_ADDR], emac->addr, 6), err, emac->tag, "write MAC address register failed");
err:
    return ret;
}

esp_err_t emac_wiznet_set_speed(esp_eth_mac_t *mac, eth_speed_t speed)
{
    esp_err_t ret = ESP_OK;
    emac_wiznet_t *emac = __containerof(mac, emac_wiznet_t, parent);
    switch (speed) {
    case ETH_SPEED_10M:
        emac->tx_tmo = WIZNET_10M_TX_TMO_US;
        ESP_LOGD(emac->tag, "working in 10Mbps");
        break;
    case ETH_SPEED_100M:
        emac->tx_tmo = WIZNET_100M_TX_TMO_US;
        ESP_LOGD(emac->tag, "working in 100Mbps");
        break;
    default:
        ESP_GOTO_ON_FALSE(false, ESP_ERR_INVALID_ARG, err, emac->tag, "unknown speed");
        break;
    }
err:
    return ret;
}

esp_err_t emac_wiznet_write_phy_reg(esp_eth_mac_t *mac, uint32_t phy_addr, uint32_t phy_reg, uint32_t reg_value)
{
    esp_err_t ret = ESP_OK;
    emac_wiznet_t *emac = __containerof(mac, emac_wiznet_t, parent);
    // WIZnet PHY registers are 8-bit and mapped directly in the chip's register space.
    // The phy_reg parameter contains the full chip register address.
    uint8_t val = (uint8_t)reg_value;
    ESP_GOTO_ON_ERROR(wiznet_write(emac, phy_reg, &val, sizeof(val)), err, emac->tag, "write PHY register failed");
err:
    return ret;
}

esp_err_t emac_wiznet_read_phy_reg(esp_eth_mac_t *mac, uint32_t phy_addr, uint32_t phy_reg, uint32_t *reg_value)
{
    esp_err_t ret = ESP_OK;
    emac_wiznet_t *emac = __containerof(mac, emac_wiznet_t, parent);
    ESP_GOTO_ON_FALSE(reg_value, ESP_ERR_INVALID_ARG, err, emac->tag, "can't set reg_value to null");
    // WIZnet PHY registers are 8-bit and mapped directly in the chip's register
    // space. The phy_reg parameter contains the full chip register address.
    //
    // Be careful about changing the size here, as the users of this function
    // only ever expect a single byte.
    ESP_GOTO_ON_ERROR(wiznet_read(emac, phy_reg, reg_value, sizeof(uint8_t)), err, emac->tag, "read PHY register failed");
err:
    return ret;
}

esp_err_t emac_wiznet_init(esp_eth_mac_t *mac)
{
    esp_err_t ret = ESP_OK;
    emac_wiznet_t *emac = __containerof(mac, emac_wiznet_t, parent);
    esp_eth_mediator_t *eth = emac->eth;
    const wiznet_chip_ops_t *ops = emac->ops;

    ESP_GOTO_ON_ERROR(wiznet_install_gpio_isr(emac), err, emac->tag, "install GPIO ISR failed");
    ESP_GOTO_ON_ERROR(eth->on_state_changed(eth, ETH_STATE_LLINIT, NULL), err, emac->tag, "lowlevel init failed");
    /* reset chip */
    ESP_GOTO_ON_ERROR(ops->reset(emac), err, emac->tag, "reset failed");
    /* verify chip id */
    ESP_GOTO_ON_ERROR(ops->verify_id(emac), err, emac->tag, "verify chip ID failed");
    /* default setup of internal registers */
    ESP_GOTO_ON_ERROR(ops->setup_default(emac), err, emac->tag, "default setup failed");
    return ESP_OK;
err:
    if (emac->int_gpio_num >= 0) {
        gpio_isr_handler_remove(emac->int_gpio_num);
        gpio_reset_pin(emac->int_gpio_num);
    }
    eth->on_state_changed(eth, ETH_STATE_DEINIT, NULL);
    return ret;
}

esp_err_t emac_wiznet_deinit(esp_eth_mac_t *mac)
{
    emac_wiznet_t *emac = __containerof(mac, emac_wiznet_t, parent);
    esp_eth_mediator_t *eth = emac->eth;
    mac->stop(mac);
    if (emac->int_gpio_num >= 0) {
        gpio_isr_handler_remove(emac->int_gpio_num);
        gpio_reset_pin(emac->int_gpio_num);
    }
    if (emac->poll_timer && esp_timer_is_active(emac->poll_timer)) {
        esp_timer_stop(emac->poll_timer);
    }
    eth->on_state_changed(eth, ETH_STATE_DEINIT, NULL);
    return ESP_OK;
}

esp_err_t emac_wiznet_del(esp_eth_mac_t *mac)
{
    emac_wiznet_t *emac = __containerof(mac, emac_wiznet_t, parent);
    if (emac->poll_timer) {
        esp_timer_delete(emac->poll_timer);
    }
    vTaskDelete(emac->rx_task_hdl);
    emac->spi.deinit(emac->spi.ctx);
    heap_caps_free(emac->rx_buffer);
    free(emac);
    return ESP_OK;
}

static IRAM_ATTR void wiznet_isr_handler(void *arg)
{
    emac_wiznet_t *emac = (emac_wiznet_t *)arg;
    BaseType_t high_task_wakeup = pdFALSE;
    /* notify RX task */
    vTaskNotifyGiveFromISR(emac->rx_task_hdl, &high_task_wakeup);
    if (high_task_wakeup != pdFALSE) {
        portYIELD_FROM_ISR();
    }
}

static void wiznet_poll_timer_cb(void *arg)
{
    emac_wiznet_t *emac = (emac_wiznet_t *)arg;
    xTaskNotifyGive(emac->rx_task_hdl);
}

esp_err_t wiznet_install_gpio_isr(emac_wiznet_t *emac)
{
    if (emac->int_gpio_num < 0) {
        return ESP_OK;  // Polling mode, no ISR needed
    }
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
    gpio_func_sel(emac->int_gpio_num, PIN_FUNC_GPIO);
    gpio_input_enable(emac->int_gpio_num);
    gpio_pullup_en(emac->int_gpio_num);
#else
    esp_rom_gpio_pad_select_gpio(emac->int_gpio_num);
    gpio_set_direction(emac->int_gpio_num, GPIO_MODE_INPUT);
    gpio_set_pull_mode(emac->int_gpio_num, GPIO_PULLUP_ONLY);
#endif
    gpio_set_intr_type(emac->int_gpio_num, GPIO_INTR_NEGEDGE);  // active low
    gpio_intr_enable(emac->int_gpio_num);
    gpio_isr_handler_add(emac->int_gpio_num, wiznet_isr_handler, emac);
    return ESP_OK;
}

esp_err_t wiznet_create_poll_timer(emac_wiznet_t *emac)
{
    if (emac->int_gpio_num >= 0) {
        return ESP_OK;  // Interrupt mode, no poll timer needed
    }
    const esp_timer_create_args_t poll_timer_args = {
        .callback = wiznet_poll_timer_cb,
        .name = "wiznet_poll",
        .arg = emac,
        .skip_unhandled_events = true
    };
    return esp_timer_create(&poll_timer_args, &emac->poll_timer);
}

/*******************************************************************************
 * SPI Read/Write Helpers (using ops for register addresses)
 ******************************************************************************/

esp_err_t wiznet_read(emac_wiznet_t *emac, uint32_t address, void *data, uint32_t len)
{
    /* Address encoding is identical for W5500/W6100:
     * - Upper bits: address phase (16-bit offset)
     * - Lower bits: control phase (BSB + RWB + OM)
     * The ops structure stores pre-encoded register addresses.
     */
    uint32_t cmd = (address >> WIZNET_ADDR_OFFSET);
    uint32_t addr = (address & 0xFFFF);  // Already includes BSB, just add read bit
    return emac->spi.read(emac->spi.ctx, cmd, addr, data, len);
}

esp_err_t wiznet_write(emac_wiznet_t *emac, uint32_t address, const void *data, uint32_t len)
{
    uint32_t cmd = (address >> WIZNET_ADDR_OFFSET);
    uint32_t addr = (address & 0xFFFF) | (WIZNET_ACCESS_MODE_WRITE << WIZNET_RWB_OFFSET);
    return emac->spi.write(emac->spi.ctx, cmd, addr, data, len);
}

esp_err_t wiznet_send_command(emac_wiznet_t *emac, uint8_t command, uint32_t timeout_ms)
{
    esp_err_t ret = ESP_OK;
    const wiznet_chip_ops_t *ops = emac->ops;

    ESP_GOTO_ON_ERROR(wiznet_write(emac, ops->reg_sock_cr, &command, sizeof(command)), err, emac->tag, "write SCR failed");
    // after chip accepts the command, the command register will be cleared automatically
    uint32_t to = 0;
    for (to = 0; to < timeout_ms / 10; to++) {
        ESP_GOTO_ON_ERROR(wiznet_read(emac, ops->reg_sock_cr, &command, sizeof(command)), err, emac->tag, "read SCR failed");
        if (!command) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    ESP_GOTO_ON_FALSE(to < timeout_ms / 10, ESP_ERR_TIMEOUT, err, emac->tag, "send command timeout");

err:
    return ret;
}

static bool wiznet_is_link_up(emac_wiznet_t *emac)
{
    const wiznet_chip_ops_t *ops = emac->ops;
    uint8_t phy_status;
    /* Check if link is up by reading PHY status register */
    if (wiznet_read(emac, ops->reg_phy_status, &phy_status, 1) == ESP_OK
            && (phy_status & ops->phy_link_mask)) {
        return true;
    }
    return false;
}

static esp_err_t wiznet_get_tx_free_size(emac_wiznet_t *emac, uint16_t *size)
{
    esp_err_t ret = ESP_OK;
    const wiznet_chip_ops_t *ops = emac->ops;
    uint16_t free0, free1 = 0;
    // read TX_FSR register more than once, until we get the same value
    do {
        ESP_GOTO_ON_ERROR(wiznet_read(emac, ops->reg_sock_tx_fsr, &free0, sizeof(free0)), err, emac->tag, "read TX FSR failed");
        ESP_GOTO_ON_ERROR(wiznet_read(emac, ops->reg_sock_tx_fsr, &free1, sizeof(free1)), err, emac->tag, "read TX FSR failed");
    } while (free0 != free1);

    *size = __builtin_bswap16(free0);

err:
    return ret;
}

static esp_err_t wiznet_get_rx_received_size(emac_wiznet_t *emac, uint16_t *size)
{
    esp_err_t ret = ESP_OK;
    const wiznet_chip_ops_t *ops = emac->ops;
    uint16_t received0, received1 = 0;
    do {
        ESP_GOTO_ON_ERROR(wiznet_read(emac, ops->reg_sock_rx_rsr, &received0, sizeof(received0)), err, emac->tag, "read RX RSR failed");
        ESP_GOTO_ON_ERROR(wiznet_read(emac, ops->reg_sock_rx_rsr, &received1, sizeof(received1)), err, emac->tag, "read RX RSR failed");
    } while (received0 != received1);
    *size = __builtin_bswap16(received0);

err:
    return ret;
}

static esp_err_t wiznet_write_buffer(emac_wiznet_t *emac, const void *buffer, uint32_t len, uint16_t offset)
{
    esp_err_t ret = ESP_OK;
    uint32_t addr = emac->ops->mem_sock_tx_base | (offset << 16);
    ESP_GOTO_ON_ERROR(wiznet_write(emac, addr, buffer, len), err, emac->tag, "write TX buffer failed");
err:
    return ret;
}

static esp_err_t wiznet_read_buffer(emac_wiznet_t *emac, void *buffer, uint32_t len, uint16_t offset)
{
    esp_err_t ret = ESP_OK;
    uint32_t addr = emac->ops->mem_sock_rx_base | (offset << 16);
    ESP_GOTO_ON_ERROR(wiznet_read(emac, addr, buffer, len), err, emac->tag, "read RX buffer failed");
err:
    return ret;
}

/*******************************************************************************
 * Common Transmit/Receive Implementation
 ******************************************************************************/

esp_err_t emac_wiznet_transmit(esp_eth_mac_t *mac, uint8_t *buf, uint32_t length)
{
    esp_err_t ret = ESP_OK;
    emac_wiznet_t *emac = __containerof(mac, emac_wiznet_t, parent);
    const wiznet_chip_ops_t *ops = emac->ops;
    uint16_t offset = 0;

    ESP_GOTO_ON_FALSE(length <= ETH_MAX_PACKET_SIZE, ESP_ERR_INVALID_ARG, err,
                      emac->tag, "frame size is too big (actual %" PRIu32 ", maximum %u)", length, ETH_MAX_PACKET_SIZE);
    // check if there's free memory to store this packet
    uint16_t free_size = 0;
    ESP_GOTO_ON_ERROR(wiznet_get_tx_free_size(emac, &free_size), err, emac->tag, "get free size failed");
    ESP_GOTO_ON_FALSE(length <= free_size, ESP_ERR_NO_MEM, err, emac->tag, "free size (%" PRIu16 ") < send length (%" PRIu32 ")", free_size, length);
    // get current write pointer
    ESP_GOTO_ON_ERROR(wiznet_read(emac, ops->reg_sock_tx_wr, &offset, sizeof(offset)), err, emac->tag, "read TX WR failed");
    offset = __builtin_bswap16(offset);
    // copy data to tx memory
    ESP_GOTO_ON_ERROR(wiznet_write_buffer(emac, buf, length, offset), err, emac->tag, "write frame failed");
    // update write pointer
    offset += length;
    offset = __builtin_bswap16(offset);
    ESP_GOTO_ON_ERROR(wiznet_write(emac, ops->reg_sock_tx_wr, &offset, sizeof(offset)), err, emac->tag, "write TX WR failed");
    // issue SEND command
    ESP_GOTO_ON_ERROR(wiznet_send_command(emac, ops->cmd_send, 100), err, emac->tag, "issue SEND command failed");

    // polling the TX done event
    uint8_t status = 0;
    uint64_t start = esp_timer_get_time();
    uint64_t now = 0;
    do {
        now = esp_timer_get_time();
        if (!wiznet_is_link_up(emac) || (now - start) > emac->tx_tmo) {
            return ESP_FAIL;
        }
        ESP_GOTO_ON_ERROR(wiznet_read(emac, ops->reg_sock_ir, &status, sizeof(status)), err, emac->tag, "read SOCK0 IR failed");
    } while (!(status & ops->sir_send));
    // clear the event bit
    status = ops->sir_send;
    ESP_GOTO_ON_ERROR(wiznet_write(emac, ops->reg_sock_irclr, &status, sizeof(status)), err, emac->tag, "write SOCK0 IRCLR failed");

err:
    return ret;
}

typedef struct {
    uint32_t offset;
    uint32_t copy_len;
    uint32_t rx_len;
    uint32_t remain;
} __attribute__((packed)) emac_wiznet_auto_buf_info_t;

#define WIZNET_ETH_MAC_RX_BUF_SIZE_AUTO (0)

static esp_err_t emac_wiznet_alloc_recv_buf(emac_wiznet_t *emac, uint8_t **buf, uint32_t *length)
{
    esp_err_t ret = ESP_OK;
    const wiznet_chip_ops_t *ops = emac->ops;
    uint16_t offset = 0;
    uint16_t rx_len = 0;
    uint32_t copy_len = 0;
    uint16_t remain_bytes = 0;
    *buf = NULL;

    wiznet_get_rx_received_size(emac, &remain_bytes);
    if (remain_bytes) {
        // get current read pointer
        ESP_GOTO_ON_ERROR(wiznet_read(emac, ops->reg_sock_rx_rd, &offset, sizeof(offset)), err, emac->tag, "read RX RD failed");
        offset = __builtin_bswap16(offset);
        // read head
        ESP_GOTO_ON_ERROR(wiznet_read_buffer(emac, &rx_len, sizeof(rx_len), offset), err, emac->tag, "read frame header failed");
        rx_len = __builtin_bswap16(rx_len) - 2; // data size includes 2 bytes of header
        // frames larger than expected will be truncated
        copy_len = rx_len > *length ? *length : rx_len;
        // runt frames are not forwarded, but check the length anyway since it could be corrupted at SPI bus
        ESP_GOTO_ON_FALSE(copy_len >= ETH_MIN_PACKET_SIZE - ETH_CRC_LEN, ESP_ERR_INVALID_SIZE, err, emac->tag, "invalid frame length %" PRIu32, copy_len);
        *buf = malloc(copy_len);
        if (*buf != NULL) {
            emac_wiznet_auto_buf_info_t *buff_info = (emac_wiznet_auto_buf_info_t *)*buf;
            buff_info->offset = offset;
            buff_info->copy_len = copy_len;
            buff_info->rx_len = rx_len;
            buff_info->remain = remain_bytes;
        } else {
            ret = ESP_ERR_NO_MEM;
            goto err;
        }
    }
err:
    *length = rx_len;
    return ret;
}

esp_err_t emac_wiznet_receive(esp_eth_mac_t *mac, uint8_t *buf, uint32_t *length)
{
    esp_err_t ret = ESP_OK;
    emac_wiznet_t *emac = __containerof(mac, emac_wiznet_t, parent);
    const wiznet_chip_ops_t *ops = emac->ops;
    uint16_t offset = 0;
    uint16_t rx_len = 0;
    uint16_t copy_len = 0;
    uint16_t remain_bytes = 0;
    emac->packets_remain = false;

    if (*length != WIZNET_ETH_MAC_RX_BUF_SIZE_AUTO) {
        wiznet_get_rx_received_size(emac, &remain_bytes);
        if (remain_bytes) {
            // get current read pointer
            ESP_GOTO_ON_ERROR(wiznet_read(emac, ops->reg_sock_rx_rd, &offset, sizeof(offset)), err, emac->tag, "read RX RD failed");
            offset = __builtin_bswap16(offset);
            // read head first
            ESP_GOTO_ON_ERROR(wiznet_read_buffer(emac, &rx_len, sizeof(rx_len), offset), err, emac->tag, "read frame header failed");
            rx_len = __builtin_bswap16(rx_len) - 2; // data size includes 2 bytes of header
            // frames larger than expected will be truncated
            copy_len = rx_len > *length ? *length : rx_len;
        } else {
            // silently return when no frame is waiting
            goto err;
        }
    } else {
        emac_wiznet_auto_buf_info_t *buff_info = (emac_wiznet_auto_buf_info_t *)buf;
        offset = buff_info->offset;
        copy_len = buff_info->copy_len;
        rx_len = buff_info->rx_len;
        remain_bytes = buff_info->remain;
    }
    // 2 bytes of header
    offset += 2;
    // read the payload
    ESP_GOTO_ON_ERROR(wiznet_read_buffer(emac, emac->rx_buffer, copy_len, offset), err, emac->tag, "read payload failed, len=%" PRIu16 ", offset=%" PRIu16, rx_len, offset);
    memcpy(buf, emac->rx_buffer, copy_len);
    offset += rx_len;
    // update read pointer
    offset = __builtin_bswap16(offset);
    ESP_GOTO_ON_ERROR(wiznet_write(emac, ops->reg_sock_rx_rd, &offset, sizeof(offset)), err, emac->tag, "write RX RD failed");
    /* issue RECV command */
    ESP_GOTO_ON_ERROR(wiznet_send_command(emac, ops->cmd_recv, 100), err, emac->tag, "issue RECV command failed");
    // check if there're more data need to process
    remain_bytes -= rx_len + 2;
    emac->packets_remain = remain_bytes > 0;

    *length = copy_len;
    return ret;
err:
    *length = 0;
    return ret;
}

static esp_err_t emac_wiznet_flush_recv_frame(emac_wiznet_t *emac)
{
    esp_err_t ret = ESP_OK;
    const wiznet_chip_ops_t *ops = emac->ops;
    uint16_t offset = 0;
    uint16_t rx_len = 0;
    uint16_t remain_bytes = 0;
    emac->packets_remain = false;

    wiznet_get_rx_received_size(emac, &remain_bytes);
    if (remain_bytes) {
        // get current read pointer
        ESP_GOTO_ON_ERROR(wiznet_read(emac, ops->reg_sock_rx_rd, &offset, sizeof(offset)), err, emac->tag, "read RX RD failed");
        offset = __builtin_bswap16(offset);
        // read head first
        ESP_GOTO_ON_ERROR(wiznet_read_buffer(emac, &rx_len, sizeof(rx_len), offset), err, emac->tag, "read frame header failed");
        // update read pointer
        rx_len = __builtin_bswap16(rx_len);
        offset += rx_len;
        offset = __builtin_bswap16(offset);
        ESP_GOTO_ON_ERROR(wiznet_write(emac, ops->reg_sock_rx_rd, &offset, sizeof(offset)), err, emac->tag, "write RX RD failed");
        /* issue RECV command */
        ESP_GOTO_ON_ERROR(wiznet_send_command(emac, ops->cmd_recv, 100), err, emac->tag, "issue RECV command failed");
        // check if there're more data need to process
        remain_bytes -= rx_len;
        emac->packets_remain = remain_bytes > 0;
    }
err:
    return ret;
}

void emac_wiznet_task(void *arg)
{
    emac_wiznet_t *emac = (emac_wiznet_t *)arg;
    const wiznet_chip_ops_t *ops = emac->ops;
    uint8_t status = 0;
    uint8_t *buffer = NULL;
    uint32_t frame_len = 0;
    uint32_t buf_len = 0;
    esp_err_t ret;
    while (1) {
        /* check if the task receives any notification */
        if (emac->int_gpio_num >= 0) {                                    // if in interrupt mode
            if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000)) == 0 &&     // if no notification ...
                    gpio_get_level(emac->int_gpio_num) != 0) {            // ...and no interrupt asserted
                continue;                                                 // -> just continue to check again
            }
        } else {
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        }
        /* read interrupt status */
        wiznet_read(emac, ops->reg_sock_ir, &status, sizeof(status));
        /* packet received */
        if (status & ops->sir_recv) {
            /* clear interrupt status */
            uint8_t clr = ops->sir_recv;
            wiznet_write(emac, ops->reg_sock_irclr, &clr, sizeof(clr));
            do {
                /* define max expected frame len */
                frame_len = ETH_MAX_PACKET_SIZE;
                if ((ret = emac_wiznet_alloc_recv_buf(emac, &buffer, &frame_len)) == ESP_OK) {
                    if (buffer != NULL) {
                        /* we have memory to receive the frame of maximal size previously defined */
                        buf_len = WIZNET_ETH_MAC_RX_BUF_SIZE_AUTO;
                        if (emac->parent.receive(&emac->parent, buffer, &buf_len) == ESP_OK) {
                            if (buf_len == 0) {
                                free(buffer);
                            } else if (frame_len > buf_len) {
                                ESP_LOGE(emac->tag, "received frame was truncated");
                                free(buffer);
                            } else {
                                ESP_LOGD(emac->tag, "receive len=%" PRIu32, buf_len);
                                /* pass the buffer to stack (e.g. TCP/IP layer) */
                                emac->eth->stack_input(emac->eth, buffer, buf_len);
                            }
                        } else {
                            ESP_LOGE(emac->tag, "frame read from module failed");
                            free(buffer);
                        }
                    } else if (frame_len) {
                        ESP_LOGE(emac->tag, "invalid combination of frame_len(%" PRIu32 ") and buffer pointer(%p)", frame_len, buffer);
                    }
                } else if (ret == ESP_ERR_NO_MEM) {
                    ESP_LOGE(emac->tag, "no mem for receive buffer");
                    emac_wiznet_flush_recv_frame(emac);
                } else {
                    ESP_LOGE(emac->tag, "unexpected error 0x%x", ret);
                }
            } while (emac->packets_remain);
        }
    }
    vTaskDelete(NULL);
}

/*******************************************************************************
 * Common Setup Helper
 ******************************************************************************/

esp_err_t wiznet_setup_default(emac_wiznet_t *emac)
{
    esp_err_t ret = ESP_OK;
    const wiznet_chip_ops_t *ops = emac->ops;
    uint8_t reg_value = 16;

    /* Only SOCK0 can be used as MAC RAW mode, so we give the whole buffer
     * (16KB TX and 16KB RX) to SOCK0, which doesn't have any effect for TX though.
     * A larger TX buffer doesn't buy us pipelining - each SEND is one frame and
     * must complete before the next. */
    ESP_GOTO_ON_ERROR(wiznet_write(emac, ops->regs[WIZNET_REG_SOCK_RXBUF_SIZE], &reg_value, sizeof(reg_value)),
                      err, emac->tag, "set rx buffer size failed");
    ESP_GOTO_ON_ERROR(wiznet_write(emac, ops->regs[WIZNET_REG_SOCK_TXBUF_SIZE], &reg_value, sizeof(reg_value)),
                      err, emac->tag, "set tx buffer size failed");
    reg_value = 0;
    /* Sockets 1-7 get zero buffer since we only use SOCK0 for MACRAW.
     * Per datasheet: socket n uses BSB = (n * 4) + 1, so offset from socket 0
     * is a BSB delta of (n * 4). */
    for (int i = 1; i < 8; i++) {
        uint32_t bsb_delta = WIZNET_BSB_SOCK_REG(i) - WIZNET_BSB_SOCK_REG(0);
        uint32_t rx_reg = ops->regs[WIZNET_REG_SOCK_RXBUF_SIZE] + WIZNET_MAKE_MAP(0, bsb_delta);
        uint32_t tx_reg = ops->regs[WIZNET_REG_SOCK_TXBUF_SIZE] + WIZNET_MAKE_MAP(0, bsb_delta);
        ESP_GOTO_ON_ERROR(wiznet_write(emac, rx_reg, &reg_value, sizeof(reg_value)),
                          err, emac->tag, "set rx buffer size failed");
        ESP_GOTO_ON_ERROR(wiznet_write(emac, tx_reg, &reg_value, sizeof(reg_value)),
                          err, emac->tag, "set tx buffer size failed");
    }

    /* Disable interrupt for all sockets by default */
    reg_value = 0;
    ESP_GOTO_ON_ERROR(wiznet_write(emac, ops->reg_simr, &reg_value, sizeof(reg_value)),
                      err, emac->tag, "write SIMR failed");

    /* Enable MAC RAW mode for SOCK0 with MAC filter and multicast blocking.
     * Note: This configures MACRAW mode, which bypasses the chip's internal
     * network stack entirely. All frames are delivered to/from the host.
     * Settings like ping block (MR_PB) have no effect in MACRAW mode. */
    reg_value = ops->smr_default;
    ESP_GOTO_ON_ERROR(wiznet_write(emac, ops->regs[WIZNET_REG_SOCK_MR], &reg_value, sizeof(reg_value)),
                      err, emac->tag, "write SMR failed");

    /* Enable receive event for SOCK0 */
    reg_value = ops->sir_recv;
    ESP_GOTO_ON_ERROR(wiznet_write(emac, ops->regs[WIZNET_REG_SOCK_IMR], &reg_value, sizeof(reg_value)),
                      err, emac->tag, "write SOCK0 IMR failed");

    /* Set the interrupt re-assert level to maximum (~1.5ms) to lower the
     * chances of missing it */
    uint16_t int_level = __builtin_bswap16(0xFFFF);
    ESP_GOTO_ON_ERROR(wiznet_write(emac, ops->regs[WIZNET_REG_INT_LEVEL], &int_level, sizeof(int_level)),
                      err, emac->tag, "write INT level failed");

err:
    return ret;
}

/*******************************************************************************
 * Common Constructor Helper
 ******************************************************************************/

void emac_wiznet_cleanup_common(emac_wiznet_t *emac)
{
    if (emac->poll_timer) {
        esp_timer_delete(emac->poll_timer);
    }
    if (emac->rx_task_hdl) {
        vTaskDelete(emac->rx_task_hdl);
    }
    if (emac->spi.ctx) {
        emac->spi.deinit(emac->spi.ctx);
    }
    heap_caps_free(emac->rx_buffer);
}

esp_err_t emac_wiznet_init_common(emac_wiznet_t *emac,
                                  const eth_wiznet_config_t *wiznet_config,
                                  const eth_mac_config_t *mac_config,
                                  const wiznet_chip_ops_t *ops,
                                  const char *tag,
                                  const char *task_name)
{
    esp_err_t ret = ESP_OK;

    /* Validate chip-specific ops */
    ESP_GOTO_ON_FALSE(ops && ops->reset && ops->verify_id && ops->setup_default,
                      ESP_ERR_INVALID_ARG, err, tag, "chip-specific ops not configured");

    /* bind methods and attributes */
    emac->tag = tag;
    emac->ops = ops;
    emac->sw_reset_timeout_ms = mac_config->sw_reset_timeout_ms;
    emac->tx_tmo = WIZNET_100M_TX_TMO_US;  // default to 100Mbps timeout
    emac->int_gpio_num = wiznet_config->int_gpio_num;
    emac->poll_period_ms = wiznet_config->poll_period_ms;
    emac->parent.set_mediator = emac_wiznet_set_mediator;
    emac->parent.init = emac_wiznet_init;
    emac->parent.deinit = emac_wiznet_deinit;
    emac->parent.start = emac_wiznet_start;
    emac->parent.stop = emac_wiznet_stop;
    emac->parent.del = emac_wiznet_del;
    emac->parent.set_addr = emac_wiznet_set_addr;
    emac->parent.get_addr = emac_wiznet_get_addr;
    emac->parent.set_speed = emac_wiznet_set_speed;
    emac->parent.set_duplex = emac_wiznet_set_duplex;
    emac->parent.set_link = emac_wiznet_set_link;
    emac->parent.set_promiscuous = emac_wiznet_set_promiscuous;
    emac->parent.set_peer_pause_ability = emac_wiznet_set_peer_pause_ability;
    emac->parent.enable_flow_ctrl = emac_wiznet_enable_flow_ctrl;
    emac->parent.write_phy_reg = emac_wiznet_write_phy_reg;
    emac->parent.read_phy_reg = emac_wiznet_read_phy_reg;
    emac->parent.transmit = emac_wiznet_transmit;
    emac->parent.receive = emac_wiznet_receive;

    /* setup SPI driver */
    if (wiznet_config->custom_spi_driver.init != NULL && wiznet_config->custom_spi_driver.deinit != NULL
            && wiznet_config->custom_spi_driver.read != NULL && wiznet_config->custom_spi_driver.write != NULL) {
        ESP_LOGD(tag, "Using user's custom SPI Driver");
        emac->spi.init = wiznet_config->custom_spi_driver.init;
        emac->spi.deinit = wiznet_config->custom_spi_driver.deinit;
        emac->spi.read = wiznet_config->custom_spi_driver.read;
        emac->spi.write = wiznet_config->custom_spi_driver.write;
        /* Custom SPI driver device init */
        ESP_GOTO_ON_FALSE((emac->spi.ctx = emac->spi.init(wiznet_config->custom_spi_driver.config)) != NULL,
                          ESP_FAIL, err, tag, "SPI initialization failed");
    } else {
        ESP_LOGD(tag, "Using default SPI Driver");
        emac->spi.init = wiznet_spi_init;
        emac->spi.deinit = wiznet_spi_deinit;
        emac->spi.read = wiznet_spi_read;
        emac->spi.write = wiznet_spi_write;
        /* SPI device init */
        ESP_GOTO_ON_FALSE((emac->spi.ctx = emac->spi.init(wiznet_config)) != NULL,
                          ESP_FAIL, err, tag, "SPI initialization failed");
    }

    /* create rx task */
    BaseType_t core_num = tskNO_AFFINITY;
    if (mac_config->flags & ETH_MAC_FLAG_PIN_TO_CORE) {
        core_num = esp_cpu_get_core_id();
    }
    BaseType_t xReturned = xTaskCreatePinnedToCore(emac_wiznet_task, task_name, mac_config->rx_task_stack_size, emac,
                                                   mac_config->rx_task_prio, &emac->rx_task_hdl, core_num);
    ESP_GOTO_ON_FALSE(xReturned == pdPASS, ESP_FAIL, err, tag, "create rx task failed");

    /* allocate RX buffer */
    emac->rx_buffer = heap_caps_malloc(ETH_MAX_PACKET_SIZE, MALLOC_CAP_DMA);
    ESP_GOTO_ON_FALSE(emac->rx_buffer, ESP_ERR_NO_MEM, err, tag, "RX buffer allocation failed");

    /* create poll timer if needed */
    ESP_GOTO_ON_ERROR(wiznet_create_poll_timer(emac), err, tag, "create poll timer failed");

    return ESP_OK;

err:
    emac_wiznet_cleanup_common(emac);
    return ret;
}
