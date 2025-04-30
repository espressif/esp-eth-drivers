#include "test_init_ksz8863.h"
#include "esp_err.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_check.h"
#include "esp_eth_ksz8863.h"
#include "sdkconfig.h"
#include "driver/gpio.h"
#include "esp_rom_gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "test_init_ksz8863";

esp_err_t init_ksz8863_interface()
{
    esp_err_t ret = ESP_OK;
    // initialize I2C interface
#ifdef CONFIG_EXAMPLE_CTRL_I2C
    i2c_master_bus_config_t i2c_mst_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = CONFIG_EXAMPLE_I2C_MASTER_PORT,
        .scl_io_num = CONFIG_EXAMPLE_I2C_SCL_GPIO,
        .sda_io_num = CONFIG_EXAMPLE_I2C_SDA_GPIO,
        .glitch_ignore_cnt = 7,
        //.flags.enable_internal_pullup = true
    };
    i2c_master_bus_handle_t bus_handle;
    ESP_GOTO_ON_ERROR(i2c_new_master_bus(&i2c_mst_config, &bus_handle), err, TAG, "I2C initialization failed");
    ksz8863_ctrl_i2c_config_t i2c_dev_config = {
        .bus_handle = bus_handle,
        .dev_addr = KSZ8863_I2C_DEV_ADDR,
        .i2c_port = CONFIG_EXAMPLE_I2C_MASTER_PORT,
        .scl_speed_hz = CONFIG_EXAMPLE_I2C_CLOCK_KHZ * 1000
    };
    ksz8863_ctrl_intf_config_t ctrl_intf_cfg = {
        .host_mode = KSZ8863_I2C_MODE,
        .i2c_dev_config = &i2c_dev_config,
    };
#elif CONFIG_EXAMPLE_CTRL_SPI
    spi_bus_config_t buscfg = {
        .miso_io_num = CONFIG_EXAMPLE_ETH_SPI_MISO_GPIO,
        .mosi_io_num = CONFIG_EXAMPLE_ETH_SPI_MOSI_GPIO,
        .sclk_io_num = CONFIG_EXAMPLE_ETH_SPI_SCLK_GPIO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    ESP_GOTO_ON_ERROR(spi_bus_initialize(CONFIG_EXAMPLE_ETH_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO), err, TAG, "SPI initialization failed");

    ksz8863_ctrl_spi_config_t spi_dev_config = {
        .host_id = CONFIG_EXAMPLE_ETH_SPI_HOST,
        .clock_speed_hz = CONFIG_EXAMPLE_ETH_SPI_CLOCK_MHZ * 1000 * 1000,
        .spics_io_num = CONFIG_EXAMPLE_ETH_SPI_CS_GPIO,
    };
    ksz8863_ctrl_intf_config_t ctrl_intf_cfg = {
        .host_mode = KSZ8863_SPI_MODE,
        .spi_dev_config = &spi_dev_config,
    };
#endif
    ESP_GOTO_ON_ERROR(ksz8863_ctrl_intf_init(&ctrl_intf_cfg), err, TAG, "KSZ8863 control interface initialization failed");

#ifdef CONFIG_EXAMPLE_EXTERNAL_CLK_EN
    // Enable KSZ's external CLK
    esp_rom_gpio_pad_select_gpio(CONFIG_EXAMPLE_EXTERNAL_CLK_EN_GPIO);
    gpio_set_direction(CONFIG_EXAMPLE_EXTERNAL_CLK_EN_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(CONFIG_EXAMPLE_EXTERNAL_CLK_EN_GPIO, 1);
#endif
    ESP_GOTO_ON_ERROR(ksz8863_hw_reset(CONFIG_EXAMPLE_KSZ8863_RST_GPIO), err, TAG, "hardware reset failed");
err:
    return ret;
}

esp_err_t init_ksz8863_auto(esp_netif_t *eth_netif, esp_eth_handle_t *host_eth_handle_ptr, esp_eth_handle_t *p1_eth_handle_ptr, esp_eth_handle_t *p2_eth_handle_ptr)
{
    esp_err_t ret = ESP_OK;

    // Init MAC and PHY configs to default
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();

    eth_esp32_emac_config_t esp32_emac_config = ETH_ESP32_EMAC_DEFAULT_CONFIG();

    phy_config.reset_gpio_num = -1; // KSZ8863 is reset by separate function call since multiple instances exist
    // MIIM interface is not used since does not provide access to all registers
    esp32_emac_config.smi_gpio.mdc_num = -1;
    esp32_emac_config.smi_gpio.mdio_num = -1;

    // Init Host Ethernet Interface (Port 3)
    esp_eth_mac_t *host_mac = esp_eth_mac_new_esp32(&esp32_emac_config, &mac_config);
    phy_config.phy_addr = -1; // this PHY is entry point to host
    esp_eth_phy_t *host_phy = esp_eth_phy_new_ksz8863(&phy_config);

    esp_eth_config_t host_config = ETH_KSZ8863_DEFAULT_CONFIG(host_mac, host_phy);
    host_config.on_lowlevel_init_done = init_ksz8863_interface;
    ESP_GOTO_ON_ERROR(esp_eth_driver_install(&host_config, host_eth_handle_ptr), err, TAG, "Couldn't install the Ethernet driver");
    // *** SPI Ethernet modules deviation ***
    // Rationale: The SPI Ethernet modules don't have a burned default factory MAC address
#if CONFIG_TARGET_USE_SPI_ETHERNET
    uint8_t base_mac_addr[ETH_ADDR_LEN];
    TEST_ESP_OK(esp_efuse_mac_get_default(base_mac_addr));
    uint8_t local_mac[ETH_ADDR_LEN];
    TEST_ESP_OK(esp_derive_local_mac(local_mac, base_mac_addr));
    TEST_ESP_OK(esp_eth_ioctl(eth_handle, ETH_CMD_S_MAC_ADDR, local_mac));
#endif
    // Create new default instance of esp-netif for Host Ethernet Port (P3)
    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
    eth_netif = esp_netif_new(&cfg);
    ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(*host_eth_handle_ptr)));
    // p1/2_eth_handle are going to be used basically only for Link Status indication and for configuration access
    // Init P1 Ethernet Interface
    ksz8863_eth_mac_config_t ksz8863_pmac_config = {
        .pmac_mode = KSZ8863_SWITCH_MODE,
        .port_num = KSZ8863_PORT_1,
    };
    esp_eth_mac_t *p1_mac = esp_eth_mac_new_ksz8863(&ksz8863_pmac_config, &mac_config);
    phy_config.phy_addr = KSZ8863_PORT_1;
    esp_eth_phy_t *p1_phy = esp_eth_phy_new_ksz8863(&phy_config);

    esp_eth_config_t p1_config = ETH_KSZ8863_DEFAULT_CONFIG(p1_mac, p1_phy);
    ESP_GOTO_ON_ERROR(esp_eth_driver_install(&p1_config, p1_eth_handle_ptr), err, TAG, "Couldn't install the Ethernet driver for port #1");

    // Init P2 Ethernet Interface
    ksz8863_pmac_config.port_num = KSZ8863_PORT_2;
    esp_eth_mac_t *p2_mac = esp_eth_mac_new_ksz8863(&ksz8863_pmac_config, &mac_config);
    phy_config.phy_addr = KSZ8863_PORT_2;
    esp_eth_phy_t *p2_phy = esp_eth_phy_new_ksz8863(&phy_config);

    esp_eth_config_t p2_config = ETH_KSZ8863_DEFAULT_CONFIG(p2_mac, p2_phy);
    ESP_GOTO_ON_ERROR(esp_eth_driver_install(&p2_config, p2_eth_handle_ptr), err, TAG, "Couldn't install the Ethernet driver for port #2");
err:
    return ret;
}