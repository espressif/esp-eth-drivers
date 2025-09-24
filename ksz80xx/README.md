# KSZ80xx Ethernet PHY Driver

## Overview

With the KSZ80xx series, Microchip offers single-chip 10BASE-T/100BASE-TX Ethernet Physical Layer Transceivers (PHY). The following chips are supported: KSZ8001, KSZ8021, KSZ8031, KSZ8041, KSZ8051, KSZ8061, KSZ8081, KSZ8091. Goto https://www.microchip.com for more information.

## ESP-IDF Usage

Add this component from [IDF Component Manager](https://components.espressif.com/) to your project using `idf.py add-dependency` and include `esp_eth_phy_ksz80xx.h`,

```c
#include "esp_eth_phy_ksz80xx.h"
```

create a `phy` driver instance by calling `esp_eth_phy_new_ksz80xx()`

```c
eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();

// Update PHY config based on board specific configuration
phy_config.phy_addr = CONFIG_EXAMPLE_ETH_PHY_ADDR;
phy_config.reset_gpio_num = CONFIG_EXAMPLE_ETH_PHY_RST_GPIO;
esp_eth_phy_t *phy = esp_eth_phy_new_ksz80xx(&phy_config);
```

For more information of how to use ESP-IDF Ethernet driver, visit [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_eth.html).
