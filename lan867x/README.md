# Microchip LAN867x Ethernet PHY Driver

## Overview

The LAN8670/1/2 is a high-performance 10BASE-T1S single-pair Ethernet PHY transceiver for 10 Mbit/s half-duplex networking over a single pair of conductors. 
More information about the chip can be found on the product page: [LAN8670](https://www.microchip.com/en-us/product/lan8670), [LAN8671](https://www.microchip.com/en-us/product/lan8671).

## ESP-IDF Usage

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

In most cases you can rely on the default CSMA/CD (Carrier Sense Multiple Access / Collision Detection) provided by the MAC layer to avoid collisions when connecting multiple nodes. It requires zero configuration to work and does a good job of coordinating when data is sent. However, due to its nature, the delays between transmissions are random. For timing-critical applications, deterministic PLCA (PHY-Level Collision Avoidance) provides a guaranteed time period between transmissions along with improved throughput. Read more about PLCA and its usage at [ieee802.org](https://www.ieee802.org/3/cg/public/July2018/PLCA%20FAQ.pdf).

PLCA requires setting at least PLCA ID, node count, and selecting one node as a coordinator but ensures precise and repeatable time between transmissions, and due to the flexibility of the transmit opportunity timer, burst mode and additional transmit opportunities you can fine-tune it to your needs.

#### Configuring PLCA

PLCA configuration is done using the `esp_eth_ioctl` function with one of the following commands.

| Command                            | Argument         | Action                                                                                        |
|------------------------------------|------------------|-----------------------------------------------------------------------------------------------|
| LAN867X_ETH_CMD_S_EN_PLCA          | bool* enable     | Enable (if true) or disable PLCA, PHY will use CDMA/CD if disabled                            |
| LAN867X_ETH_CMD_G_EN_PLCA          | bool* enable     | Write PLCA status (true if enabled) to the location via pointer                               |
| LAN867X_ETH_CMD_S_PLCA_NCNT        | uint8_t* count   | Set node count to the value passed through the pointer                                        |
| LAN867X_ETH_CMD_G_PLCA_NCNT        | uint8_t* count   | Write node count configured in the PLCA to the location via pointer                           |
| LAN867X_ETH_CMD_S_PLCA_ID          | uint8_t* id      | Set ID to the value passed through the pointer                                                |
| LAN867X_ETH_CMD_G_PLCA_ID          | uint8_t* id      | Write ID configured in the PLCA to the location via pointer                                   |
| LAN867X_ETH_CMD_S_PLCA_TOT         | uint8_t* time    | Set PLCA Transmit Opportunity Timer in BTs                                                    |
| LAN867X_ETH_CMD_G_PLCA_TOT         | uint8_t* time    | Write Transmit Opportunity Timer value in BTs                                                 |
| LAN867X_ETH_CMD_ADD_TX_OPPORTUNITY | uint8_t* node_id | Add additional transmit opportunity for chosen node                                           |
| LAN867X_ETH_CMD_RM_TX_OPPORTUNITY  | uint8_t* node_id | Remove additional transmit opportunity for chosen node                                        |
| LAN867X_ETH_CMD_S_MAX_BURST_COUNT  | uint8_t* maxcnt  | Set max count of additonal packets transmitted during one frame, or 0 to disable PLCA burst   |
| LAN867X_ETH_CMD_G_MAX_BURST_COUNT  | uint8_t* maxcnt  | Write max count of additonal packets transmitted during one frame to the location via pointer |
| LAN867X_ETH_CMD_S_BURST_TIMER      | uint8_t* time    | Set time during which additional packets in BTs                                               |
| LAN867X_ETH_CMD_G_BURST_TIMER      | uint8_t* time    | Write time during which additional packets can be sent in BTs to the location via pointer     |
| LAN768X_ETH_CMD_PLCA_RST           |                  | Perform reset of the PLCA                                                                     |

One of the devices on the network must be a **coordinator** for which you need to set _node count_ to amount of connected nodes, and _ID_ to 0.
On all other nodes, only _ID_ is required, which must be unique for every node.

After that the device is ready and the Ethernet driver can be used as normal. For more information on how to use the ESP-IDF Ethernet driver, visit the [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_eth.html).

---
BT - Bit time with a value of 100 ns