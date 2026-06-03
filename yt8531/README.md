# YT8531 Ethernet PHY Driver

## Overview

YT8531S / YT8531SC is a single-port 10/100/1000BASE-T Gigabit Ethernet transceiver from Motorcomm
(裕太微电子). The chip supports RGMII and SGMII MAC interfaces and includes a built-in 3.3 V → 1.1 V
switching regulator plus an optional LDO for 2.5 V / 1.8 V RGMII I/O.

This driver targets the UTP ↔ RGMII application mode (the default pin-strap configuration,
`CFG_MODE[2:0] = 3'b000`).

## Register access model

The YT8531 exposes two register spaces:

| Space | Access | Description |
|-------|--------|-------------|
| Standard IEEE 802.3 (0x00 – 0x1F) | Direct MDIO | UTP *or* SDS bank, controlled by `SMI_SDS_PHY` |
| Extended registers (`EXT_0x????`) | Indirect via reg `0x1E` (address) / `0x1F` (data) | Chip-common and per-bank registers |

The driver always sets `SMI_SDS_PHY` (EXT `0xA000`) bit 1 to `0` during `init` to map the standard
register space to the UTP bank.

## ESP-IDF Usage

Add this component from [IDF Component Manager](https://components.espressif.com/) to your project
using `idf.py add-dependency` and include `esp_eth_phy_yt8531.h`:

```c
#include "esp_eth_phy_yt8531.h"
```

Create a `phy` driver instance by calling `esp_eth_phy_new_yt8531()`:

```c
eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
phy_config.phy_addr      = CONFIG_EXAMPLE_ETH_PHY_ADDR;
phy_config.reset_gpio_num = CONFIG_EXAMPLE_ETH_PHY_RST_GPIO;
esp_eth_phy_t *phy = esp_eth_phy_new_yt8531(&phy_config);
```

### Configuring RGMII clock delays

The YT8531 has two stages of RX clock delay and one stage of TX clock delay that can be tuned
via the `YT8531_ETH_CMD_S_RGMII_CLK_DELAY` ioctl after the Ethernet driver is installed but
before it is started:

```c
yt8531_rgmii_clk_delay_config_t clk_delay = {
    .rxc_dly_en      = true,  // enable ~2 ns coarse RX delay
    .rx_delay_sel    = 0,     // fine RX delay: 0-15 steps × ~150 ps
    .tx_delay_sel    = 1,     // TX delay at 1000 Mbps: 0-15 steps × ~150 ps
    .tx_delay_sel_fe = 0xF,   // TX delay at 100/10 Mbps: 0-15 steps × ~150 ps
};
esp_eth_ioctl(eth_handle, YT8531_ETH_CMD_S_RGMII_CLK_DELAY, &clk_delay);
```

### Configuring far-end (remote UTP) loopback

The YT8531 far-end loopback control is exposed in `Misc_Config` (EXT `0xA006`) bit 5
(`Rem_lpbk_phy`). You can enable or disable it with the custom IOCTL
`YT8531_ETH_CMD_S_FAREND_LOOPBACK`:

```c
bool farend_loopback_en = true;
esp_eth_ioctl(eth_handle, YT8531_ETH_CMD_S_FAREND_LOOPBACK, &farend_loopback_en);
```

For more information on using the ESP-IDF Ethernet driver, visit the
[ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_eth.html).
