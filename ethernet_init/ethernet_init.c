/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <unistd.h>
#include "ethernet_init.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_check.h"
#include "esp_mac.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#if CONFIG_ETH_USE_SPI_ETHERNET
#include "driver/spi_master.h"
#endif // CONFIG_ETH_USE_SPI_ETHERNET
#include "ethernet_init.h"


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

#define INIT_SPI_ETH_MODULE_CONFIG(eth_module_config, num)                                      \
    do {                                                                                        \
        eth_module_config[num].spi_cs_gpio = CONFIG_ETHERNET_SPI_CS ##num## _GPIO;           \
        eth_module_config[num].int_gpio = CONFIG_ETHERNET_SPI_INT ##num## _GPIO;             \
        eth_module_config[num].phy_reset_gpio = CONFIG_ETHERNET_SPI_PHY_RST ##num## _GPIO;   \
        eth_module_config[num].phy_addr = CONFIG_ETHERNET_SPI_PHY_ADDR ##num;                \
    } while(0)

#if !defined(CONFIG_ETHERNET_INTERNAL_SUPPORT)
#define CONFIG_ETHERNET_INTERNAL_SUPPORT 0
#endif

#if !defined(CONFIG_ETHERNET_SPI_NUMBER)
#define CONFIG_ETHERNET_SPI_NUMBER 0
#endif

typedef struct {
    uint8_t spi_cs_gpio;
    uint8_t int_gpio;
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


void eth_event_handler(void *arg, esp_event_base_t event_base,
                       int32_t event_id, void *event_data)
{
    uint8_t pin1 = 0, pin2 = 0;
    uint8_t mac_addr[6] = {0};
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

    // Init vendor specific MAC config to default
    eth_esp32_emac_config_t esp32_emac_config = ETH_ESP32_EMAC_DEFAULT_CONFIG();
    // Update vendor specific MAC config based on board configuration
    esp32_emac_config.smi_mdc_gpio_num = CONFIG_ETHERNET_MDC_GPIO;
    esp32_emac_config.smi_mdio_gpio_num = CONFIG_ETHERNET_MDIO_GPIO;

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
#if CONFIG_ETHERNET_PHY_IP101
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
#endif

    // Init Ethernet driver to default and install it
    esp_eth_handle_t eth_handle = NULL;
    esp_eth_config_t config = ETH_DEFAULT_CONFIG(dev_out->mac, dev_out->phy);
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

    // Init common MAC and PHY configs to default
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
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
#if CONFIG_ETHERNET_USE_KSZ8851SNL
    eth_ksz8851snl_config_t ksz8851snl_config = ETH_KSZ8851SNL_DEFAULT_CONFIG(CONFIG_ETHERNET_SPI_HOST, &spi_devcfg);
    ksz8851snl_config.int_gpio_num = spi_eth_module_config->int_gpio;
    dev->mac = esp_eth_mac_new_ksz8851snl(&ksz8851snl_config, &mac_config);
    dev->phy = esp_eth_phy_new_ksz8851snl(&phy_config);
    sprintf(dev->dev_info.name, "KSZ8851SNL");
#elif CONFIG_ETHERNET_USE_DM9051
    eth_dm9051_config_t dm9051_config = ETH_DM9051_DEFAULT_CONFIG(CONFIG_ETHERNET_SPI_HOST, &spi_devcfg);
    dm9051_config.int_gpio_num = spi_eth_module_config->int_gpio;
    dev->mac = esp_eth_mac_new_dm9051(&dm9051_config, &mac_config);
    dev->phy = esp_eth_phy_new_dm9051(&phy_config);
    sprintf(dev->dev_info.name, "DM9051");
#elif CONFIG_ETHERNET_USE_W5500
    eth_w5500_config_t w5500_config = ETH_W5500_DEFAULT_CONFIG(CONFIG_ETHERNET_SPI_HOST, &spi_devcfg);
    w5500_config.int_gpio_num = spi_eth_module_config->int_gpio;
    dev_out->mac = esp_eth_mac_new_w5500(&w5500_config, &mac_config);
    dev_out->phy = esp_eth_phy_new_w5500(&phy_config);
    sprintf(dev_out->dev_info.name, "W5500");
#endif //CONFIG_ETHERNET_USE_W5500
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
        eth_instance_g[eth_cnt_g].dev_info.pin.eth_spi_cs = CONFIG_ETHERNET_SPI_CS0_GPIO;
        eth_instance_g[eth_cnt_g].dev_info.pin.eth_spi_int = CONFIG_ETHERNET_SPI_INT0_GPIO;
        eth_cnt_g++;
    }
#endif // CONFIG_ETHERNET_SPI_SUPPORT

#else
    ESP_LOGD(TAG, "no Ethernet device selected to init");
#endif // CONFIG_ETHERNET_INTERNAL_SUPPORT || CONFIG_ETHERNET_SPI_SUPPORT
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