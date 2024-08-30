# WCH CH182 Ethernet Driver (beta)

## Overview

CH182 is an industrial 10/100M Ethernet PHY transceiver that supports Auto-MDIX. CH182 includes modules required for Ethernet Transceiver functions, such as physical coding sublayer (PCS), physical medium access layer (PMA), twisted pair physical medium dependent sublayer (TP-PMD), 10BASE-TX encoder/decoder, twisted pair medium connection unit (TPMAU), MII and RMII interfaces. More information about the chip can be found in the product [datasheets](https://www.wch.cn/downloads/CH182DS1_PDF.html).

## ESP-IDF Usage

Add this component from [IDF Component Manager](https://components.espressif.com/) to your project using `idf.py add-dependency` and include `esp_eth_phy_ch182.h`.

```c
#include "esp_eth_phy_ch182.h"
```

Create a `phy` default configuration.

```c
eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
```

Create a `phy` driver instance by calling `esp_eth_phy_new_ch182() or esp_eth_phy_new_ch182_use_esp_refclk()`.

#### 1. REFCLK from CH182

If REFCLK comes from CH182(as you would by default with most other PHYs), call this default function, which is similar to other drivers.
```c
esp_eth_phy_t *phy = esp_eth_phy_new_ch182(&phy_config);
```

#### 2. REFCLK from ESP32
If REFCLK comes from ESP32, you should firstly also change `RMII clock mode` in `menuconfig`, then call this special function.
```c
esp_eth_phy_t *phy = esp_eth_phy_new_ch182_use_esp_refclk(&phy_config);
```

and use the Ethernet driver as you are used to. For more information of how to use ESP-IDF Ethernet driver, visit [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_eth.html).

