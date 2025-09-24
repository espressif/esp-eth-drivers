# RTL8201 Ethernet PHY Driver

## Overview

The RTL8201F/SR8201F is a single port 10/100Mb Ethernet Transceiver with auto MDIX. Goto http://www.corechip-sz.com/productsview.asp?id=22 for more information.

## ESP-IDF Usage

Add this component from [IDF Component Manager](https://components.espressif.com/) to your project using `idf.py add-dependency` and include `esp_eth_phy_rtl8201.h`,

```c
#include "esp_eth_phy_rtl8201.h"
```

create a `phy` driver instance by calling `esp_eth_phy_new_rtl8201()`

```c
eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();

// Update PHY config based on board specific configuration
phy_config.phy_addr = CONFIG_EXAMPLE_ETH_PHY_ADDR;
phy_config.reset_gpio_num = CONFIG_EXAMPLE_ETH_PHY_RST_GPIO;
esp_eth_phy_t *phy = esp_eth_phy_new_rtl8201(&phy_config);
```

For more information of how to use ESP-IDF Ethernet driver, visit [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_eth.html).
