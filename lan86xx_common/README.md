# LAN86XX PHY Common Sub-component

This sub-component is common for both LAN865x and LAN867x drivers. It covers PHY layer functionality. It ensures initialization of ESP-IDF PHY instance and provides an interface for PLCA (Physical Layer Collision Avoidance) configuration.

> [!CAUTION]
> This component is not intended for standalone use!

## Configuring PLCA

> [!TIP]
> Use [Component for Ethernet Initialization](https://components.espressif.com/components/espressif/ethernet_init) to configure PLCA during the project configuration in Kconfig menu. The `Component for Ethernet Initialization` also lists extended help related to the PLCA configuration and forbids you to use invalid combinations of configuration options.

PLCA requires setting at least PLCA ID, node count, and selecting one node as a coordinator but ensures precise and repeatable time between transmissions, and due to the flexibility of the transmit opportunity timer, burst mode and additional transmit opportunities you can fine-tune it to your needs.

PLCA configuration is performed using the `esp_eth_ioctl` function with one of the following commands.

| Command                            | Argument         | Action                                                                                        |
|------------------------------------|------------------|-----------------------------------------------------------------------------------------------|
| LAN86XX_ETH_CMD_S_EN_PLCA          | bool* enable     | Enable (if true) or disable PLCA, PHY will use CDMA/CD if disabled                            |
| LAN86XX_ETH_CMD_G_EN_PLCA          | bool* enable     | Write PLCA status (true if enabled) to the location via pointer                               |
| LAN86XX_ETH_CMD_S_PLCA_NCNT        | uint8_t* count   | Set node count to the value passed through the pointer                                        |
| LAN86XX_ETH_CMD_G_PLCA_NCNT        | uint8_t* count   | Write node count configured in the PLCA to the location via pointer                           |
| LAN86XX_ETH_CMD_S_PLCA_ID          | uint8_t* id      | Set ID to the value passed through the pointer                                                |
| LAN86XX_ETH_CMD_G_PLCA_ID          | uint8_t* id      | Write ID configured in the PLCA to the location via pointer                                   |
| LAN86XX_ETH_CMD_S_PLCA_TOT         | uint8_t* time    | Set PLCA Transmit Opportunity Timer in BTs                                                    |
| LAN86XX_ETH_CMD_G_PLCA_TOT         | uint8_t* time    | Write Transmit Opportunity Timer value in BTs                                                 |
| LAN86XX_ETH_CMD_ADD_TX_OPPORTUNITY | uint8_t* node_id | Add additional transmit opportunity for chosen node                                           |
| LAN86XX_ETH_CMD_RM_TX_OPPORTUNITY  | uint8_t* node_id | Remove additional transmit opportunity for chosen node                                        |
| LAN86XX_ETH_CMD_S_MAX_BURST_COUNT  | uint8_t* maxcnt  | Set max count of additional packets transmitted during one frame, or 0 to disable PLCA burst   |
| LAN86XX_ETH_CMD_G_MAX_BURST_COUNT  | uint8_t* maxcnt  | Write max count of additional packets transmitted during one frame to the location via pointer |
| LAN86XX_ETH_CMD_S_BURST_TIMER      | uint8_t* time    | Set time during which additional packets in BTs                                               |
| LAN86XX_ETH_CMD_G_BURST_TIMER      | uint8_t* time    | Write time during which additional packets can be sent in BTs to the location via pointer     |
| LAN86XX_ETH_CMD_PLCA_RST           |                  | Perform reset of the PLCA                                                                     |

One of the devices on the network must be a **coordinator** for which you need to set _node count_ to amount of connected nodes, and _ID_ to 0.
On all other nodes, only _ID_ is required, which must be unique for every node.

After that the device is ready and the Ethernet driver can be used as normal. For more information on how to use the ESP-IDF Ethernet driver, visit the [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_eth.html).

---
BT - Bit time with a value of 100 ns