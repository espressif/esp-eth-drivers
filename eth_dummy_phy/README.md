# Dummy PHY (EMAC to EMAC)

The `Dummy PHY` component enables you a direct **EMAC to EMAC** communication between two Ethernet capable devices without usage of external PHY transceiver. The design idea is to provide EMAC to EMAC connection between two devices without any modification to ESP-IDF Ethernet driver.

Note that usage of the `Dummy PHY` component does not conform with the RevMII standard! As such, it does not provide any management interface and so the speed/duplex mode needs to be statically configured to **the same mode in both devices**. Default configuration is 100Mbps/full duplex mode. If you need to change it, use `esp_eth_ioctl()` function with `ETH_CMD_S_SPEED` or `ETH_CMD_S_DUPLEX_MODE` command.

## API

### Steps to use the component in an example code

1. Add this component to your project using `idf.py add-dependency` command.
2. Include `esp_eth_phy_dummy.h`
3. Create a dummy `phy` driver instance by calling `esp_eth_phy_new_dummy`and initialize Ethernet as you are used to. The only difference is to set SMI GPIOs to `-1` since the management interface is not utilized.
    ```c
    // Init common MAC and PHY configs to default
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();

    // Update PHY config based on board specific configuration
    phy_config.reset_gpio_num = -1; // no HW reset

    // Init vendor specific MAC config to default
    eth_esp32_emac_config_t esp32_emac_config = ETH_ESP32_EMAC_DEFAULT_CONFIG();
    // Update vendor specific MAC config based on board configuration
    // No SMI, speed/duplex must be statically configured the same in both devices
    // MIIM interface is not used since does not provide access to all registers
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
    esp32_emac_config.smi_gpio.mdc_num = -1;
    esp32_emac_config.smi_gpio.mdio_num = -1;
#else
    esp32_emac_config.smi_mdc_gpio_num = -1;
    esp32_emac_config.smi_mdio_gpio_num = -1;
#endif

    // Create new ESP32 Ethernet MAC instance
    esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&esp32_emac_config, &mac_config);
    // Create dummy PHY instance
    esp_eth_phy_t *phy = esp_eth_phy_new_dummy(&phy_config);
    ```

## Examples

Please refer to [Simple Example](./examples/simple/README.md) for more information.
