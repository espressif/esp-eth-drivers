/*
 * SPDX-FileCopyrightText: 2023-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <inttypes.h>
#include "ethernet_init.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_check.h"
#include "esp_mac.h"
#include "driver/gpio.h"
#include "esp_idf_version.h"
#include "sdkconfig.h"
#if CONFIG_ETHERNET_SPI_SUPPORT
#include "driver/spi_master.h"
#endif // CONFIG_ETHERNET_SPI_SUPPORT

#if CONFIG_ETHERNET_PHY_LAN867X
#include "esp_eth_phy_lan867x.h"
#endif // CONFIG_ETHERNET_PHY_LAN867X

#if CONFIG_ETHERNET_SPI_USE_CH390
#include "esp_eth_mac_ch390.h"
#include "esp_eth_phy_ch390.h"
#endif // CONFIG_ETHERNET_SPI_USE_CH390

#if CONFIG_ETHERNET_SPI_USE_ENC28J60
#include "esp_eth_enc28j60.h"
#endif // CONFIG_ETHERNET_SPI_USE_ENC28J60

#if CONFIG_ETHERNET_SPI_USE_LAN865X
#include "esp_eth_mac_lan865x.h"
#include "esp_eth_phy_lan865x.h"
#endif // CONFIG_ETHERNET_PHY_LAN865X

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
#if CONFIG_ETHERNET_SPI_USE_DM9051
#include "esp_eth_mac_dm9051.h"
#include "esp_eth_phy_dm9051.h"
#endif // CONFIG_ETHERNET_SPI_USE_DM9051

#if CONFIG_ETHERNET_SPI_USE_KSZ8851SNL
#include "esp_eth_mac_ksz8851snl.h"
#include "esp_eth_phy_ksz8851snl.h"
#endif // CONFIG_ETHERNET_SPI_USE_KSZ8851SNL

#if CONFIG_ETHERNET_SPI_USE_W5500
#include "esp_eth_mac_w5500.h"
#include "esp_eth_phy_w5500.h"
#endif // CONFIG_ETHERNET_SPI_USE_W5500

#if CONFIG_ETHERNET_PHY_IP101
#include "esp_eth_phy_ip101.h"
#endif // CONFIG_ETHERNET_PHY_IP101

#if CONFIG_ETHERNET_PHY_LAN87XX
#include "esp_eth_phy_lan87xx.h"
#endif // CONFIG_ETHERNET_PHY_LAN87XX

#if CONFIG_ETHERNET_PHY_DP83848
#include "esp_eth_phy_dp83848.h"
#endif // CONFIG_ETHERNET_PHY_DP83848

#if CONFIG_ETHERNET_PHY_RTL8201
#include "esp_eth_phy_rtl8201.h"
#endif // CONFIG_ETHERNET_PHY_RTL8201

#if CONFIG_ETHERNET_PHY_KSZ80XX
#include "esp_eth_phy_ksz80xx.h"
#endif // CONFIG_ETHERNET_PHY_KSZ80XX
#endif // ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)

#define INIT_SPI_ETH_MODULE_CONFIG(eth_module_config, num)                                   \
    do {                                                                                     \
        eth_module_config[num].dev = CONFIG_ETHERNET_SPI_DEV ##num## _ID;                   \
        eth_module_config[num].spi_cs_gpio = CONFIG_ETHERNET_SPI_CS ##num## _GPIO;           \
        eth_module_config[num].int_gpio = CONFIG_ETHERNET_SPI_INT ##num## _GPIO;             \
        eth_module_config[num].poll_period_ms = CONFIG_ETHERNET_SPI_POLLING ##num## _MS;     \
        eth_module_config[num].phy_reset_gpio = CONFIG_ETHERNET_SPI_PHY_RST ##num## _GPIO;   \
        eth_module_config[num].phy_addr = CONFIG_ETHERNET_SPI_PHY_ADDR ##num;                \
    } while(0)

#if !defined(CONFIG_ETHERNET_INTERNAL_SUPPORT)
#define CONFIG_ETHERNET_INTERNAL_SUPPORT 0
#endif

#if !defined(CONFIG_ETHERNET_SPI_SUPPORT)
#define ETHERNET_SPI_NUMBER 0
#elif defined(CONFIG_ETHERNET_SPI_DEV1_NONE)
#define ETHERNET_SPI_NUMBER 1
#else
#define ETHERNET_SPI_NUMBER 2
#endif

#if !defined(CONFIG_ETHERNET_OPENETH_SUPPORT)
#define CONFIG_ETHERNET_OPENETH_SUPPORT 0
#endif

/* This enum definition must be aligned with the ETHERNET_SPI_USE_ID* definitions in Kconfig.projbuild */
typedef enum {
    SPI_DEV_TYPE_DM9051,
    SPI_DEV_TYPE_KSZ8851SNL,
    SPI_DEV_TYPE_W5500,
    SPI_DEV_TYPE_CH390,
    SPI_DEV_TYPE_ENC28J60,
    SPI_DEV_TYPE_LAN865X,
} spi_eth_dev_type_t;

typedef struct {
    spi_eth_dev_type_t dev;
    uint8_t spi_cs_gpio;
    int8_t int_gpio;
    uint32_t poll_period_ms;
    int8_t phy_reset_gpio;
    uint8_t phy_addr;
    uint8_t *mac_addr;
} spi_eth_module_config_t;

typedef enum {
    DEV_STATE_UNINITIALIZED,
    DEV_STATE_INITIALIZED,
} dev_state;

typedef struct {
    esp_eth_handle_t eth_handle;
    dev_state state;
    eth_dev_info_t dev_info;
} eth_device;

static const char *TAG = "ethernet_init";
static uint8_t eth_cnt_g = 0;
#if CONFIG_ETHERNET_INTERNAL_SUPPORT || CONFIG_ETHERNET_SPI_SUPPORT || CONFIG_ETHERNET_OPENETH_SUPPORT
static eth_device eth_instance_g[CONFIG_ETHERNET_INTERNAL_SUPPORT + ETHERNET_SPI_NUMBER + CONFIG_ETHERNET_OPENETH_SUPPORT];
#if CONFIG_ETHERNET_SPI_SUPPORT
static bool spi_bus_deinit_g = false;
#endif // CONFIG_ETHERNET_SPI_SUPPORT
#if CONFIG_ETHERNET_DEFAULT_EVENT_HANDLER
static esp_event_handler_instance_t eth_event_ctx_g;

static void eth_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
    uint8_t pin1 = 0, pin2 = 0;
    uint8_t mac_addr[ETH_ADDR_LEN] = {0};
    // we can get the ethernet driver handle from event data
    esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;
    eth_dev_info_t dev_info = ethernet_init_get_dev_info(eth_handle);

    // check if the handle is valid and was created by "ethernet_init" component
    if (dev_info.type == ETH_DEV_TYPE_UNKNOWN) {
        return;
    }

    if (dev_info.type == ETH_DEV_TYPE_INTERNAL_ETH) {
        pin1 = dev_info.pin.eth_internal_mdc;
        pin2 = dev_info.pin.eth_internal_mdio;
    } else if (dev_info.type == ETH_DEV_TYPE_SPI) {
        pin1 = dev_info.pin.eth_spi_cs;
        pin2 = dev_info.pin.eth_spi_int;
    }

    switch (event_id) {
    case ETHERNET_EVENT_CONNECTED:
        esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
        ESP_LOGI(TAG, "Ethernet(%s[%d,%d]) Link Up", dev_info.name, pin1, pin2);
        ESP_LOGI(TAG, "Ethernet(%s[%d,%d]) HW Addr %02x:%02x:%02x:%02x:%02x:%02x", dev_info.name, pin1, pin2,
                 mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
        break;
    case ETHERNET_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "Ethernet(%s[%d,%d]) Link Down", dev_info.name, pin1, pin2);
        break;
    case ETHERNET_EVENT_START:
        ESP_LOGI(TAG, "Ethernet(%s[%d,%d]) Started", dev_info.name, pin1, pin2);
        break;
    case ETHERNET_EVENT_STOP:
        ESP_LOGI(TAG, "Ethernet(%s[%d,%d]) Stopped", dev_info.name, pin1, pin2);
        break;
    default:
        ESP_LOGI(TAG, "Default Event");
        break;
    }
}
#endif // CONFIG_ETHERNET_DEFAULT_EVENT_HANDLER

#if CONFIG_ETHERNET_BOARD_SPECIFIC_INIT_WEAK
__attribute__((weak)) esp_err_t eth_board_specific_init(esp_eth_handle_t eth_handle)
{
    ESP_LOGW(TAG, "No board specific init defined - define your own init function");
    return ESP_OK;
}
#else
esp_err_t eth_board_specific_init(esp_eth_handle_t eth_handle)
{
#ifdef CONFIG_ETHERNET_EXT_OSC_EN_GPIO_NUM
    if (CONFIG_ETHERNET_EXT_OSC_EN_GPIO_NUM >= 0) {
        uint32_t pin = CONFIG_ETHERNET_EXT_OSC_EN_GPIO_NUM;
        gpio_config_t gpio_sink_cfg = {
            .pin_bit_mask = (1ULL << pin),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE
        };
        gpio_config(&gpio_sink_cfg);
        // enable external oscillator
        gpio_set_level(pin, 1);
    }
#endif
    return ESP_OK;
}
#endif
#endif // CONFIG_ETHERNET_INTERNAL_SUPPORT || CONFIG_ETHERNET_SPI_SUPPORT || CONFIG_ETHERNET_OPENETH_SUPPORT

#if CONFIG_ETHERNET_INTERNAL_SUPPORT
/**
 * @brief Internal ESP32 Ethernet initialization
 *
 * @param[out] dev_name device name string
 * @return
 *          - esp_eth_handle_t if init succeeded
 *          - NULL if init failed
 */
static esp_eth_handle_t eth_init_internal(char *dev_name)
{
    esp_eth_handle_t ret = NULL;

    if (dev_name == NULL) {
        ESP_LOGE(TAG, "dev_name NULL");
        return ret;
    }

    // Init common MAC configs to default
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
#if CONFIG_ETHERNET_RX_TASK_STACK_SIZE > 0
    mac_config.rx_task_stack_size = CONFIG_ETHERNET_RX_TASK_STACK_SIZE;
#endif // CONFIG_ETHERNET_RX_TASK_STACK_SIZE > 0

    // Init vendor specific MAC config to default
    eth_esp32_emac_config_t esp32_emac_config = ETH_ESP32_EMAC_DEFAULT_CONFIG();

    // Update vendor specific MAC config based on board configuration
    esp32_emac_config.smi_gpio.mdc_num = CONFIG_ETHERNET_MDC_GPIO;
    esp32_emac_config.smi_gpio.mdio_num = CONFIG_ETHERNET_MDIO_GPIO;

#if CONFIG_ETHERNET_PHY_INTERFACE_RMII
    // Configure RMII based on Kconfig when non-default configuration selected
    esp32_emac_config.interface = EMAC_DATA_INTERFACE_RMII;

    // Configure RMII clock mode and GPIO
#if CONFIG_ETHERNET_RMII_CLK_INPUT
    esp32_emac_config.clock_config.rmii.clock_mode = EMAC_CLK_EXT_IN;
#else // CONFIG_ETHERNET_RMII_CLK_OUTPUT
    esp32_emac_config.clock_config.rmii.clock_mode = EMAC_CLK_OUT;
#endif
    esp32_emac_config.clock_config.rmii.clock_gpio = CONFIG_ETHERNET_RMII_CLK_GPIO;

#if CONFIG_ETHERNET_RMII_CLK_EXT_LOOPBACK_EN
    esp32_emac_config.clock_config_out_in.rmii.clock_gpio = CONFIG_ETHERNET_RMII_CLK_EXT_LOOPBACK_IN_GPIO;
    esp32_emac_config.clock_config_out_in.rmii.clock_mode = EMAC_CLK_EXT_IN;
#endif

#if SOC_EMAC_USE_MULTI_IO_MUX
    // Configure RMII datapane GPIOs
    esp32_emac_config.emac_dataif_gpio.rmii.tx_en_num = CONFIG_ETHERNET_RMII_TX_EN_GPIO;
    esp32_emac_config.emac_dataif_gpio.rmii.txd0_num = CONFIG_ETHERNET_RMII_TXD0_GPIO;
    esp32_emac_config.emac_dataif_gpio.rmii.txd1_num = CONFIG_ETHERNET_RMII_TXD1_GPIO;
    esp32_emac_config.emac_dataif_gpio.rmii.crs_dv_num = CONFIG_ETHERNET_RMII_CRS_DV_GPIO;
    esp32_emac_config.emac_dataif_gpio.rmii.rxd0_num = CONFIG_ETHERNET_RMII_RXD0_GPIO;
    esp32_emac_config.emac_dataif_gpio.rmii.rxd1_num = CONFIG_ETHERNET_RMII_RXD1_GPIO;
#endif // SOC_EMAC_USE_MULTI_IO_MUX
#endif // CONFIG_ETHERNET_PHY_INTERFACE_RMII

#if CONFIG_ETHERNET_DMA_BURST_LEN_1
    esp32_emac_config.dma_burst_len = ETH_DMA_BURST_LEN_1;
#elif CONFIG_ETHERNET_DMA_BURST_LEN_2
    esp32_emac_config.dma_burst_len = ETH_DMA_BURST_LEN_2;
#elif CONFIG_ETHERNET_DMA_BURST_LEN_4
    esp32_emac_config.dma_burst_len = ETH_DMA_BURST_LEN_4;
#elif CONFIG_ETHERNET_DMA_BURST_LEN_8
    esp32_emac_config.dma_burst_len = ETH_DMA_BURST_LEN_8;
#elif CONFIG_ETHERNET_DMA_BURST_LEN_16
    esp32_emac_config.dma_burst_len = ETH_DMA_BURST_LEN_16;
#elif CONFIG_ETHERNET_DMA_BURST_LEN_32
    esp32_emac_config.dma_burst_len = ETH_DMA_BURST_LEN_32;
#endif // CONFIG_ETHERNET_DMA_BURST_LEN_1

#if CONFIG_ETHERNET_SPI_SUPPORT && !(CONFIG_ETHERNET_DMA_BURST_LEN_1 || CONFIG_ETHERNET_DMA_BURST_LEN_2 || CONFIG_ETHERNET_DMA_BURST_LEN_4 || CONFIG_ETHERNET_DMA_BURST_LEN_8)
#warning "SPI Ethernet is used along with internal EMAC, consider lowering EMAC DMA burst length to 1, 2, 4 or 8 beats"
#endif
    esp_eth_mac_t *mac = NULL;
    esp_eth_phy_t *phy = NULL;

    // Create new ESP32 Ethernet MAC instance
    mac = esp_eth_mac_new_esp32(&esp32_emac_config, &mac_config);

    // Init common PHY configs to default
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();

    // Update PHY config based on board specific configuration
    phy_config.phy_addr = CONFIG_ETHERNET_PHY_ADDR;
    phy_config.reset_gpio_num = CONFIG_ETHERNET_PHY_RST_GPIO;
#if CONFIG_ETHERNET_PHY_RST_TIMING_EN
    phy_config.hw_reset_assert_time_us = CONFIG_ETHERNET_PHY_RST_ASSERT_TIME_US;
    phy_config.post_hw_reset_delay_ms = CONFIG_ETHERNET_PHY_RST_DELAY_MS;
#endif // CONFIG_ETHERNET_PHY_RST_TIMING_EN

    // Create new PHY instance based on board configuration
#if CONFIG_ETHERNET_PHY_GENERIC
    phy = esp_eth_phy_new_generic(&phy_config);
#elif CONFIG_ETHERNET_PHY_IP101
    phy = esp_eth_phy_new_ip101(&phy_config);
    (void)snprintf(dev_name, ETH_DEV_NAME_MAX_LEN, "IP101");
#elif CONFIG_ETHERNET_PHY_RTL8201
    phy = esp_eth_phy_new_rtl8201(&phy_config);
    (void)snprintf(dev_name, ETH_DEV_NAME_MAX_LEN, "RTL8201");
#elif CONFIG_ETHERNET_PHY_LAN87XX
    phy = esp_eth_phy_new_lan87xx(&phy_config);
    (void)snprintf(dev_name, ETH_DEV_NAME_MAX_LEN, "LAN87XX");
#elif CONFIG_ETHERNET_PHY_DP83848
    phy = esp_eth_phy_new_dp83848(&phy_config);
    (void)snprintf(dev_name, ETH_DEV_NAME_MAX_LEN, "DP83848");
#elif CONFIG_ETHERNET_PHY_KSZ80XX
    phy = esp_eth_phy_new_ksz80xx(&phy_config);
    (void)snprintf(dev_name, ETH_DEV_NAME_MAX_LEN, "KSZ80XX");
#elif CONFIG_ETHERNET_PHY_LAN867X
    phy = esp_eth_phy_new_lan867x(&phy_config);
    (void)snprintf(dev_name, ETH_DEV_NAME_MAX_LEN, "LAN867X");
#endif

    // Init Ethernet driver to default and install it
    esp_eth_handle_t eth_handle = NULL;
    esp_eth_config_t config = ETH_DEFAULT_CONFIG(mac, phy);
    config.on_lowlevel_init_done = eth_board_specific_init;
    ESP_GOTO_ON_FALSE(esp_eth_driver_install(&config, &eth_handle) == ESP_OK, NULL,
                      err, TAG, "Ethernet driver install failed");

    return eth_handle;
err:
    if (eth_handle != NULL) {
        esp_eth_driver_uninstall(eth_handle);
    }
    if (mac != NULL) {
        mac->del(mac);
    }
    if (phy != NULL) {
        phy->del(phy);
    }
    return ret;
}
#endif // CONFIG_ETHERNET_INTERNAL_SUPPORT

#if CONFIG_ETHERNET_SPI_SUPPORT
/**
 * @brief SPI bus initialization (to be used by Ethernet SPI modules)
 *
 * @return
 *          - ESP_OK on success
 */
static esp_err_t spi_bus_init(void)
{
    esp_err_t ret = ESP_OK;

    // Install GPIO ISR handler to be able to service SPI Eth modules interrupts
    ret = gpio_install_isr_service(0);
    if (ret != ESP_OK) {
        if (ret == ESP_ERR_INVALID_STATE) {
            ESP_LOGD(TAG, "GPIO ISR handler has been already installed");
            ret = ESP_OK; // ISR handler has been already installed so no issues
        } else {
            ESP_LOGE(TAG, "GPIO ISR handler install failed");
            goto err;
        }
    }

    // Init SPI bus
    spi_bus_config_t buscfg = {
        .miso_io_num = CONFIG_ETHERNET_SPI_MISO_GPIO,
        .mosi_io_num = CONFIG_ETHERNET_SPI_MOSI_GPIO,
        .sclk_io_num = CONFIG_ETHERNET_SPI_SCLK_GPIO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };

    ret = spi_bus_initialize(CONFIG_ETHERNET_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret == ESP_OK) {
        // SPI bus initialized by us, so we need to deinitialize it later on deinit
        spi_bus_deinit_g = true;
    } else {
        if (ret == ESP_ERR_INVALID_STATE) {
            ESP_LOGD(TAG, "SPI host #%d has been already initialized", CONFIG_ETHERNET_SPI_HOST);
            ret = ESP_OK; // SPI host has been already initialized so no issues
        } else {
            ESP_LOGE(TAG, "SPI host #%d init failed", CONFIG_ETHERNET_SPI_HOST);
            goto err;
        }
    }

    return ESP_OK;
err:
    return ret;
}

/**
 * @brief Ethernet SPI modules initialization
 *
 * @param[in] spi_eth_module_config specific SPI Ethernet module configuration
 * @param[out] dev_name device name string
 * @return
 *          - esp_eth_handle_t if init succeeded
 *          - NULL if init failed
 */
static esp_eth_handle_t eth_init_spi(spi_eth_module_config_t *spi_eth_module_config, char *dev_name)
{
    esp_eth_handle_t ret = NULL;

    if (dev_name == NULL) {
        ESP_LOGE(TAG, "dev_name NULL");
        return ret;
    }

    esp_eth_mac_t *mac = NULL;
    esp_eth_phy_t *phy = NULL;

    // Init common MAC and PHY configs to default
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
#if CONFIG_ETHERNET_RX_TASK_STACK_SIZE > 0
    mac_config.rx_task_stack_size = CONFIG_ETHERNET_RX_TASK_STACK_SIZE;
#endif // CONFIG_ETHERNET_RX_TASK_STACK_SIZE > 0
#if CONFIG_ETHERNET_RX_TASK_PRIO > -1
    mac_config.rx_task_prio = CONFIG_ETHERNET_RX_TASK_PRIO;
#endif // CONFIG_ETHERNET_RX_TASK_PRIO > -1
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();

    // Update PHY config based on board specific configuration
    phy_config.phy_addr = spi_eth_module_config->phy_addr;
    phy_config.reset_gpio_num = spi_eth_module_config->phy_reset_gpio;

    // Configure SPI interface for specific SPI module
    spi_device_interface_config_t spi_devcfg = {
        .mode = 0,
        .clock_speed_hz = CONFIG_ETHERNET_SPI_CLOCK_MHZ * 1000 * 1000,
        .queue_size = 20,
        .spics_io_num = spi_eth_module_config->spi_cs_gpio
    };
    /* Init vendor specific MAC config to default, and create new SPI Ethernet MAC instance
       and new PHY instance based on board configuration */
    if (spi_eth_module_config->dev == SPI_DEV_TYPE_KSZ8851SNL) {
#if CONFIG_ETHERNET_SPI_USE_KSZ8851SNL
        eth_ksz8851snl_config_t ksz8851snl_config = ETH_KSZ8851SNL_DEFAULT_CONFIG(CONFIG_ETHERNET_SPI_HOST, &spi_devcfg);
        ksz8851snl_config.int_gpio_num = spi_eth_module_config->int_gpio;
        ksz8851snl_config.poll_period_ms = spi_eth_module_config->poll_period_ms;
        mac = esp_eth_mac_new_ksz8851snl(&ksz8851snl_config, &mac_config);
        phy = esp_eth_phy_new_ksz8851snl(&phy_config);
        (void)snprintf(dev_name, ETH_DEV_NAME_MAX_LEN, "KSZ8851SNL");
#endif // CONFIG_ETHERNET_SPI_USE_KSZ8851SNL
    } else if (spi_eth_module_config->dev == SPI_DEV_TYPE_DM9051) {
#if CONFIG_ETHERNET_SPI_USE_DM9051
        eth_dm9051_config_t dm9051_config = ETH_DM9051_DEFAULT_CONFIG(CONFIG_ETHERNET_SPI_HOST, &spi_devcfg);
        dm9051_config.int_gpio_num = spi_eth_module_config->int_gpio;
        dm9051_config.poll_period_ms = spi_eth_module_config->poll_period_ms;
        mac = esp_eth_mac_new_dm9051(&dm9051_config, &mac_config);
        phy = esp_eth_phy_new_dm9051(&phy_config);
        (void)snprintf(dev_name, ETH_DEV_NAME_MAX_LEN, "DM9051");
#endif // CONFIG_ETHERNET_SPI_USE_DM9051
    } else if (spi_eth_module_config->dev == SPI_DEV_TYPE_W5500) {
#if CONFIG_ETHERNET_SPI_USE_W5500
        eth_w5500_config_t w5500_config = ETH_W5500_DEFAULT_CONFIG(CONFIG_ETHERNET_SPI_HOST, &spi_devcfg);
        w5500_config.int_gpio_num = spi_eth_module_config->int_gpio;
        w5500_config.poll_period_ms = spi_eth_module_config->poll_period_ms;
        mac = esp_eth_mac_new_w5500(&w5500_config, &mac_config);
        phy = esp_eth_phy_new_w5500(&phy_config);
        (void)snprintf(dev_name, ETH_DEV_NAME_MAX_LEN, "W5500");
#endif // CONFIG_ETHERNET_SPI_USE_W5500
    } else if (spi_eth_module_config->dev == SPI_DEV_TYPE_CH390) {
#if CONFIG_ETHERNET_SPI_USE_CH390
        eth_ch390_config_t ch390_config = ETH_CH390_DEFAULT_CONFIG(CONFIG_ETHERNET_SPI_HOST, &spi_devcfg);
        ch390_config.int_gpio_num = spi_eth_module_config->int_gpio;
        ch390_config.poll_period_ms = spi_eth_module_config->poll_period_ms;
        mac = esp_eth_mac_new_ch390(&ch390_config, &mac_config);
        phy = esp_eth_phy_new_ch390(&phy_config);
        (void)snprintf(dev_name, ETH_DEV_NAME_MAX_LEN, "CH390");
#endif // CONFIG_ETHERNET_SPI_USE_CH390
    } else if (spi_eth_module_config->dev == SPI_DEV_TYPE_ENC28J60) {
#if CONFIG_ETHERNET_SPI_USE_ENC28J60
        spi_devcfg.cs_ena_posttrans = enc28j60_cal_spi_cs_hold_time(CONFIG_ETHERNET_SPI_CLOCK_MHZ);
        eth_enc28j60_config_t enc28j60_config = ETH_ENC28J60_DEFAULT_CONFIG(CONFIG_ETHERNET_SPI_HOST, &spi_devcfg);
        enc28j60_config.int_gpio_num = spi_eth_module_config->int_gpio;
        mac = esp_eth_mac_new_enc28j60(&enc28j60_config, &mac_config);

        // ENC28J60 Errata #1 check
        ESP_GOTO_ON_FALSE(mac, NULL, err, TAG, "creation of ENC28J60 MAC instance failed");
        ESP_GOTO_ON_FALSE(emac_enc28j60_get_chip_info(mac) >= ENC28J60_REV_B5 || CONFIG_ETHERNET_SPI_CLOCK_MHZ >= 8,
                          NULL, err, TAG, "SPI frequency must be at least 8 MHz for chip revision less than 5");

        phy_config.autonego_timeout_ms = 0; // ENC28J60 doesn't support auto-negotiation
        phy_config.reset_gpio_num = -1; // ENC28J60 doesn't have a pin to reset internal PHY
        phy = esp_eth_phy_new_enc28j60(&phy_config);
        (void)snprintf(dev_name, ETH_DEV_NAME_MAX_LEN, "ENC28J60");
#endif // CONFIG_ETHERNET_SPI_USE_ENC28J60
    } else if (spi_eth_module_config->dev == SPI_DEV_TYPE_LAN865X) {
#if CONFIG_ETHERNET_SPI_USE_LAN865X
        eth_lan865x_config_t lan865x_config = ETH_LAN865X_DEFAULT_CONFIG(CONFIG_ETHERNET_SPI_HOST, &spi_devcfg);
        lan865x_config.int_gpio_num = spi_eth_module_config->int_gpio;
        lan865x_config.poll_period_ms = spi_eth_module_config->poll_period_ms;

        mac = esp_eth_mac_new_lan865x(&lan865x_config, &mac_config);
        phy = esp_eth_phy_new_lan865x(&phy_config);
        (void)snprintf(dev_name, ETH_DEV_NAME_MAX_LEN, "LAN865X");
#endif // CONFIG_ETHERNET_SPI_USE_LAN865X
    } else {
        ESP_LOGE(TAG, "Unsupported SPI Ethernet module type ID: %i", spi_eth_module_config->dev);
        goto err;
    }

    // Init Ethernet driver to default and install it
    esp_eth_handle_t eth_handle = NULL;
    esp_eth_config_t eth_config_spi = ETH_DEFAULT_CONFIG(mac, phy);
    ESP_GOTO_ON_FALSE(esp_eth_driver_install(&eth_config_spi, &eth_handle) == ESP_OK, NULL, err, TAG, "SPI Ethernet driver install failed");

    // The SPI Ethernet module might not have a burned factory MAC address, we can set it manually.
    if (spi_eth_module_config->mac_addr != NULL) {
        ESP_GOTO_ON_FALSE(esp_eth_ioctl(eth_handle, ETH_CMD_S_MAC_ADDR, spi_eth_module_config->mac_addr) == ESP_OK,
                          NULL, err, TAG, "SPI Ethernet MAC address config failed");
    }

    return eth_handle;
err:
    if (eth_handle != NULL) {
        esp_eth_driver_uninstall(eth_handle);
    }
    if (mac != NULL) {
        mac->del(mac);
    }
    if (phy != NULL) {
        phy->del(phy);
    }
    return ret;
}
#endif // CONFIG_ETHERNET_SPI_SUPPORT

#if CONFIG_ETHERNET_OPENETH_SUPPORT
/**
 * @brief OpenCores Ethernet initialization
 *
 * @param[out] dev_name device name string
 * @return esp_eth_handle_t
 */
static esp_eth_handle_t eth_init_openeth(char *dev_name)
{
    esp_eth_handle_t ret = NULL;
    if (dev_name == NULL) {
        ESP_LOGE(TAG, "dev_name NULL");
        return ret;
    }

    esp_eth_mac_t *mac = NULL;
    esp_eth_phy_t *phy = NULL;

    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
#if CONFIG_ETHERNET_RX_TASK_STACK_SIZE > 0
    mac_config.rx_task_stack_size = CONFIG_ETHERNET_RX_TASK_STACK_SIZE;
#endif // CONFIG_ETHERNET_RX_TASK_STACK_SIZE > 0
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();

    phy_config.autonego_timeout_ms = 100;
    mac = esp_eth_mac_new_openeth(&mac_config);
    phy = esp_eth_phy_new_generic(&phy_config);
    (void)snprintf(dev_name, ETH_DEV_NAME_MAX_LEN, "OPENETH");

    // Init Ethernet driver to default and install it
    esp_eth_handle_t eth_handle = NULL;
    esp_eth_config_t eth_config_openeth = ETH_DEFAULT_CONFIG(mac, phy);
    ESP_GOTO_ON_FALSE(esp_eth_driver_install(&eth_config_openeth, &eth_handle) == ESP_OK, NULL, err, TAG, "OPENETH Ethernet driver install failed");
    return eth_handle;
err:
    if (mac != NULL) {
        mac->del(mac);
    }
    if (phy != NULL) {
        phy->del(phy);
    }
    return ret;
}
#endif // CONFIG_ETHERNET_OPENETH_SUPPORT

esp_err_t ethernet_init_all(esp_eth_handle_t *eth_handles_out[], uint8_t *eth_cnt_out)
{
    esp_err_t ret = ESP_OK;
    esp_eth_handle_t *eth_handles = NULL;

#if CONFIG_ETHERNET_INTERNAL_SUPPORT || CONFIG_ETHERNET_SPI_SUPPORT || CONFIG_ETHERNET_OPENETH_SUPPORT
    ESP_GOTO_ON_FALSE(eth_handles_out != NULL && eth_cnt_out != NULL, ESP_ERR_INVALID_ARG,
                      err, TAG, "invalid arguments: initialized handles array or number of interfaces");
    eth_handles = calloc(CONFIG_ETHERNET_INTERNAL_SUPPORT + ETHERNET_SPI_NUMBER + CONFIG_ETHERNET_OPENETH_SUPPORT, sizeof(esp_eth_handle_t));
    ESP_GOTO_ON_FALSE(eth_handles != NULL, ESP_ERR_NO_MEM, err, TAG, "no memory");

#if CONFIG_ETHERNET_INTERNAL_SUPPORT
    eth_handles[eth_cnt_g] = eth_init_internal(eth_instance_g[eth_cnt_g].dev_info.name);
    ESP_GOTO_ON_FALSE(eth_handles[eth_cnt_g], ESP_FAIL, err, TAG, "internal Ethernet init failed");
    eth_instance_g[eth_cnt_g].state = DEV_STATE_INITIALIZED;
    eth_instance_g[eth_cnt_g].eth_handle = eth_handles[eth_cnt_g];
    eth_instance_g[eth_cnt_g].dev_info.type = ETH_DEV_TYPE_INTERNAL_ETH;
    eth_instance_g[eth_cnt_g].dev_info.pin.eth_internal_mdc = CONFIG_ETHERNET_MDC_GPIO;
    eth_instance_g[eth_cnt_g].dev_info.pin.eth_internal_mdio = CONFIG_ETHERNET_MDIO_GPIO;
    eth_cnt_g++;
#endif //CONFIG_ETHERNET_INTERNAL_SUPPORT

#if CONFIG_ETHERNET_SPI_SUPPORT
    ESP_GOTO_ON_ERROR(spi_bus_init(), err, TAG, "SPI bus init failed");
    // Init specific SPI Ethernet module configuration from Kconfig (CS GPIO, Interrupt GPIO, etc.)
    spi_eth_module_config_t spi_eth_module_config[ETHERNET_SPI_NUMBER] = { 0 };

    /*
    The SPI Ethernet module(s) might not have a burned factory MAC address, hence use manually configured address(es).
    In this component, a locally administered MAC address derived from ESP32x base MAC address is used or
    the MAC address is configured via Kconfig.
    Note: The locally administered OUI range should be used only when testing on a LAN under your control!
    */

    uint8_t local_mac_0[ETH_ADDR_LEN];
#if CONFIG_ETHERNET_SPI_AUTOCONFIG_MAC_ADDR0 || CONFIG_ETHERNET_SPI_AUTOCONFIG_MAC_ADDR1
    uint8_t base_eth_mac_addr[ETH_ADDR_LEN];
    ESP_GOTO_ON_ERROR(esp_read_mac(base_eth_mac_addr, ESP_MAC_ETH), err, TAG, "get ETH MAC failed");
#endif

#if CONFIG_ETHERNET_SPI_AUTOCONFIG_MAC_ADDR0
    esp_derive_local_mac(local_mac_0, base_eth_mac_addr);
#else
    sscanf(CONFIG_ETHERNET_SPI_MAC_ADDR0, "%2x:%2x:%2x:%2x:%2x:%2x", (unsigned int *) & (local_mac_0[0]),
           (unsigned int *)&local_mac_0[1],
           (unsigned int *)&local_mac_0[2],
           (unsigned int *)&local_mac_0[3],
           (unsigned int *)&local_mac_0[4],
           (unsigned int *)&local_mac_0[5]);
    ESP_GOTO_ON_ERROR(esp_iface_mac_addr_set(local_mac_0, ESP_MAC_ETH), err, TAG, "set ETH MAC failed");
#endif
    INIT_SPI_ETH_MODULE_CONFIG(spi_eth_module_config, 0);
    spi_eth_module_config[0].mac_addr = local_mac_0;

#if ETHERNET_SPI_NUMBER > 1
    uint8_t local_mac_1[ETH_ADDR_LEN];
#if CONFIG_ETHERNET_SPI_AUTOCONFIG_MAC_ADDR1
    base_eth_mac_addr[ETH_ADDR_LEN - 1] += 1;
    esp_derive_local_mac(local_mac_1, base_eth_mac_addr);
#else
    sscanf(CONFIG_ETHERNET_SPI_MAC_ADDR1, "%2x:%2x:%2x:%2x:%2x:%2x", (unsigned int *) & (local_mac_1[0]),
           (unsigned int *)&local_mac_1[1],
           (unsigned int *)&local_mac_1[2],
           (unsigned int *)&local_mac_1[3],
           (unsigned int *)&local_mac_1[4],
           (unsigned int *)&local_mac_1[5]);
#endif
    INIT_SPI_ETH_MODULE_CONFIG(spi_eth_module_config, 1);
    spi_eth_module_config[1].mac_addr = local_mac_1;
#endif

#if ETHERNET_SPI_NUMBER > 2
#error Maximum number of supported SPI Ethernet devices is currently limited to 2 by this example.
#endif

    for (int i = 0; i < ETHERNET_SPI_NUMBER; i++) {
        eth_handles[eth_cnt_g] = eth_init_spi(&spi_eth_module_config[i], eth_instance_g[eth_cnt_g].dev_info.name);
        ESP_GOTO_ON_FALSE(eth_handles[eth_cnt_g], ESP_FAIL, err, TAG, "SPI Ethernet init failed");
        eth_instance_g[eth_cnt_g].state = DEV_STATE_INITIALIZED;
        eth_instance_g[eth_cnt_g].eth_handle = eth_handles[eth_cnt_g];
        eth_instance_g[eth_cnt_g].dev_info.type = ETH_DEV_TYPE_SPI;
        eth_instance_g[eth_cnt_g].dev_info.pin.eth_spi_cs = spi_eth_module_config[i].spi_cs_gpio;
        eth_instance_g[eth_cnt_g].dev_info.pin.eth_spi_int = spi_eth_module_config[i].int_gpio;
        eth_cnt_g++;
    }
#if CONFIG_ETHERNET_ENC28J60_DUPLEX_FULL
    for (int i = 0; i < eth_cnt_g; i++) {
        if (strcmp(eth_instance_g[i].dev_info.name, "ENC28J60") == 0) {
            // It is recommended to use ENC28J60 in Full Duplex mode since multiple errata exist to the Half Duplex mode
            eth_duplex_t duplex = ETH_DUPLEX_FULL;
            ESP_GOTO_ON_ERROR(esp_eth_ioctl(eth_instance_g[i].eth_handle, ETH_CMD_S_DUPLEX_MODE, &duplex),
                              err, TAG, "failed to set duplex mode");
        }
    }
#endif // CONFIG_ETHERNET_ENC28J60_DUPLEX_FULL
#endif // CONFIG_ETHERNET_SPI_SUPPORT

#if CONFIG_ETHERNET_OPENETH_SUPPORT
    eth_handles[eth_cnt_g] = eth_init_openeth(eth_instance_g[eth_cnt_g].dev_info.name);
    ESP_GOTO_ON_FALSE(eth_handles[eth_cnt_g], ESP_FAIL, err, TAG, "OpenCores Ethernet init failed");
    eth_instance_g[eth_cnt_g].state = DEV_STATE_INITIALIZED;
    eth_instance_g[eth_cnt_g].eth_handle = eth_handles[eth_cnt_g];
    eth_instance_g[eth_cnt_g].dev_info.type = ETH_DEV_TYPE_OPENETH;
    eth_cnt_g++;
#endif // CONFIG_ETHERNET_OPENETH_SUPPORT
#if CONFIG_ETHERNET_DEFAULT_EVENT_HANDLER
    // Register Ethernet event handler
    if (eth_event_ctx_g == NULL) {
        ESP_GOTO_ON_ERROR(esp_event_handler_instance_register(ETH_EVENT, ESP_EVENT_ANY_ID, eth_event_handler, NULL, &eth_event_ctx_g),
                          err, TAG, "failed to register event handler instance");
    }
#endif // CONFIG_ETHERNET_DEFAULT_EVENT_HANDLER
#else
    ESP_LOGD(TAG, "no Ethernet device selected to init");
#endif // CONFIG_ETHERNET_INTERNAL_SUPPORT || CONFIG_ETHERNET_SPI_SUPPORT || CONFIG_ETHERNET_OPENETH_SUPPORT

#ifdef CONFIG_ETHERNET_USE_PLCA
    for (int i = 0; i < eth_cnt_g; i++) {
        if (strcmp(eth_instance_g[i].dev_info.name, "LAN867X") == 0 ||
                strcmp(eth_instance_g[i].dev_info.name, "LAN865X") == 0) {
            uint8_t plca_id = 0; // PLCA coordinator as default
#if CONFIG_ETHERNET_PLCA_COORDINATOR
            // Configure PLCA as coordinator
            uint8_t plca_nodes_count = CONFIG_ETHERNET_PLCA_NODE_COUNT;
            ESP_GOTO_ON_ERROR(esp_eth_ioctl(eth_instance_g[i].eth_handle, LAN86XX_ETH_CMD_S_PLCA_NCNT, &plca_nodes_count),
                              err, TAG, "failed to set PLCA node count");
            ESP_LOGI(TAG, "PLCA node count %" PRIu8, plca_nodes_count);
#elif CONFIG_ETHERNET_PLCA_FOLLOWER
            // Configure PLCA with node number from config
            plca_id = CONFIG_ETHERNET_PLCA_ID;
#endif
            ESP_GOTO_ON_ERROR(esp_eth_ioctl(eth_instance_g[i].eth_handle, LAN86XX_ETH_CMD_S_PLCA_ID, &plca_id),
                              err, TAG, "failed to set PLCA node ID");

            uint8_t plca_max_burst_count = CONFIG_ETHERNET_PLCA_BURST_COUNT;
            ESP_GOTO_ON_ERROR(esp_eth_ioctl(eth_instance_g[i].eth_handle, LAN86XX_ETH_CMD_S_MAX_BURST_COUNT, &plca_max_burst_count),
                              err, TAG, "failed to set PLCA max burst count");

#ifdef CONFIG_ETHERNET_PLCA_BURST_TIMER
            uint8_t plca_burst_timer = CONFIG_ETHERNET_PLCA_BURST_TIMER;
            ESP_GOTO_ON_ERROR(esp_eth_ioctl(eth_instance_g[i].eth_handle, LAN86XX_ETH_CMD_S_BURST_TIMER, &plca_burst_timer),
                              err, TAG, "failed to set PLCA max burst timer");
#endif

#ifdef CONFIG_ETHERNET_PLCA_MULTI_IDS_EN
            const char *start = CONFIG_ETHERNET_PLCA_MULTI_IDS;
            for (int id_cnt = 0; id_cnt < 8; id_cnt++) {
                char *end;
                long multi_id = strtol(start, &end, 10);
                if (start == end) {
                    break;
                }
                start = end;
                if (multi_id <= 0 || multi_id >= 0xFF) {
                    ESP_LOGE(TAG, "Invalid PLCA additional local ID: %li", multi_id);
                } else {
                    ESP_GOTO_ON_ERROR(esp_eth_ioctl(eth_instance_g[i].eth_handle, LAN86XX_ETH_CMD_ADD_TX_OPPORTUNITY, &multi_id),
                                      err, TAG, "failed to add additional local ID (%li)", multi_id);
                    ESP_LOGI(TAG, "PLCA additional local ID: %li", multi_id);
                }
            }
#endif
            // it is recommended to be Transmit Opportunity Timer always configured to desired value
            uint8_t plca_tot = CONFIG_ETHERNET_PLCA_TOT;
            ESP_GOTO_ON_ERROR(esp_eth_ioctl(eth_instance_g[i].eth_handle, LAN86XX_ETH_CMD_S_PLCA_TOT, &plca_tot),
                              err, TAG, "failed to set PLCA Transmit Opportunity timer");

            bool plca_en = true;
            ESP_GOTO_ON_ERROR(esp_eth_ioctl(eth_instance_g[i].eth_handle, LAN86XX_ETH_CMD_S_EN_PLCA, &plca_en),
                              err, TAG, "failed to enable PLCA");
            ESP_LOGI(TAG, "PLCA enabled, node ID: %" PRIu8, plca_id);
        }
    }
#endif // CONFIG_ETHERNET_USE_PLCA

    *eth_handles_out = eth_handles;
    *eth_cnt_out = eth_cnt_g;

    return ret;
#if CONFIG_ETHERNET_INTERNAL_SUPPORT || CONFIG_ETHERNET_SPI_SUPPORT || CONFIG_ETHERNET_OPENETH_SUPPORT
err:
    ethernet_deinit_all(eth_handles);
    return ret;
#endif
}

esp_err_t ethernet_deinit_all(esp_eth_handle_t *eth_handles)
{
#if CONFIG_ETHERNET_INTERNAL_SUPPORT || CONFIG_ETHERNET_SPI_SUPPORT || CONFIG_ETHERNET_OPENETH_SUPPORT
    if (eth_handles == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t deinit_cnt = 0;
    for (int eth_cnt = 0; eth_cnt < eth_cnt_g; eth_cnt++) {
        if ((eth_instance_g[eth_cnt].state == DEV_STATE_INITIALIZED) &&
                (eth_instance_g[eth_cnt].eth_handle != NULL)) {
            esp_eth_mac_t *mac = NULL;
            esp_eth_phy_t *phy = NULL;
            esp_eth_get_mac_instance(eth_instance_g[eth_cnt].eth_handle, &mac);
            esp_eth_get_phy_instance(eth_instance_g[eth_cnt].eth_handle, &phy);
            if (esp_eth_driver_uninstall(eth_instance_g[eth_cnt].eth_handle) == ESP_OK) {
                if (mac != NULL) {
                    mac->del(mac);
                }
                if (phy != NULL) {
                    phy->del(phy);
                }
                eth_instance_g[eth_cnt].state = DEV_STATE_UNINITIALIZED;
                eth_instance_g[eth_cnt].eth_handle = NULL;
                deinit_cnt++;
            } else {
                ESP_LOGE(TAG, "Unable to deinitialize ethernet handle: %p, if#: %" PRIu8,
                         eth_instance_g[eth_cnt].eth_handle, eth_cnt);
            }
        }
    }
    // continue only if all Ethernet devices were deinitialized
    if (deinit_cnt != eth_cnt_g) {
        return ESP_FAIL;
    }
#if CONFIG_ETHERNET_DEFAULT_EVENT_HANDLER
    if (eth_event_ctx_g != NULL) {
        esp_event_handler_instance_unregister(ETH_EVENT, ESP_EVENT_ANY_ID, eth_event_ctx_g);
        eth_event_ctx_g = NULL;
    }
#endif // CONFIG_ETHERNET_DEFAULT_EVENT_HANDLER
#if CONFIG_ETHERNET_SPI_SUPPORT
    if (spi_bus_deinit_g) {
        spi_bus_free(CONFIG_ETHERNET_SPI_HOST);
        spi_bus_deinit_g = false;
    }
    gpio_uninstall_isr_service();
#endif // CONFIG_ETHERNET_SPI_SUPPORT
    free(eth_handles);
    eth_cnt_g = 0;
    ESP_LOGI(TAG, "All Ethernet devices were deinitialized");
#else
    ESP_LOGD(TAG, "no Ethernet device was selected to init");
    return ESP_ERR_INVALID_STATE;
#endif // CONFIG_ETHERNET_INTERNAL_SUPPORT || CONFIG_ETHERNET_SPI_SUPPORT || CONFIG_ETHERNET_OPENETH_SUPPORT
    return ESP_OK;
}

eth_dev_info_t ethernet_init_get_dev_info(esp_eth_handle_t eth_handle)
{
    eth_dev_info_t ret = {.type = ETH_DEV_TYPE_UNKNOWN};
#if CONFIG_ETHERNET_INTERNAL_SUPPORT || CONFIG_ETHERNET_SPI_SUPPORT || CONFIG_ETHERNET_OPENETH_SUPPORT
    for (int i = 0; i < eth_cnt_g; i++) {
        if (eth_handle == eth_instance_g[i].eth_handle) {
            return eth_instance_g[i].dev_info;
        }
    }
#endif // CONFIG_ETHERNET_INTERNAL_SUPPORT || CONFIG_ETHERNET_SPI_SUPPORT || CONFIG_ETHERNET_OPENETH_SUPPORT
    return ret;
}
