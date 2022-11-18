# Analog Devices ADIN1200 Ethernet PHY Driver

## Overview

The ADIN1200 is low power, low latency single-port Ethernet transceiver designed for industrial Ethernet applications. It is compliant with the IEEE 802.3 Ethernet standard. The ADIN1200 supports 10/100 Mbps Ethernet speeds. It supports unmanaged configuration using multi-level strapping, or managed configuration using MDIO access and operate over a wide industrial temperature range (−40°C to + 105°C). The ADIN1200 support MII, RMII, and RGMII MAC interfaces. More information about the chip can found in the product [datasheets](https://www.analog.com/media/en/technical-documentation/data-sheets/ADIN1200.pdf).

## ESP-IDF Usage

Just add include of `esp_eth_phy_adin1200.h` to your project,

```c
#include "esp_eth_phy_adin1200.h"
```

create a `phy` driver instance by calling `esp_eth_phy_new_adin1200()`

```c
eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();

// Update PHY config based on board specific configuration
phy_config.phy_addr = CONFIG_EXAMPLE_ETH_PHY_ADDR;
phy_config.reset_gpio_num = CONFIG_EXAMPLE_ETH_PHY_RST_GPIO;
esp_eth_phy_t *phy = esp_eth_phy_new_adin1200(&phy_config);
```

and use the Ethernet driver as you are used to. For more information of how to use ESP-IDF Ethernet driver, visit [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_eth.html).
