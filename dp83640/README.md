# TI DP83640 Ethernet PHY Driver (Preview)

## Overview

The DP83640 is a 10/100 Mbps PHY with IEEE 1588 v1 and v2 standard supported, which normally used in the Ethernet time synchronization scenario. It supports UDP/IPv4, UDP/IPv6, and Layer2 Ethernet
Packets Supported. The DP83640 supports to select MII and RMII MAC interfaces by a strapped pin. It can also output the synchronized clock via some programmable GPIOs. But it still requires a Precision Time Protocol (PTP) in the upper layer to provide the software state machine in the network synchronization applications. More information about the chip can found in the product [datasheets](https://www.ti.com/lit/ds/symlink/dp83640.pdf).

## ESP-IDF Usage

Just add include of `esp_eth_phy_dp83640.h` to your project,

```c
#include "esp_eth_phy_dp83640.h"
```

create a `phy` driver instance by calling `esp_eth_phy_new_dp83640()`

```c
eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();

// Update PHY config based on board specific configuration
phy_config.phy_addr = CONFIG_EXAMPLE_ETH_PHY_ADDR;
phy_config.reset_gpio_num = CONFIG_EXAMPLE_ETH_PHY_RST_GPIO;
esp_eth_phy_t *phy = esp_eth_phy_new_dp83640(&phy_config);
```

For the information of how to use ESP-IDF Ethernet driver without IEEE1588 feature, visit [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_eth.html).
