# Davicom DM9051(A) Ethernet Driver

## Overview

DM9051 is a single-chip 10/100Mbps Ethernet controller with an SPI interface. It integrates a 10/100Mbps Ethernet MAC and PHY, supporting auto-negotiation, auto-MDIX, and wake-on-LAN functionality. The DM9051 operates at 3.3V and provides a simple SPI interface for easy integration with microcontrollers. More information about the chip can be found in the product [datasheets](https://www.davicom.com.tw/production-item.php?lang_id=en).

## ESP-IDF Usage

Add this component from [IDF Component Manager](https://components.espressif.com/) to your project using `idf.py add-dependency` and include `esp_eth_phy_dm9051.h` and `esp_eth_mac_dm9051.h`,

```c
#include "esp_eth_phy_dm9051.h"
#include "esp_eth_mac_dm9051.h"
```

create a configuration instance by calling `ETH_DM9051_DEFAULT_CONFIG`,

```c
// Configure SPI interface for specific SPI module
spi_device_interface_config_t spi_devcfg = {
    .mode = 0,
    .clock_speed_hz = CONFIG_TCPSERVER_ETH_SPI_CLOCK_MHZ * 1000 * 1000,
    .queue_size = 16,
    .spics_io_num = CONFIG_TCPSERVER_ETH_SPI_CS_GPIO
};

eth_dm9051_config_t dm9051_config = ETH_DM9051_DEFAULT_CONFIG(CONFIG_TCPSERVER_ETH_SPI_HOST,&spi_devcfg);
dm9051_config.int_gpio_num = CONFIG_TCPSERVER_ETH_SPI_INT_GPIO;

#if CONFIG_TCPSERVER_ETH_SPI_INT_GPIO < 0
dm9051_config.poll_period_ms = CONFIG_TCPSERVER_ETH_SPI_POLLING_MS_VAL;
#endif
```

create a `mac` driver instance by calling `esp_eth_mac_new_dm9051`,

```c
eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();

// To avoid stack overflow, you could choose a larger value
mac_config.rx_task_stack_size = 4096;

// DM9051 has a factory burned MAC. Therefore, configuring MAC address manually is optional.
esp_eth_mac_t *mac = esp_eth_mac_new_dm9051(&dm9051_config,&mac_config);
```

create a `phy` driver instance by calling `esp_eth_phy_new_dm9051()`.


```c
eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();

esp_eth_phy_t *phy = esp_eth_phy_new_dm9051(&phy_config);
```

For more information of how to use ESP-IDF Ethernet driver, visit [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_eth.html).

