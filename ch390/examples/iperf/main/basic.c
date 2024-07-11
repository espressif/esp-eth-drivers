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
#include "driver/spi_master.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>
#include "sdkconfig.h"
#include "esp_eth_mac_ch390.h"
#include "esp_eth_phy_ch390.h"
#include "basic.h"

static const char *TAG = "basic";

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
        .miso_io_num = CONFIG_IPERF_ETH_SPI_MISO_GPIO,
        .mosi_io_num = CONFIG_IPERF_ETH_SPI_MOSI_GPIO,
        .sclk_io_num = CONFIG_IPERF_ETH_SPI_SCLK_GPIO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        // .flags = SPICOMMON_BUSFLAG_IOMUX_PINS
    };
    ESP_GOTO_ON_ERROR(spi_bus_initialize(CONFIG_IPERF_ETH_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO),
                      err, TAG, "SPI host #%d init failed", 2);

err:
    return ret;
}

void basic_init(esp_eth_handle_t *handle)
{
    spi_bus_init();

    // Init common MAC and PHY configs to default
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    mac_config.rx_task_stack_size = 8192;

    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.reset_gpio_num = -1;

    // Configure SPI interface for specific SPI module
    spi_device_interface_config_t spi_devcfg = {
        .mode = 0,
        .clock_speed_hz = CONFIG_IPERF_ETH_SPI_CLOCK_MHZ * 1000 * 1000,
        .queue_size = 16,
        .spics_io_num = CONFIG_IPERF_ETH_SPI_CS_GPIO};

    eth_ch390_config_t ch390_config = ETH_CH390_DEFAULT_CONFIG(CONFIG_IPERF_ETH_SPI_HOST, &spi_devcfg);
    ch390_config.int_gpio_num = CONFIG_IPERF_ETH_SPI_INT_GPIO;
#if CONFIG_EXAMPLE_ETH_SPI_INT_GPIO < 0
    ch390_config.poll_period_ms = CONFIG_IPERF_ETH_SPI_POLLING_MS_VAL;
#endif

    esp_eth_mac_t *mac = esp_eth_mac_new_ch390(&ch390_config, &mac_config);
    esp_eth_phy_t *phy = esp_eth_phy_new_ch390(&phy_config);

    // Init Ethernet driver to default and install it
    esp_eth_config_t eth_config_spi = ETH_DEFAULT_CONFIG(mac, phy);

    esp_eth_driver_install(&eth_config_spi, handle);
}
