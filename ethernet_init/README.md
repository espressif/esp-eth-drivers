# Component for Ethernet Initialization

This component makes it easier to set up and control Ethernet connections in Espressif IoT projects. It hides the lower-level complexities of Ethernet driver initialization and configuration, so developers can focus on building their applications without delving deep into hardware details.
It also allows users to select from various supported Ethernet chips, making development faster.

Supported devices are:
* Generic 802.3 PHY (any Ethernet PHY chip compliant with IEEE 802.3)
* IP101
* RTL8201/SR8201
* LAN87xx
* DP83848
* KSZ80xx
* LAN867x (10BASE-T1S)
* SPI Ethernet:
    * DM9051 MAC-PHY Module
    * KSZ8851SNL MAC-PHY Module
    * W5500 MAC-PHY Module
    * ENC28J60 MAC-PHY Module
    * CH390 MAC-PHY Module
    * LAN865x MAC-PHY Module (10BASE-T1S)

> ⚠️ **Warning**: When selecting `Generic 802.3 PHY`, basic functionality should always work for PHY compliant with IEEE 802.3. However, some specific features might be limited. A typical example is loopback functionality, where certain PHYs may require setting a specific speed mode to operate correctly.

## API

### Steps to use the component in an example code:
1. Add this component to your project using ```idf.py add-dependency``` command.
2. Include ```ethernet_init.h```
3. Call the following functions as required:
    ```c
    // Initialize Ethernet driver
    ESP_ERROR_CHECK(ethernet_init_all(&eth_handles, &eth_port_cnt));

    // Get the device information of the ethernet handle
    eth_dev_info_t eth_info = ethernet_init_get_dev_info(eth_handle);

    // Stop and Deinitialize ethernet
    ethernet_deinit_all(eth_handles);
    ```
