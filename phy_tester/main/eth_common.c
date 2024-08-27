/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_rom_sys.h"
#include "esp_event.h"
#include "esp_check.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "eth_common.h"

static const char *TAG = "ethernet_fncs";

/** Event handler for Ethernet events */
static void eth_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
    EventGroupHandle_t eth_event_group = (EventGroupHandle_t)arg;
    switch (event_id) {
    case ETHERNET_EVENT_CONNECTED:
        xEventGroupSetBits(eth_event_group, ETH_CONNECT_BIT);
        break;
    case ETHERNET_EVENT_DISCONNECTED:
        break;
    case ETHERNET_EVENT_START:
        xEventGroupSetBits(eth_event_group, ETH_START_BIT);
        break;
    case ETHERNET_EVENT_STOP:
        xEventGroupSetBits(eth_event_group, ETH_STOP_BIT);
        break;
    default:
        break;
    }
}

/** Event handler for IP_EVENT_ETH_GOT_IP */
void got_ip_event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    EventGroupHandle_t eth_event_group = (EventGroupHandle_t)arg;
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    const esp_netif_ip_info_t *ip_info = &event->ip_info;
    ESP_LOGI(TAG, "Ethernet Got IP Address");
    ESP_LOGI(TAG, "~~~~~~~~~~~");
    ESP_LOGI(TAG, "ETHIP:" IPSTR, IP2STR(&ip_info->ip));
    ESP_LOGI(TAG, "ETHMASK:" IPSTR, IP2STR(&ip_info->netmask));
    ESP_LOGI(TAG, "ETHGW:" IPSTR, IP2STR(&ip_info->gw));
    ESP_LOGI(TAG, "~~~~~~~~~~~");
    xEventGroupSetBits(eth_event_group, ETH_GOT_IP_BIT);
}

void delete_eth_event_group(EventGroupHandle_t eth_event_group)
{
    esp_event_handler_unregister(ETH_EVENT, ESP_EVENT_ANY_ID, eth_event_handler);
    if (eth_event_group != NULL) {
        vEventGroupDelete(eth_event_group);
    }
}

EventGroupHandle_t create_eth_event_group(void)
{
    EventGroupHandle_t eth_event_group = xEventGroupCreate();
    if (eth_event_group == NULL) {
        return NULL;
    }
    if (esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, eth_event_group) != ESP_OK) {
        ESP_LOGE(TAG, "event handler registration failed");
        goto err;
    }

    return eth_event_group;
err:
    delete_eth_event_group(eth_event_group);
    return NULL;
}

esp_err_t dump_phy_regs(esp_eth_handle_t *eth_handle, uint32_t start_addr, uint32_t end_addr)
{
    esp_eth_phy_reg_rw_data_t reg;
    uint32_t reg_val;
    reg.reg_value_p = &reg_val;

    printf("--- PHY Registers Dump ---\n");
    for (uint32_t curr_addr = start_addr; curr_addr <= end_addr; curr_addr++) {
        reg.reg_addr = curr_addr;
        ESP_RETURN_ON_ERROR(esp_eth_ioctl(eth_handle, ETH_CMD_READ_PHY_REG, &reg), TAG, "ioctl read PHY register failed");
        printf("Addr: 0x%02lx, value: 0x%04lx\n", curr_addr, reg_val);
    }
    printf("\n");

    return ESP_OK;
}

esp_err_t write_phy_reg(esp_eth_handle_t *eth_handle, uint32_t addr, uint32_t data)
{
    esp_eth_phy_reg_rw_data_t reg;
    uint32_t reg_val;
    reg.reg_value_p = &reg_val;

    reg.reg_addr = addr;
    reg_val = data;
    ESP_RETURN_ON_ERROR(esp_eth_ioctl(eth_handle, ETH_CMD_WRITE_PHY_REG, &reg), TAG, "ioctl write PHY register data failed");
    return ESP_OK;
}

esp_err_t loopback_near_end_en(esp_eth_handle_t *eth_handle, phy_id_t phy_id, bool enable)
{
    esp_err_t ret = ESP_OK;
    static bool nego_en = true;

    bool loopback_en;
    if (enable) {
        loopback_en = true;
        if ((ret = esp_eth_ioctl(eth_handle, ETH_CMD_S_PHY_LOOPBACK, &loopback_en)) != ESP_OK) {
            ESP_GOTO_ON_ERROR(esp_eth_ioctl(eth_handle, ETH_CMD_G_AUTONEGO, &nego_en), err, TAG, "get auto-negotiation failed");
            if (nego_en == false) {
                return ret;
            }
            ESP_LOGW(TAG, "some PHY requires to disable auto-negotiation => disabling and trying to enable loopback again...");
            nego_en = false;
            ESP_GOTO_ON_ERROR(esp_eth_ioctl(eth_handle, ETH_CMD_S_AUTONEGO, &nego_en), err, TAG, "auto-negotiation configuration failed");
            eth_speed_t speed = ETH_SPEED_100M;
            ESP_GOTO_ON_ERROR(esp_eth_ioctl(eth_handle, ETH_CMD_S_SPEED, &speed), err, TAG, "speed configuration failed");
            eth_duplex_t duplex = ETH_DUPLEX_FULL;
            ESP_GOTO_ON_ERROR(esp_eth_ioctl(eth_handle, ETH_CMD_S_DUPLEX_MODE, &duplex), err, TAG, "speed configuration failed");
            ESP_GOTO_ON_ERROR(esp_eth_ioctl(eth_handle, ETH_CMD_S_PHY_LOOPBACK, &loopback_en), err, TAG, "loopback configuration failed");
            ESP_LOGW(TAG, "loopback configuration succeeded at the second attempt, you can ignore above errors");
        }
    } else {
        // configure the PHY back to default setting
        loopback_en = false;
        ESP_GOTO_ON_ERROR(esp_eth_ioctl(eth_handle, ETH_CMD_S_PHY_LOOPBACK, &loopback_en), err, TAG, "loopback configuration failed");
        if (nego_en == false) {
            nego_en = true;
            ESP_GOTO_ON_ERROR(esp_eth_ioctl(eth_handle, ETH_CMD_S_AUTONEGO, &nego_en), err, TAG, "auto-negotiation configuration failed");
        }
    }
    return ESP_OK;
err:
    return ret;
}

esp_err_t loopback_far_end_en(esp_eth_handle_t *eth_handle, phy_id_t phy_id, bool enable)
{
    esp_err_t ret = ESP_OK;
    esp_eth_phy_reg_rw_data_t reg;
    uint32_t reg_val;
    uint32_t reg_val_exp;
    reg.reg_value_p = &reg_val;

    switch (phy_id) {
    case PHY_IP101:
        reg.reg_addr = 20; // page control reg.
        reg_val = 1; // page 1
        esp_eth_ioctl(eth_handle, ETH_CMD_WRITE_PHY_REG, &reg);
        reg.reg_addr = 23; // UTP PHY Specific Control Register
        esp_eth_ioctl(eth_handle, ETH_CMD_READ_PHY_REG, &reg);
        if (enable) {
            reg_val |= 1 << 13;
        } else {
            reg_val &= ~(1 << 13);
        }
        break;
    case PHY_LAN87XX:
        // TODO add check that 100BASE-TX, see datasheet
        reg.reg_addr = 17; // Mode Control Register
        esp_eth_ioctl(eth_handle, ETH_CMD_READ_PHY_REG, &reg);
        if (enable) {
            reg_val |= 1 << 9;
        } else {
            reg_val &= ~(1 << 9);
        }
        break;
    case PHY_KSZ80XX:
        reg.reg_addr = 0x1e; // PHY Control 1 Register
        esp_eth_ioctl(eth_handle, ETH_CMD_READ_PHY_REG, &reg);
        if (enable) {
            reg_val |= 1 << 7;
        } else {
            reg_val &= ~(1 << 7);
        }
        break;
    case PHY_RTL8201:
    case PHY_DP83848: // TODO DP83848 offers BIST => investigate if could be used instead
        ESP_LOGE(TAG, "far-end loopback is not supported by selected PHY");
        return ESP_ERR_NOT_SUPPORTED;
    default:

        return ESP_FAIL;
    }
    // it was observed that e.g. IP101 needs to be commanded multiple time to take it effect
    uint8_t attempt = 0;
    do {
        esp_eth_ioctl(eth_handle, ETH_CMD_WRITE_PHY_REG, &reg);
        reg_val_exp = reg_val;
        esp_eth_ioctl(eth_handle, ETH_CMD_READ_PHY_REG, &reg);
        attempt++;
    } while (reg_val_exp != reg_val && attempt < 10);

    if (attempt >= 10) {
        ESP_LOGE(TAG, "error to configure far-end loopback");
        ESP_LOGE(TAG, "expected reg. val 0x%lx, actual 0x%lx", reg_val_exp, reg_val);
        return ESP_FAIL;
    }

    if (enable) {
        ESP_GOTO_ON_ERROR(esp_eth_start(eth_handle), err, TAG, "failed to start Ethernet"); // just to see link status
    } else {
        ESP_GOTO_ON_ERROR(esp_eth_stop(eth_handle), err, TAG, "failed to stop Ethernet");
    }
err:
    return ret;
}
