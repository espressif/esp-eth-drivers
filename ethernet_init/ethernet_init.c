/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
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
#if CONFIG_ETH_USE_SPI_ETHERNET
#include "driver/spi_master.h"
#endif // CONFIG_ETH_USE_SPI_ETHERNET

#if CONFIG_ETHERNET_PHY_LAN867X
#include "esp_eth_phy_lan867x.h"
#endif // CONFIG_ETHERNET_PHY_LAN867X

#if CONFIG_ETHERNET_SPI_USE_CH390
#include "esp_eth_mac_ch390.h"
#include "esp_eth_phy_ch390.h"
#endif // CONFIG_ETHERNET_USE_CH390

#if CONFIG_ETHERNET_SPI_USE_ENC28J60
#include "esp_eth_enc28j60.h"
#endif // CONFIG_ETHERNET_USE_ENC28J60

#if CONFIG_ETHERNET_SPI_USE_LAN865X
#include "esp_eth_mac_lan865x.h"
#include "esp_eth_phy_lan865x.h"
#endif // CONFIG_ETHERNET_PHY_LAN865X

#if CONFIG_ETHERNET_SPI_NUMBER
#define SPI_ETHERNETS_NUM           CONFIG_ETHERNET_SPI_NUMBER
#else
#define SPI_ETHERNETS_NUM           0
#endif

#if CONFIG_ETHERNET_INTERNAL_SUPPORT
#define INTERNAL_ETHERNETS_NUM      1
#else
#define INTERNAL_ETHERNETS_NUM      0
#endif

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

#if !defined(CONFIG_ETHERNET_SPI_NUMBER)
#define CONFIG_ETHERNET_SPI_NUMBER 0
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
    esp_eth_mac_t *mac;
    esp_eth_phy_t *phy;
    dev_state state;
    eth_dev_info_t dev_info;
} eth_device;

static const char *TAG = "ethernet_init";
static uint8_t eth_cnt_g = 0;
static eth_device eth_instance_g[CONFIG_ETHERNET_INTERNAL_SUPPORT + CONFIG_ETHERNET_SPI_NUMBER];

static void eth_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
    uint8_t pin1 = 0, pin2 = 0;
    uint8_t mac_addr[ETH_ADDR_LEN] = {0};
    // we can get the ethernet driver handle from event data
    esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;
    eth_dev_info_t dev_info = ethernet_init_get_dev_info(eth_handle);

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

#if CONFIG_ETHERNET_INTERNAL_SUPPORT
/**
 * @brief Internal ESP32 Ethernet initialization
 *
 * @param[out] dev_out device information of the ethernet
 * @return
 *          - esp_eth_handle_t if init succeeded
 *          - NULL if init failed
 */
static esp_eth_handle_t eth_init_internal(eth_device *dev_out)
{
    esp_eth_handle_t ret = NULL;

    if (dev_out == NULL) {
        ESP_LOGE(TAG, "eth_device NULL");
        return ret;
    }

    // Init common MAC configs to default
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    mac_config.rx_task_stack_size = CONFIG_ETHERNET_RX_TASK_STACK_SIZE;

    // Init vendor specific MAC config to default
    eth_esp32_emac_config_t esp32_emac_config = ETH_ESP32_EMAC_DEFAULT_CONFIG();
    // Update vendor specific MAC config based on board configuration
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
    esp32_emac_config.smi_gpio.mdc_num = CONFIG_ETHERNET_MDC_GPIO;
    esp32_emac_config.smi_gpio.mdio_num = CONFIG_ETHERNET_MDIO_GPIO;
#else
    esp32_emac_config.smi_mdc_gpio_num = CONFIG_ETHERNET_MDC_GPIO;
    esp32_emac_config.smi_mdio_gpio_num = CONFIG_ETHERNET_MDIO_GPIO;
#endif

#if CONFIG_ETHERNET_SPI_SUPPORT
    // The DMA is shared resource between EMAC and the SPI. Therefore, adjust
    // EMAC DMA burst length when SPI Ethernet is used along with EMAC.
    esp32_emac_config.dma_burst_len = ETH_DMA_BURST_LEN_4;
#endif // CONFIG_ETHERNET_SPI_SUPPORT

    // Create new ESP32 Ethernet MAC instance
    dev_out->mac = esp_eth_mac_new_esp32(&esp32_emac_config, &mac_config);

    // Init common PHY configs to default
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();

    // Update PHY config based on board specific configuration
    phy_config.phy_addr = CONFIG_ETHERNET_PHY_ADDR;
    phy_config.reset_gpio_num = CONFIG_ETHERNET_PHY_RST_GPIO;

    // Create new PHY instance based on board configuration
#if CONFIG_ETHERNET_PHY_GENERIC
    dev_out->phy = esp_eth_phy_new_generic(&phy_config);
#elif CONFIG_ETHERNET_PHY_IP101
    dev_out->phy = esp_eth_phy_new_ip101(&phy_config);
    sprintf(dev_out->dev_info.name, "IP101");
#elif CONFIG_ETHERNET_PHY_RTL8201
    dev_out->phy = esp_eth_phy_new_rtl8201(&phy_config);
    sprintf(dev_out->dev_info.name, "RTL8201");
#elif CONFIG_ETHERNET_PHY_LAN87XX
    dev_out->phy = esp_eth_phy_new_lan87xx(&phy_config);
    sprintf(dev_out->dev_info.name, "LAN87XX");
#elif CONFIG_ETHERNET_PHY_DP83848
    dev_out->phy = esp_eth_phy_new_dp83848(&phy_config);
    sprintf(dev_out->dev_info.name, "DP83848");
#elif CONFIG_ETHERNET_PHY_KSZ80XX
    dev_out->phy = esp_eth_phy_new_ksz80xx(&phy_config);
    sprintf(dev_out->dev_info.name, "KSZ80XX");
#elif CONFIG_ETHERNET_PHY_LAN867X
    dev_out->phy = esp_eth_phy_new_lan867x(&phy_config);
    sprintf(dev_out->dev_info.name, "LAN867X");
#endif

    // Init Ethernet driver to default and install it
    esp_eth_handle_t eth_handle = NULL;
    esp_eth_config_t config = ETH_DEFAULT_CONFIG(dev_out->mac, dev_out->phy);
    config.on_lowlevel_init_done = eth_board_specific_init;
    ESP_GOTO_ON_FALSE(esp_eth_driver_install(&config, &eth_handle) == ESP_OK, NULL,
                      err, TAG, "Ethernet driver install failed");

    return eth_handle;
err:
    if (eth_handle != NULL) {
        esp_eth_driver_uninstall(eth_handle);
    }
    if (dev_out->mac != NULL) {
        dev_out->mac->del(dev_out->mac);
    }
    if (dev_out->phy != NULL) {
        dev_out->phy->del(dev_out->phy);
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
            ESP_LOGW(TAG, "GPIO ISR handler has been already installed");
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
    ESP_GOTO_ON_ERROR(spi_bus_initialize(CONFIG_ETHERNET_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO),
                      err, TAG, "SPI host #%d init failed", CONFIG_ETHERNET_SPI_HOST);

err:
    return ret;
}

/**
 * @brief Ethernet SPI modules initialization
 *
 * @param[in] spi_eth_module_config specific SPI Ethernet module configuration
 * @param[out] dev device information of the ethernet
 * @return
 *          - esp_eth_handle_t if init succeeded
 *          - NULL if init failed
 */
static esp_eth_handle_t eth_init_spi(spi_eth_module_config_t *spi_eth_module_config, eth_device *dev_out)
{
    esp_eth_handle_t ret = NULL;

    if (dev_out == NULL) {
        ESP_LOGE(TAG, "eth_device NULL");
        return ret;
    }

    dev_out->mac = NULL;
    dev_out->phy = NULL;

    // Init common MAC and PHY configs to default
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    mac_config.rx_task_stack_size = CONFIG_ETHERNET_RX_TASK_STACK_SIZE;
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
        dev_out->mac = esp_eth_mac_new_ksz8851snl(&ksz8851snl_config, &mac_config);
        dev_out->phy = esp_eth_phy_new_ksz8851snl(&phy_config);
        sprintf(dev_out->dev_info.name, "KSZ8851SNL");
#endif // CONFIG_ETHERNET_SPI_USE_KSZ8851SNL
    } else if (spi_eth_module_config->dev == SPI_DEV_TYPE_DM9051) {
#if CONFIG_ETHERNET_SPI_USE_DM9051
        eth_dm9051_config_t dm9051_config = ETH_DM9051_DEFAULT_CONFIG(CONFIG_ETHERNET_SPI_HOST, &spi_devcfg);
        dm9051_config.int_gpio_num = spi_eth_module_config->int_gpio;
        dm9051_config.poll_period_ms = spi_eth_module_config->poll_period_ms;
        dev_out->mac = esp_eth_mac_new_dm9051(&dm9051_config, &mac_config);
        dev_out->phy = esp_eth_phy_new_dm9051(&phy_config);
        sprintf(dev_out->dev_info.name, "DM9051");
#endif // CONFIG_ETHERNET_SPI_USE_DM9051
    } else if (spi_eth_module_config->dev == SPI_DEV_TYPE_W5500) {
#if CONFIG_ETHERNET_SPI_USE_W5500
        eth_w5500_config_t w5500_config = ETH_W5500_DEFAULT_CONFIG(CONFIG_ETHERNET_SPI_HOST, &spi_devcfg);
        w5500_config.int_gpio_num = spi_eth_module_config->int_gpio;
        w5500_config.poll_period_ms = spi_eth_module_config->poll_period_ms;
        dev_out->mac = esp_eth_mac_new_w5500(&w5500_config, &mac_config);
        dev_out->phy = esp_eth_phy_new_w5500(&phy_config);
        sprintf(dev_out->dev_info.name, "W5500");
#endif // CONFIG_ETHERNET_SPI_USE_W5500
    } else if (spi_eth_module_config->dev == SPI_DEV_TYPE_CH390) {
#if CONFIG_ETHERNET_SPI_USE_CH390
        eth_ch390_config_t ch390_config = ETH_CH390_DEFAULT_CONFIG(CONFIG_ETHERNET_SPI_HOST, &spi_devcfg);
        ch390_config.int_gpio_num = spi_eth_module_config->int_gpio;
        ch390_config.poll_period_ms = spi_eth_module_config->poll_period_ms;
        dev_out->mac = esp_eth_mac_new_ch390(&ch390_config, &mac_config);
        dev_out->phy = esp_eth_phy_new_ch390(&phy_config);
        sprintf(dev_out->dev_info.name, "CH390");
#endif // CONFIG_ETHERNET_SPI_USE_CH390
    } else if (spi_eth_module_config->dev == SPI_DEV_TYPE_ENC28J60) {
#if CONFIG_ETHERNET_SPI_USE_ENC28J60
        spi_devcfg.cs_ena_posttrans = enc28j60_cal_spi_cs_hold_time(CONFIG_ETHERNET_SPI_CLOCK_MHZ);
        eth_enc28j60_config_t enc28j60_config = ETH_ENC28J60_DEFAULT_CONFIG(CONFIG_ETHERNET_SPI_HOST, &spi_devcfg);
        enc28j60_config.int_gpio_num = spi_eth_module_config->int_gpio;
        dev_out->mac = esp_eth_mac_new_enc28j60(&enc28j60_config, &mac_config);

        // ENC28J60 Errata #1 check
        ESP_GOTO_ON_FALSE(dev_out->mac, NULL, err, TAG, "creation of ENC28J60 MAC instance failed");
        ESP_GOTO_ON_FALSE(emac_enc28j60_get_chip_info(dev_out->mac) >= ENC28J60_REV_B5 || CONFIG_ETHERNET_SPI_CLOCK_MHZ >= 8,
                          NULL, err, TAG, "SPI frequency must be at least 8 MHz for chip revision less than 5");

        phy_config.autonego_timeout_ms = 0; // ENC28J60 doesn't support auto-negotiation
        phy_config.reset_gpio_num = -1; // ENC28J60 doesn't have a pin to reset internal PHY
        dev_out->phy = esp_eth_phy_new_enc28j60(&phy_config);
        sprintf(dev_out->dev_info.name, "ENC28J60");
#endif // CONFIG_ETHERNET_SPI_USE_ENC28J60
    } else if (spi_eth_module_config->dev == SPI_DEV_TYPE_LAN865X) {
#if CONFIG_ETHERNET_SPI_USE_LAN865X
        eth_lan865x_config_t lan865x_config = ETH_LAN865X_DEFAULT_CONFIG(CONFIG_ETHERNET_SPI_HOST, &spi_devcfg);
        lan865x_config.int_gpio_num = spi_eth_module_config->int_gpio;
        lan865x_config.poll_period_ms = spi_eth_module_config->poll_period_ms;

        dev_out->mac = esp_eth_mac_new_lan865x(&lan865x_config, &mac_config);
        dev_out->phy = esp_eth_phy_new_lan865x(&phy_config);
        sprintf(dev_out->dev_info.name, "LAN865X");
#endif // CONFIG_ETHERNET_SPI_USE_LAN865X
    } else {
        ESP_LOGE(TAG, "Unsupported SPI Ethernet module type ID: %i", spi_eth_module_config->dev);
        goto err;
    }

    // Init Ethernet driver to default and install it
    esp_eth_handle_t eth_handle = NULL;
    esp_eth_config_t eth_config_spi = ETH_DEFAULT_CONFIG(dev_out->mac, dev_out->phy);
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
    if (dev_out->mac != NULL) {
        dev_out->mac->del(dev_out->mac);
    }
    if (dev_out->phy != NULL) {
        dev_out->phy->del(dev_out->phy);
    }
    return ret;
}
#endif // CONFIG_ETHERNET_SPI_SUPPORT

esp_err_t ethernet_init_all(esp_eth_handle_t *eth_handles_out[], uint8_t *eth_cnt_out)
{
    esp_err_t ret = ESP_OK;
    esp_eth_handle_t *eth_handles = NULL;
    static int called = 0;

    if (called == 0) {
        ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL));
        called = 1;
    }

#if CONFIG_ETHERNET_INTERNAL_SUPPORT || CONFIG_ETHERNET_SPI_SUPPORT
    ESP_GOTO_ON_FALSE(eth_handles_out != NULL && eth_cnt_out != NULL, ESP_ERR_INVALID_ARG,
                      err, TAG, "invalid arguments: initialized handles array or number of interfaces");
    eth_handles = calloc(SPI_ETHERNETS_NUM + INTERNAL_ETHERNETS_NUM, sizeof(esp_eth_handle_t));
    ESP_GOTO_ON_FALSE(eth_handles != NULL, ESP_ERR_NO_MEM, err, TAG, "no memory");

#if CONFIG_ETHERNET_INTERNAL_SUPPORT
    eth_handles[eth_cnt_g] = eth_init_internal(&eth_instance_g[eth_cnt_g]);
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
    spi_eth_module_config_t spi_eth_module_config[CONFIG_ETHERNET_SPI_NUMBER] = { 0 };

    /*
    The SPI Ethernet module(s) might not have a burned factory MAC address, hence use manually configured address(es).
    In this component, a locally administered MAC address derived from ESP32x base MAC address is used or
    the MAC address is configured via Kconfig.
    Note: The locally administered OUI range should be used only when testing on a LAN under your control!
    */

    uint8_t local_mac_0[ETH_ADDR_LEN];
#if CONFIG_ETHERNET_SPI_AUTOCONFIG_MAC_ADDR0 || CONFIG_ETHERNET_SPI_AUTOCONFIG_MAC_ADDR1
    uint8_t base_mac_addr[ETH_ADDR_LEN];
    ESP_GOTO_ON_ERROR(esp_efuse_mac_get_default(base_mac_addr), err, TAG, "get EFUSE MAC failed");
#endif

#if CONFIG_ETHERNET_SPI_AUTOCONFIG_MAC_ADDR0
    esp_derive_local_mac(local_mac_0, base_mac_addr);
#else
    sscanf(CONFIG_ETHERNET_SPI_MAC_ADDR0, "%2x:%2x:%2x:%2x:%2x:%2x", (unsigned int *) & (local_mac_0[0]),
           (unsigned int *)&local_mac_0[1],
           (unsigned int *)&local_mac_0[2],
           (unsigned int *)&local_mac_0[3],
           (unsigned int *)&local_mac_0[4],
           (unsigned int *)&local_mac_0[5]);
#endif
    INIT_SPI_ETH_MODULE_CONFIG(spi_eth_module_config, 0);
    spi_eth_module_config[0].mac_addr = local_mac_0;

#if CONFIG_ETHERNET_SPI_NUMBER > 1
    uint8_t local_mac_1[ETH_ADDR_LEN];
#if CONFIG_ETHERNET_SPI_AUTOCONFIG_MAC_ADDR1
    base_mac_addr[ETH_ADDR_LEN - 1] += 1;
    esp_derive_local_mac(local_mac_1, base_mac_addr);
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

#if CONFIG_ETHERNET_SPI_NUMBER > 2
#error Maximum number of supported SPI Ethernet devices is currently limited to 2 by this example.
#endif

    for (int i = 0; i < CONFIG_ETHERNET_SPI_NUMBER; i++) {
        eth_handles[eth_cnt_g] = eth_init_spi(&spi_eth_module_config[i], &eth_instance_g[eth_cnt_g]);
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
            ESP_ERROR_CHECK(esp_eth_ioctl(eth_instance_g[i].eth_handle, ETH_CMD_S_DUPLEX_MODE, &duplex));
        }
    }
#endif // CONFIG_ETHERNET_ENC28J60_DUPLEX_FULL
#endif // CONFIG_ETHERNET_SPI_SUPPORT

#else
    ESP_LOGD(TAG, "no Ethernet device selected to init");
#endif // CONFIG_ETHERNET_INTERNAL_SUPPORT || CONFIG_ETHERNET_SPI_SUPPORT

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
#if CONFIG_ETHERNET_INTERNAL_SUPPORT || CONFIG_ETHERNET_SPI_SUPPORT
err:
    ethernet_deinit_all(eth_handles);
    return ret;
#endif
}

void ethernet_deinit_all(esp_eth_handle_t *eth_handles)
{
    while (eth_cnt_g) {
        eth_cnt_g--;
        if ((eth_instance_g[eth_cnt_g].state == DEV_STATE_INITIALIZED) &&
                (eth_instance_g[eth_cnt_g].eth_handle != NULL)) {
            if (esp_eth_driver_uninstall(eth_instance_g[eth_cnt_g].eth_handle) != ESP_OK) {
                ESP_LOGE(TAG, "Unable to deinitialize ethernet handle: %d", eth_cnt_g);
            }

            if (eth_instance_g[eth_cnt_g].mac != NULL) {
                eth_instance_g[eth_cnt_g].mac->del(eth_instance_g[eth_cnt_g].mac);
            }
            if (eth_instance_g[eth_cnt_g].phy != NULL) {
                eth_instance_g[eth_cnt_g].phy->del(eth_instance_g[eth_cnt_g].phy);
            }
        }
    }

#if CONFIG_ETHERNET_SPI_SUPPORT
    spi_bus_free(CONFIG_ETHERNET_SPI_HOST);
    gpio_uninstall_isr_service();
#endif

    free(eth_handles);
    if (eth_cnt_g != 0) {
        ESP_LOGE(TAG, "Something is very wrong. eth_cnt_g is not zero(%d).", eth_cnt_g);
    }
}

/**
 * @brief Returns the device type of the ethernet handle
 *
 * @param[out] eth_handles Initialized Ethernet driver handles
 * @return
 *          - eth_dev_info_t device information of the ethernet handle
 */
eth_dev_info_t ethernet_init_get_dev_info(esp_eth_handle_t *eth_handle)
{
    eth_dev_info_t ret = {.type = ETH_DEV_TYPE_UNKNOWN};

    for (int i = 0; i < eth_cnt_g; i++) {
        if (eth_handle == eth_instance_g[i].eth_handle) {
            return eth_instance_g[i].dev_info;
        }
    }

    return ret;
}
