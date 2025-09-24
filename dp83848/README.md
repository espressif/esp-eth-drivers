# DP83848 Ethernet PHY Driver

## Overview

DP83848 is a single port 10/100Mb/s Ethernet Physical Layer Transceiver. Goto http://www.ti.com/product/DP83848J for more information.

## ESP-IDF Usage

Add this component from [IDF Component Manager](https://components.espressif.com/) to your project using `idf.py add-dependency` and include `esp_eth_phy_dp83848.h`,

```c
#include "esp_eth_phy_dp83848.h"
```

create a `phy` driver instance by calling `esp_eth_phy_new_dp83848()`

```c
eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();

// Update PHY config based on board specific configuration
phy_config.phy_addr = CONFIG_EXAMPLE_ETH_PHY_ADDR;
phy_config.reset_gpio_num = CONFIG_EXAMPLE_ETH_PHY_RST_GPIO;
esp_eth_phy_t *phy = esp_eth_phy_new_dp83848(&phy_config);
```

For more information of how to use ESP-IDF Ethernet driver, visit [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_eth.html).
