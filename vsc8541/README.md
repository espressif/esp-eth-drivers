# VSC8541 Ethernet PHY Driver

## Overview

VSC8541 is a single port 10/100/1000BASE-T Gigabit Ethernet transceiver from Microchip (formerly Vitesse).
The driver supports link/speed/duplex detection over the MII/GMII/RMII/RGMII interfaces exposed by the chip.

## ESP-IDF Usage

Add this component from [IDF Component Manager](https://components.espressif.com/) to your project using `idf.py add-dependency` and include `esp_eth_phy_vsc8541.h`,

```c
#include "esp_eth_phy_vsc8541.h"
```

create a `phy` driver instance by calling `esp_eth_phy_new_vsc8541()`

```c
eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();

// Update PHY config based on board specific configuration
phy_config.phy_addr = CONFIG_EXAMPLE_ETH_PHY_ADDR;
phy_config.reset_gpio_num = CONFIG_EXAMPLE_ETH_PHY_RST_GPIO;
esp_eth_phy_t *phy = esp_eth_phy_new_vsc8541(&phy_config);
```

### Overriding MAC interface in software

The VSC8541 MAC interface is normally selected by bootstrap pins. Boards strapped for RGMII
can be switched to RMII (or another supported interface) in software, which can be useful for
debug.

> [!IMPORTANT]
> Use `VSC8541_ETH_CMD_S_MAC_IFACE` after `esp_eth_driver_install()` and only while the
> Ethernet driver is **stopped** (before `esp_eth_start()`, or after `esp_eth_stop()`). The PHY
> driver cannot observe the Ethernet start/stop state, so this is not enforced at the PHY layer.

Once set, the selection is sticky: it is re-applied after PHY software or hardware reset.

```c
vsc8541_mac_interface_sel_t mac_iface = VSC8541_MAC_INTF_SEL_RMII;
esp_eth_ioctl(eth_handle, VSC8541_ETH_CMD_S_MAC_IFACE, &mac_iface);
```

### Disabling RMII CLKOUT (RX_CLK)

In RMII Mode 1 the MAC (or an external oscillator) supplies the 50 MHz reference, so the PHY
must not drive RMII_CLKOUT on the RX_CLK pin. That output is enabled by default; disable it
with `VSC8541_ETH_CMD_S_RMII_CLKOUT` after the Ethernet driver is installed:

```c
bool rmii_clkout_en = false;
esp_eth_ioctl(eth_handle, VSC8541_ETH_CMD_S_RMII_CLKOUT, &rmii_clkout_en);
```

For more information of how to use ESP-IDF Ethernet driver, visit [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_eth.html).
