# WIZnet W5500 Ethernet Driver

## Overview

W5500 is a hardwired TCP/IP embedded Ethernet controller that enables easier internet connection for embedded systems using SPI (Serial Peripheral Interface). The W5500 provides a complete and hardwired TCP/IP stack (not used in the ESP driver) and 10/100 Ethernet MAC and PHY. It supports 3.3V or 2.5V operation and has a built-in 32K bytes internal buffer for data transmission. More information about the chip can be found in the product [datasheets](https://wiznet.io/products/ethernet-chips/w5500).

## ESP-IDF Usage

Add this component from [IDF Component Manager](https://components.espressif.com/) to your project using `idf.py add-dependency` and include `esp_eth_phy_w5500.h` and `esp_eth_mac_w5500.h`,

```c
#include "esp_eth_phy_w5500.h"
#include "esp_eth_mac_w5500.h"
```

create a configuration instance by calling `ETH_W5500_DEFAULT_CONFIG`,

```c
// Configure SPI interface for specific SPI module
spi_device_interface_config_t spi_devcfg = {
    .mode = 0,
    .clock_speed_hz = CONFIG_TCPSERVER_ETH_SPI_CLOCK_MHZ * 1000 * 1000,
    .queue_size = 16,
    .spics_io_num = CONFIG_TCPSERVER_ETH_SPI_CS_GPIO
};

eth_w5500_config_t w5500_config = ETH_W5500_DEFAULT_CONFIG(CONFIG_TCPSERVER_ETH_SPI_HOST,&spi_devcfg);
w5500_config.int_gpio_num = CONFIG_TCPSERVER_ETH_SPI_INT_GPIO;

#if CONFIG_TCPSERVER_ETH_SPI_INT_GPIO < 0
w5500_config.poll_period_ms = CONFIG_TCPSERVER_ETH_SPI_POLLING_MS_VAL;
#endif
```

create a `mac` driver instance by calling `esp_eth_mac_new_w5500`,

```c
eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();

// To avoid stack overflow, you could choose a larger value
mac_config.rx_task_stack_size = 4096;

// W5500 has a factory burned MAC. Therefore, configuring MAC address manually is optional.
esp_eth_mac_t *mac = esp_eth_mac_new_w5500(&w5500_config,&mac_config);
```

create a `phy` driver instance by calling `esp_eth_phy_new_w5500()`.


```c
eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();

esp_eth_phy_t *phy = esp_eth_phy_new_w5500(&phy_config);
```

For more information of how to use ESP-IDF Ethernet driver, visit [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_eth.html).
