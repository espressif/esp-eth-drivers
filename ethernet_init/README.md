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

> [!WARNING]
> When selecting `Generic 802.3 PHY`, basic functionality should always work for PHY compliant with IEEE 802.3. However, some specific features might be limited. A typical example is loopback functionality, where certain PHYs may require setting a specific speed mode to operate correctly.

## API

### Steps to use the component in an example code:
1. Add this component to your project using ```idf.py add-dependency``` command.
2. Include ```ethernet_init.h```
3. Call the following functions as required:
    ```c
    uint8_t eth_port_cnt = 0;
    esp_eth_handle_t *eth_handles;

    // Create default event loop that running in background to handle Ethernet events
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Initialize Ethernet driver
    ESP_ERROR_CHECK(ethernet_init_all(&eth_handles, &eth_port_cnt));
    ```
4. Attach Ethernet interfaces to IP stack via `ESP-NETIF`. This is most common but semioptional step - in advanced applications you can access frames directly.
    ```c
    // Create instance(s) of esp-netif for Ethernet(s)
    if (eth_port_cnt == 1) {
        // Use ESP_NETIF_DEFAULT_ETH when just one Ethernet interface is used and you don't need to modify
        // default esp-netif configuration parameters.
        esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
        esp_netif_t *eth_netif = esp_netif_new(&cfg);
        // Attach Ethernet driver to TCP/IP stack
        ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handles[0])));
    } else {
        // Use ESP_NETIF_INHERENT_DEFAULT_ETH when multiple Ethernet interfaces are used and so you need to modify
        // esp-netif configuration parameters for each interface (name, priority, etc.).
        esp_netif_inherent_config_t esp_netif_config = ESP_NETIF_INHERENT_DEFAULT_ETH();
        esp_netif_config_t cfg_spi = {
            .base = &esp_netif_config,
            .stack = ESP_NETIF_NETSTACK_DEFAULT_ETH
        };

        for (int i = 0; i < eth_port_cnt; i++) {
            sprintf(if_key_str, "ETH_%d", i);
            sprintf(if_desc_str, "eth%d", i);
            esp_netif_config.if_key = if_key_str;
            esp_netif_config.if_desc = if_desc_str;
            esp_netif_config.route_prio -= i * 5;
            esp_netif_t *eth_netif = esp_netif_new(&cfg_spi);

            // Attach Ethernet driver to TCP/IP stack
            ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handles[i])));
        }
    }
    ```
5. Start the Ethernet interfaces.
    ```c
    // Start Ethernet driver state machine
    for (int i = 0; i < eth_port_cnt; i++) {
        ESP_ERROR_CHECK(esp_eth_start(eth_handles[i]));
    }
    ```
4. Optional step to get information about initialized network interfaces:
    ```c
    // Get the device information of the first ethernet handle
    eth_dev_info_t eth_info = ethernet_init_get_dev_info(eth_handles[0]);
    ```
5. Optional step to deinit when you do not need Ethernet in your application:
    ```c
    // Stop and Deinitialize ethernet
    for (int i = 0; i < eth_port_cnt; i++) {
        ESP_ERROR_CHECK(esp_eth_stop(eth_handles[i]));
    }
    ethernet_deinit_all(eth_handles);
    ```

## Configuration

The ESP-IDF supports the usage of multiple Ethernet interfaces at a time when external modules are utilized. There are several options you can combine:
   * Internal EMAC and one SPI Ethernet module.
   * Two SPI Ethernet modules connected to single SPI interface and accessed by switching appropriate CS.
   * Internal EMAC and two SPI Ethernet modules.

>[!NOTE]
> You can select to use different types of SPI Ethernet modules. Such configuration does not make much sense in typical 10BASE-T/100BASE-TX network configuration but can be useful when implementing **10BASE-T/100BASE-TX to 10BASE-T1S** bridging.

>[!TIP]
> Follow the help inside **Project Configuration** (`idf.py menuconfig`) for each configuration option to gain better understanding. Press `?` on any option to view its help text.

### Pin Assignment

Please always consult Espressif Technical reference manual along with datasheet or `ESP-IDF Programming Guide` for specific ESP SoC you use when assigning GPIO pins, especially when choosing from system configuration menu for the Ethernet initialization. Some pins cannot be used (they may already be utilized for different purpose like SPI Flash/RAM, some pins might be inputs only, etc.).

Typical examples:
* GPIO16 or GPIO17 cannot be used as RMII CLK output on ESP32-WROVER module since these pins are already occupied by PSRAM.

### Using Internal MAC

* **RMII PHY** wiring is fixed on **ESP32** and can not be changed through either IO MUX or GPIO Matrix. However, on more modern chips like **ESP32P4**, you have multiple options how to route RMII signals over IO MUX.

* **SMI (Serial Management Interface)** wiring is not fixed. You may change it to any GPIO via GPIO Matrix.

* **PHY chip reset pin**, if want to do a hardware reset during initialization, then you have to connect it with one GPIO via GPIO Matrix.

### Using SPI Ethernet Modules

* SPI Ethernet modules (DM9051, W5500, ...) typically consume one SPI interface plus an interrupt and reset GPIO. They can be connected as follows for ESP32 as an example. However, they can be remapped to any pin using the GPIO Matrix.

| ESP32 GPIO | SPI ETH Module |
|   ------   | -------------- |
|   GPIO14   | SPI_CLK        |
|   GPIO13   | SPI_MOSI       |
|   GPIO12   | SPI_MISO       |
|   GPIO15   | SPI_CS         |
|   GPIO4    | Interrupt      |
|   NC       | Reset          |

* To be able to utilize full SPI speed on some chips (ESP32 for example), you need to configure SPI GPIO to pins which can be directly connected to peripherals using IO MUX.

* When using two Ethernet SPI modules at a time, they are to be connected to single SPI interface. Both modules then share data (MOSI/MISO) and CLK signals. However, the CS, interrupt and reset pins need to be connected to separate GPIO for each Ethernet SPI module.

* You don't need to connect the interrupt signal. Instead, you can use the SPI module in `polling` mode. In polling mode, you need to configure how often the module checks for new data (this is called the polling period).


