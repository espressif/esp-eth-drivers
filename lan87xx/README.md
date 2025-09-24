# LAN87xx Ethernet PHY Driver

## Overview

The LAN87xx series are single-port 10/100 Mbps Ethernet PHY transceivers. Below chips are supported:

* LAN8710A is a small footprint MII/RMII 10/100 Ethernet Transceiver with HP Auto-MDIX and flexPWR® Technology.
* LAN8720A is a small footprint RMII 10/100 Ethernet Transceiver with HP Auto-MDIX Support.
* LAN8740A/LAN8741A is a small footprint MII/RMII 10/100 Energy Efficient Ethernet Transceiver with HP Auto-MDIX and flexPWR® Technology.
* LAN8742A is a small footprint RMII 10/100 Ethernet Transceiver with HP Auto-MDIX and flexPWR® Technology.

Goto https://www.microchip.com for more information.

## ESP-IDF Usage

Add this component from [IDF Component Manager](https://components.espressif.com/) to your project using `idf.py add-dependency` and include `esp_eth_phy_lan87xx.h`,

```c
#include "esp_eth_phy_lan87xx.h"
```

create a `phy` driver instance by calling `esp_eth_phy_new_lan87xx()`

```c
eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();

// Update PHY config based on board specific configuration
phy_config.phy_addr = CONFIG_EXAMPLE_ETH_PHY_ADDR;
phy_config.reset_gpio_num = CONFIG_EXAMPLE_ETH_PHY_RST_GPIO;
esp_eth_phy_t *phy = esp_eth_phy_new_lan87xx(&phy_config);
```

For more information of how to use ESP-IDF Ethernet driver, visit [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_eth.html).
