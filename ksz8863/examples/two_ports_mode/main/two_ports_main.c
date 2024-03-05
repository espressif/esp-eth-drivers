/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_event.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "esp_check.h"

#include "driver/gpio.h"
#include "esp_rom_gpio.h"
#include "esp_rom_sys.h"

// needed for L2 TAP VFS
#include <stdio.h>
#include <unistd.h> // read/write
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include "esp_vfs_l2tap.h"
#include "lwip/prot/ethernet.h" // Ethernet headers
#include "errno.h"
#include "arpa/inet.h" // for ntohs, etc.

#include "esp_eth_ksz8863.h"

typedef struct {
    struct eth_hdr header;
    union {
        int cnt;
        char str[44];
    };
} test_vfs_eth_tap_msg_t;

static const char *TAG = "ksz8863_eth_example";
static SemaphoreHandle_t init_done;

static void print_dyn_mac(void *pvParameters)
{
    esp_eth_handle_t port_eth_handle = (esp_eth_handle_t) pvParameters;
    ksz8863_dyn_mac_table_t dyn_mac_tbls[5];
    ksz8863_mac_tbl_info_t get_tbl_info = {
        .start_entry = 0,  // read from the first entry
        .etries_num = 5,   // read 5 entries
        .dyn_tbls = dyn_mac_tbls,
    };

    xSemaphoreGive(init_done);

    while (1) {
        esp_eth_ioctl(port_eth_handle, KSZ8863_ETH_CMD_G_MAC_DYN_TBL, &get_tbl_info);
        ESP_LOGI(TAG, "Dynamic MAC Table content:");
        ESP_LOGI(TAG, "valid entries %" PRIu16, dyn_mac_tbls[0].val_entries + 1);
        for (int i = 0; i < (dyn_mac_tbls[0].val_entries + 1) && i < 5; i++) {
            ESP_LOGI(TAG, "port %" PRIu8, dyn_mac_tbls[i].src_port + 1);
            ESP_LOG_BUFFER_HEX(TAG, dyn_mac_tbls[i].mac_addr, 6);
        }
        printf("\n");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

static void transmit_l2test_msgs(void *pvParameters)
{
    esp_vfs_l2tap_intf_register(NULL);
    int ret;
    int eth_tap_fd_p1 = -1;
    int eth_tap_fd_p2 = -1;

    eth_tap_fd_p1 = open("/dev/net/tap", O_NONBLOCK);
    if (eth_tap_fd_p1 < 0) {
        ESP_LOGE(TAG, "Unable to open P1 L2 TAP interface: errno %i", errno);
        goto err;
    }
    eth_tap_fd_p2 = open("/dev/net/tap", O_NONBLOCK);
    if (eth_tap_fd_p2 < 0) {
        ESP_LOGE(TAG, "Unable to open P2 L2 TAP interface: errno %i", errno);
        goto err;
    }
    uint16_t eth_type_filter = 0x7000;
    // Set Ethernet interface on which to get raw frames
    if ((ret = ioctl(eth_tap_fd_p1, L2TAP_S_INTF_DEVICE, "ETH_0")) == -1) {
        ESP_LOGE(TAG, "Unable to bound P1 L2 TAP with Ethernet device: errno %i", errno);
        goto err;
    }
    if ((ret = ioctl(eth_tap_fd_p2, L2TAP_S_INTF_DEVICE, "ETH_1")) == -1) {
        ESP_LOGE(TAG, "Unable to bound P2 L2 TAP with Ethernet device: errno %i", errno);
        goto err;
    }

    if ((ret = ioctl(eth_tap_fd_p1, L2TAP_S_RCV_FILTER, &eth_type_filter)) == -1) {
        ESP_LOGE(TAG, "Unable to configure P1 L2 TAP Ethernet type receive filter: errno %i", errno);
        goto err;
    }
    if ((ret = ioctl(eth_tap_fd_p2, L2TAP_S_RCV_FILTER, &eth_type_filter)) == -1) {
        ESP_LOGE(TAG, "Unable to configure P2 L2 TAP Ethernet type receive filter: errno %i", errno);
        goto err;
    }

    esp_eth_handle_t p1_eth_handle = esp_netif_get_io_driver(esp_netif_get_handle_from_ifkey("ETH_0"));
    esp_eth_handle_t p2_eth_handle = esp_netif_get_io_driver(esp_netif_get_handle_from_ifkey("ETH_1"));

    test_vfs_eth_tap_msg_t test_msg_p1 = {
        .header = {
            .src.addr = {0},
            .dest.addr = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }, // broadcast
            .type = ntohs(eth_type_filter),
        },
        .str = "This is ESP32 L2 TAP test msg from Port: 1"
    };

    test_vfs_eth_tap_msg_t test_msg_p2 = {
        .header = {
            .src.addr = {0},
            .dest.addr = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }, // broadcast
            .type = ntohs(eth_type_filter),
        },
        .str = "This is ESP32 L2 TAP test msg from Port: 2"
    };

    // Set source MAC address in test message
    if ((ret = esp_eth_ioctl(p1_eth_handle, ETH_CMD_G_MAC_ADDR, test_msg_p1.header.src.addr)) == -1) {
        ESP_LOGE(TAG, "get P1 MAC addr error");
    }
    if ((ret = esp_eth_ioctl(p2_eth_handle, ETH_CMD_G_MAC_ADDR, test_msg_p2.header.src.addr)) == -1) {
        ESP_LOGE(TAG, "get P2 MAC addr error");
    }

    while (1) {
        ret = write(eth_tap_fd_p1, &test_msg_p1, sizeof(test_msg_p1));
        if (ret == -1) {
            ESP_LOGE(TAG, "P1 L2 TAP write error, errno: %i\n", errno);
        }
        ret = write(eth_tap_fd_p2, &test_msg_p2, sizeof(test_msg_p2));
        if (ret == -1) {
            ESP_LOGE(TAG, "P2 L2 TAP write error, errno: %i\n", errno);
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
err:
    if (eth_tap_fd_p1 != -1) {
        close(eth_tap_fd_p1);
    }
    if (eth_tap_fd_p2 != -1) {
        close(eth_tap_fd_p2);
    }
    vTaskDelete(NULL);
}

/** Event handler for Ethernet events */
static void eth_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
    uint8_t mac_addr[6] = {0};
    int32_t port_num;
    esp_err_t ret;
    /* we can get the ethernet driver handle from event data */
    esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;

    switch (event_id) {
    case ETHERNET_EVENT_CONNECTED:
        esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
        ret = esp_eth_ioctl(eth_handle, KSZ8863_ETH_CMD_G_PORT_NUM, &port_num);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Ethernet Link Up Port %" PRIi32, port_num + 1);
        } else {
            ESP_LOGI(TAG, "Ethernet Link Up");
        }
        ESP_LOGI(TAG, "Ethernet HW Addr %02x:%02x:%02x:%02x:%02x:%02x",
                 mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
        break;
    case ETHERNET_EVENT_DISCONNECTED:
        ret = esp_eth_ioctl(eth_handle, KSZ8863_ETH_CMD_G_PORT_NUM, &port_num);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Ethernet Link Down Port %" PRIi32, port_num + 1);
        } else {
            ESP_LOGI(TAG, "Ethernet Link Down");
        }
        break;
    case ETHERNET_EVENT_START:
        ESP_LOGI(TAG, "Ethernet Started");
        break;
    case ETHERNET_EVENT_STOP:
        ESP_LOGI(TAG, "Ethernet Stopped");
        break;
    default:
        break;
    }
}

/** Event handler for IP_EVENT_ETH_GOT_IP */
static void got_ip_event_handler(void *arg, esp_event_base_t event_base,
                                 int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
    const esp_netif_ip_info_t *ip_info = &event->ip_info;

    ESP_LOGI(TAG, "Ethernet Got IP Address");
    ESP_LOGI(TAG, "~~~~~~~~~~~");
    ESP_LOGI(TAG, "ETHIP:" IPSTR, IP2STR(&ip_info->ip));
    ESP_LOGI(TAG, "ETHMASK:" IPSTR, IP2STR(&ip_info->netmask));
    ESP_LOGI(TAG, "ETHGW:" IPSTR, IP2STR(&ip_info->gw));
    ESP_LOGI(TAG, "~~~~~~~~~~~");
}

esp_err_t i2c_init(i2c_port_t i2c_master_port, i2c_config_t *i2c_conf)
{
    esp_err_t ret;

    ESP_GOTO_ON_ERROR(i2c_param_config(i2c_master_port, i2c_conf), err, TAG, "I2C parameters configuration failed");
    ESP_GOTO_ON_ERROR(i2c_driver_install(i2c_master_port, i2c_conf->mode, 0, 0, 0), err, TAG, "I2C driver install failed");

    return ESP_OK;
err:
    return ret;
}

// board specific initialization routine, user to update per specific needs
esp_err_t ksz8863_board_specific_init(esp_eth_handle_t eth_handle)
{
    esp_err_t ret = ESP_OK;

#if CONFIG_EXAMPLE_CTRL_I2C
    // initialize I2C interface
    i2c_config_t i2c_bus_config = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = CONFIG_EXAMPLE_I2C_SDA_GPIO,
        .scl_io_num = CONFIG_EXAMPLE_I2C_SCL_GPIO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = CONFIG_EXAMPLE_I2C_CLOCK_KHZ * 1000,
    };
    ESP_GOTO_ON_ERROR(i2c_init(CONFIG_EXAMPLE_I2C_MASTER_PORT, &i2c_bus_config), err, TAG, "I2C initialization failed");
    ksz8863_ctrl_i2c_config_t i2c_dev_config = {
        .dev_addr = KSZ8863_I2C_DEV_ADDR,
        .i2c_master_port = CONFIG_EXAMPLE_I2C_MASTER_PORT,
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
    ESP_ERROR_CHECK(spi_bus_initialize(CONFIG_EXAMPLE_ETH_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));

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
    // it does not make much sense to execute SW reset right after HW reset but it is present here for demonstration purposes
    ESP_GOTO_ON_ERROR(ksz8863_sw_reset(eth_handle), err, TAG, "software reset failed");
#if CONFIG_EXAMPLE_P3_RMII_CLKI_INTERNAL
    ESP_GOTO_ON_ERROR(ksz8863_p3_rmii_internal_clk(eth_handle, true), err, TAG, "P3 internal clk config failed");
#endif

#if CONFIG_EXAMPLE_P3_RMII_CLKI_INVERT
    ESP_GOTO_ON_ERROR(ksz8863_p3_rmii_clk_invert(eth_handle, true), err, TAG, "P3 invert ckl failed");
#endif
err:
    return ret;
}

void app_main(void)
{
    ESP_LOGW(TAG, "Two Port endpoints mode Example...\n");

    // Initialize TCP/IP network interface (should be called only once in application)
    ESP_ERROR_CHECK(esp_netif_init());
    // Create default event loop that running in background
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Init MAC and PHY configs to default
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();

    eth_esp32_emac_config_t esp32_emac_config = ETH_ESP32_EMAC_DEFAULT_CONFIG();

    phy_config.reset_gpio_num = -1; // KSZ8863 is reset by separate function call since multiple instances exist
    esp32_emac_config.smi_mdc_gpio_num = -1; // MIIM interface is not used since does not provide access to all registers
    esp32_emac_config.smi_mdio_gpio_num = -1;

    // Init Host Ethernet Interface (Port 3)
    esp_eth_mac_t *host_mac = esp_eth_mac_new_esp32(&esp32_emac_config, &mac_config);
    phy_config.phy_addr = -1; // this PHY is entry point to host
    esp_eth_phy_t *host_phy = esp_eth_phy_new_ksz8863(&phy_config);

    esp_eth_config_t host_config = ETH_KSZ8863_DEFAULT_CONFIG(host_mac, host_phy);
    host_config.on_lowlevel_init_done = ksz8863_board_specific_init;
    esp_eth_handle_t host_eth_handle = NULL;
    ESP_ERROR_CHECK(esp_eth_driver_install(&host_config, &host_eth_handle));

    // Init P1 Ethernet Interface
    ksz8863_eth_mac_config_t ksz8863_pmac_config = {
        .pmac_mode = KSZ8863_PORT_MODE,
        .port_num = KSZ8863_PORT_1,
    };
    esp_eth_mac_t *p1_mac = esp_eth_mac_new_ksz8863(&ksz8863_pmac_config, &mac_config);
    phy_config.phy_addr = KSZ8863_PORT_1;
    esp_eth_phy_t *p1_phy = esp_eth_phy_new_ksz8863(&phy_config);

    esp_eth_config_t p1_config = ETH_KSZ8863_DEFAULT_CONFIG(p1_mac, p1_phy);
    esp_eth_handle_t p1_eth_handle = NULL;
    ESP_ERROR_CHECK(esp_eth_driver_install(&p1_config, &p1_eth_handle));

    // Init P2 Ethernet Interface
    ksz8863_pmac_config.port_num = KSZ8863_PORT_2;
    esp_eth_mac_t *p2_mac = esp_eth_mac_new_ksz8863(&ksz8863_pmac_config, &mac_config);
    phy_config.phy_addr = KSZ8863_PORT_2;
    esp_eth_phy_t *p2_phy = esp_eth_phy_new_ksz8863(&phy_config);

    esp_eth_config_t p2_config = ETH_KSZ8863_DEFAULT_CONFIG(p2_mac, p2_phy);
    esp_eth_handle_t p2_eth_handle = NULL;
    ESP_ERROR_CHECK(esp_eth_driver_install(&p2_config, &p2_eth_handle));

    // KSZ8863 Ports 1/2 does not have any default MAC
    ESP_ERROR_CHECK(esp_eth_ioctl(p1_eth_handle, ETH_CMD_S_MAC_ADDR, (uint8_t[]) {
        0x8c, 0x4b, 0x14, 0x0a, 0x14, 0x00
    }));
    ESP_ERROR_CHECK(esp_eth_ioctl(p2_eth_handle, ETH_CMD_S_MAC_ADDR, (uint8_t[]) {
        0x8c, 0x4b, 0x14, 0x0a, 0x14, 0x01
    }));

    bool enable = true;
    // Internal EMAC needs to receive frames from other KSZ8863 ports => do not perform any filterring
    ESP_ERROR_CHECK(esp_eth_ioctl(host_eth_handle, ETH_CMD_S_PROMISCUOUS, &enable));

    // Register Ports to which forward traffic received by "host eth"
    ksz8863_register_tail_tag_port(p1_eth_handle, 0);
    ksz8863_register_tail_tag_port(p2_eth_handle, 1);
    // Make "host eth" decide where to forward traffic (i.e. to registered ports)
    ESP_ERROR_CHECK(esp_eth_update_input_path(host_eth_handle, ksz8863_eth_tail_tag_port_forward, NULL));
    // Register "host eth" so it could be used by Ports for transmit
    ESP_ERROR_CHECK(ksz8863_register_host_eth_hndl(host_eth_handle));

    // Create instance(s) of esp-netif for Port1 & Port 2 Ethernets
    esp_netif_inherent_config_t esp_netif_config = ESP_NETIF_INHERENT_DEFAULT_ETH();
    esp_netif_config_t netif_cfg = {
        .base = &esp_netif_config,
        .stack = ESP_NETIF_NETSTACK_DEFAULT_ETH
    };
    esp_netif_t *eth_netif_port[2] = { NULL };
    char if_key_str[10];
    char if_desc_str[10];
    char num_str[3];
    for (int i = 0; i < 2; i++) {
        itoa(i, num_str, 10);
        strcat(strcpy(if_key_str, "ETH_"), num_str);
        strcat(strcpy(if_desc_str, "eth"), num_str);
        esp_netif_config.if_key = if_key_str;
        esp_netif_config.if_desc = if_desc_str;
        esp_netif_config.route_prio = 30 - i;
        eth_netif_port[i] = esp_netif_new(&netif_cfg);
    }
    ESP_ERROR_CHECK(esp_netif_attach(eth_netif_port[0], esp_eth_new_netif_glue(p1_eth_handle)));
    ESP_ERROR_CHECK(esp_netif_attach(eth_netif_port[1], esp_eth_new_netif_glue(p2_eth_handle)));

    // Register user defined event handers
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_event_handler, NULL));

    // start Ethernet driver state machines
    ESP_ERROR_CHECK(esp_eth_start(host_eth_handle));
    ESP_ERROR_CHECK(esp_eth_start(p1_eth_handle));
    ESP_ERROR_CHECK(esp_eth_start(p2_eth_handle));

    // Sync semaphore is needed since main task local variables are used during initialization in other tasks
    init_done = xSemaphoreCreateBinary();
    assert(init_done);

    // Periodically print content of Dynamic MAC table
    xTaskCreate(print_dyn_mac, "print_dyn_mac", 4096, p1_eth_handle, 5, NULL);
    xSemaphoreTake(init_done, portMAX_DELAY);
    // Periodically send L2 test messages at each port
    xTaskCreate(transmit_l2test_msgs, "tx_test_msgs", 4096, NULL, 4, NULL);

    vSemaphoreDelete(init_done);

    // The below is just for demonstration purpose...
    // The first entry of static MAC table is not modifiable in Two Ports mode hence, it is interesting to see what is located here.
    // The first entry defines to forward all broadcast traffic to Host (P3) port and so ensure that P1 & P2 act as independent ports
    // even for broadcasted frames.
    ksz8863_sta_mac_table_t sta_mac_tbls[3];
    ksz8863_mac_tbl_info_t get_sta_tbl_info = {
        .start_entry = 0,
        .etries_num = 3,
        .sta_tbls = sta_mac_tbls,
    };

    esp_eth_ioctl(p1_eth_handle, KSZ8863_ETH_CMD_G_MAC_STA_TBL, &get_sta_tbl_info);
    ESP_LOGI(TAG, "static MAC table content:");
    for (uint32_t i = 0; i < 3; i++) {
        ESP_LOGI(TAG, "fwd port %" PRIu16, sta_mac_tbls[i].fwd_ports);
        ESP_LOGI(TAG, "valid %" PRIu16, sta_mac_tbls[i].entry_val);
        ESP_LOG_BUFFER_HEX(TAG, sta_mac_tbls[i].mac_addr, 6);
        printf("\n");
    }
}
