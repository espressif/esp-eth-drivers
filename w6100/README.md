# WIZnet W6100 Ethernet Driver

## Overview

W6100 is the successor to the W5500, adding IPv6 support to the hardwired TCP/IP stack. Like the W5500, it is an embedded Ethernet controller using SPI (Serial Peripheral Interface). The W6100 provides a complete and hardwired TCP/IP stack (not used in the ESP driver) and 10/100 Ethernet MAC and PHY. It supports 3.3V operation and has a built-in 32K bytes internal buffer for data transmission. More information about the chip can be found in the product [datasheet](https://docs.wiznet.io/Product/iEthernet/W6100/datasheet).

## ESP-IDF Usage

Add this component from [IDF Component Manager](https://components.espressif.com/) to your project using `idf.py add-dependency` and include `esp_eth_phy_w6100.h` and `esp_eth_mac_w6100.h`,

```c
#include "esp_eth_phy_w6100.h"
#include "esp_eth_mac_w6100.h"
```

create a configuration instance by calling `ETH_W6100_DEFAULT_CONFIG`,

```c
// Configure SPI interface for specific SPI module
spi_device_interface_config_t spi_devcfg = {
    .mode = 0,
    .clock_speed_hz = CONFIG_EXAMPLE_ETH_SPI_CLOCK_MHZ * 1000 * 1000,
    .queue_size = 16,
    .spics_io_num = CONFIG_EXAMPLE_ETH_SPI_CS_GPIO
};

eth_w6100_config_t w6100_config = ETH_W6100_DEFAULT_CONFIG(CONFIG_EXAMPLE_ETH_SPI_HOST, &spi_devcfg);
w6100_config.int_gpio_num = CONFIG_EXAMPLE_ETH_SPI_INT_GPIO;

#if CONFIG_EXAMPLE_ETH_SPI_INT_GPIO < 0
w6100_config.poll_period_ms = CONFIG_EXAMPLE_ETH_SPI_POLLING_MS_VAL;
#endif
```

create a `mac` driver instance by calling `esp_eth_mac_new_w6100`,

```c
eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();

// To avoid stack overflow, you could choose a larger value
mac_config.rx_task_stack_size = 4096;

// W6100 has a factory burned MAC. Therefore, configuring MAC address manually is optional.
esp_eth_mac_t *mac = esp_eth_mac_new_w6100(&w6100_config, &mac_config);
```

create a `phy` driver instance by calling `esp_eth_phy_new_w6100()`.

```c
eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();

esp_eth_phy_t *phy = esp_eth_phy_new_w6100(&phy_config);
```

For more information of how to use ESP-IDF Ethernet driver, visit [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_eth.html).
