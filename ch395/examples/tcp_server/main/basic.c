/*
 * SPDX-FileCopyrightText: 2024 Sergey Kharenko
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileContributor: 2024 Sergey Kharenko
 * SPDX-FileContributor: 2024 Espressif Systems (Shanghai) CO LTD
 */
#include <stdio.h>

#include "driver/gpio.h"
#include "driver/spi_common.h"
#include "driver/spi_master.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_check.h"
#include <lwip/netdb.h>
#include "hal/uart_types.h"
#include "sdkconfig.h"
#include "esp_eth_mac_ch395.h"
#include "esp_eth_phy_ch395.h"
#include "basic.h"

static const char *TAG = "basic";

#if CONFIG_ETH_CH395_INTERFACE_SPI
static esp_err_t spi_bus_init(void)
{
    esp_err_t ret = ESP_OK;

    // Install GPIO ISR handler to be able to service SPI Eth modules interrupts
    ret = gpio_install_isr_service(0);
    if (ret != ESP_OK) {
        if (ret == ESP_ERR_INVALID_STATE) {
            ESP_LOGW(TAG, "GPIO ISR handler has been already installed");
            ret = ESP_OK; // ISR handler has been already installed so no issues
        } else {
            ESP_LOGE(TAG, "GPIO ISR handler install failed");
            goto err;
        }
    }

    // Init SPI bus
    spi_bus_config_t buscfg = {
        .miso_io_num = CONFIG_TCPSERVER_ETH_SPI_MISO_GPIO,
        .mosi_io_num = CONFIG_TCPSERVER_ETH_SPI_MOSI_GPIO,
        .sclk_io_num = CONFIG_TCPSERVER_ETH_SPI_SCLK_GPIO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        // .flags = SPICOMMON_BUSFLAG_IOMUX_PINS
    };
    ESP_GOTO_ON_ERROR(spi_bus_initialize(CONFIG_TCPSERVER_ETH_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO),
                      err, TAG, "SPI host #%d init failed", 2);

err:
    return ret;
}
#endif

#if CONFIG_ETH_CH395_INTERFACE_UART
static esp_err_t intr_init(void)
{
    esp_err_t ret = ESP_OK;

    // Install GPIO ISR handler to be able to service SPI Eth modules interrupts
    ret = gpio_install_isr_service(0);
    if (ret != ESP_OK) {
        if (ret == ESP_ERR_INVALID_STATE) {
            ESP_LOGW(TAG, "GPIO ISR handler has been already installed");
            ret = ESP_OK; // ISR handler has been already installed so no issues
        } else {
            ESP_LOGE(TAG, "GPIO ISR handler install failed");
            goto err;
        }
    }

err:
    return ret;
}
#endif

void basic_init(esp_eth_handle_t *handle)
{
#if CONFIG_ETH_CH395_INTERFACE_SPI
    spi_bus_init();
#endif

#if CONFIG_ETH_CH395_INTERFACE_UART
    intr_init();
#endif

    // Init common MAC and PHY configs to default
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    mac_config.rx_task_stack_size = 8192;

    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.reset_gpio_num = -1;

#if CONFIG_ETH_CH395_INTERFACE_SPI
    // Configure SPI interface for specific SPI module
    spi_device_interface_config_t spi_devcfg = {
        .mode = 0,
        .clock_speed_hz = CONFIG_TCPSERVER_ETH_SPI_CLOCK_MHZ * 1000 * 1000,
        .queue_size = 16,
        .spics_io_num = -1
    };

    eth_ch395_config_t ch395_config = ETH_CH395_DEFAULT_CONFIG(CONFIG_TCPSERVER_ETH_SPI_HOST, &spi_devcfg);
    ch395_config.int_gpio_num = CONFIG_TCPSERVER_ETH_INT_GPIO;
    ch395_config.spi_cs_gpio_num = CONFIG_TCPSERVER_ETH_SPI_CS_GPIO;
#endif

#if CONFIG_ETH_CH395_INTERFACE_UART
    // Configure UART interface for specific UART module
    uart_config_t uart_devcfg = {
        .baud_rate = CONFIG_TCPSERVER_ETH_UART_BAUDRATE,
        .data_bits = UART_DATA_8_BITS,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .parity = UART_PARITY_DISABLE,
    };

    eth_ch395_config_t ch395_config = ETH_CH395_DEFAULT_CONFIG(CONFIG_TCPSERVER_ETH_UART_PORT, &uart_devcfg);
    ch395_config.uart_tx_gpio_num = CONFIG_TCPSERVER_ETH_UART_TX_GPIO;
    ch395_config.uart_rx_gpio_num = CONFIG_TCPSERVER_ETH_UART_RX_GPIO;
    ch395_config.int_gpio_num = CONFIG_TCPSERVER_ETH_INT_GPIO;
#endif

#if CONFIG_TCPSERVER_ETH_INT_GPIO < 0
    ch395_config.poll_period_ms = CONFIG_TCPSERVER_ETH_POLLING_MS_VAL;
#endif

    esp_eth_mac_t *mac = esp_eth_mac_new_ch395(&ch395_config, &mac_config);
    esp_eth_phy_t *phy = esp_eth_phy_new_ch395(&phy_config);

    // Init Ethernet driver to default and install it
    esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(mac, phy);

    esp_eth_driver_install(&eth_config, handle);
}
