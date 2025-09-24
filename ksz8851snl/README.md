# Microchip KSZ8851SNL Ethernet Driver

## Overview

KSZ8851SNL is a single-chip 10/100Mbps Ethernet controller with an SPI interface. It integrates a 10/100Mbps Ethernet MAC and PHY, supporting auto-negotiation, auto-MDIX, and energy-efficient Ethernet (EEE) functionality. The KSZ8851SNL operates at 3.3V and provides a simple SPI interface for easy integration with microcontrollers. It features advanced power management and supports wake-on-LAN functionality. More information about the chip can be found in the product [datasheets](https://www.microchip.com/en-us/product/KSZ8851).

## ESP-IDF Usage

Add this component from [IDF Component Manager](https://components.espressif.com/) to your project using `idf.py add-dependency` and include `esp_eth_phy_ksz8851snl.h` and `esp_eth_mac_ksz8851snl.h`,

```c
#include "esp_eth_phy_ksz8851snl.h"
#include "esp_eth_mac_ksz8851snl.h"
```

create a configuration instance by calling `ETH_KSZ8851SNL_DEFAULT_CONFIG`,

```c
// Configure SPI interface for specific SPI module
spi_device_interface_config_t spi_devcfg = {
    .mode = 0,
    .clock_speed_hz = CONFIG_TCPSERVER_ETH_SPI_CLOCK_MHZ * 1000 * 1000,
    .queue_size = 16,
    .spics_io_num = CONFIG_TCPSERVER_ETH_SPI_CS_GPIO
};

eth_ksz8851snl_config_t ksz8851snl_config = ETH_KSZ8851SNL_DEFAULT_CONFIG(CONFIG_TCPSERVER_ETH_SPI_HOST,&spi_devcfg);
ksz8851snl_config.int_gpio_num = CONFIG_TCPSERVER_ETH_SPI_INT_GPIO;

#if CONFIG_TCPSERVER_ETH_SPI_INT_GPIO < 0
ksz8851snl_config.poll_period_ms = CONFIG_TCPSERVER_ETH_SPI_POLLING_MS_VAL;
#endif
```

create a `mac` driver instance by calling `esp_eth_mac_new_ksz8851snl`,

```c
eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();

// To avoid stack overflow, you could choose a larger value
mac_config.rx_task_stack_size = 4096;

// KSZ8851SNL has a factory burned MAC. Therefore, configuring MAC address manually is optional.
esp_eth_mac_t *mac = esp_eth_mac_new_ksz8851snl(&ksz8851snl_config,&mac_config);
```

create a `phy` driver instance by calling `esp_eth_phy_new_ksz8851snl()`.


```c
eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();

esp_eth_phy_t *phy = esp_eth_phy_new_ksz8851snl(&phy_config);
```

For more information of how to use ESP-IDF Ethernet driver, visit [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_eth.html).
