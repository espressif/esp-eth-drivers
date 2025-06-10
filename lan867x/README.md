# Microchip LAN867x Ethernet PHY Driver

## Overview

The LAN8670/1/2 is a high-performance 10BASE-T1S single-pair Ethernet PHY transceiver for 10 Mbit/s half-duplex networking over a single pair of conductors.
More information about the chip can be found on the product page: [LAN8670](https://www.microchip.com/en-us/product/lan8670), [LAN8671](https://www.microchip.com/en-us/product/lan8671).

## ESP-IDF Usage

> [!TIP]
> Use [Component for Ethernet Initialization](https://components.espressif.com/components/espressif/ethernet_init) for smoother user experience.

Add this component from [IDF Component Manager](https://components.espressif.com/) to your project using `idf.py add-dependency` and include `esp_eth_phy_lan867x.h`,

```c
#include "esp_eth_phy_lan867x.h"
```

create a `phy` driver instance by calling `esp_eth_phy_new_lan867x()`

```c
eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();

// Update PHY config based on board specific configuration
phy_config.phy_addr = CONFIG_EXAMPLE_ETH_PHY_ADDR;
phy_config.reset_gpio_num = CONFIG_EXAMPLE_ETH_PHY_RST_GPIO;
esp_eth_phy_t *phy = esp_eth_phy_new_lan867x(&phy_config);
```

#### Collision avoidance methods: CSMA/CD and PLCA

In most cases you can rely on the default CSMA/CD (Carrier Sense Multiple Access / Collision Detection) provided by the MAC layer to avoid collisions when connecting multiple nodes. It requires zero configuration to work and does a good job of coordinating when data is sent. However, due to its nature, the delays between transmissions are random. For timing-critical applications, deterministic PLCA (PHY-Level Collision Avoidance) provides a guaranteed time period between transmissions along with improved throughput. Read more about PLCA and its usage in the [PLCA FAQ](https://www.ieee802.org/3/cg/public/July2018/PLCA%20FAQ.pdf).

#### Configuring PLCA

PLCA configuration is performed using the `esp_eth_ioctl` function with one of the commands described in [LAN86XX PHY Sub-component](../lan86xx_common/README.md).
