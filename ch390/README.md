# WCH CH390H/D Ethernet Driver

## Overview

CH390 is an industrial-grade Ethernet controller IC with 10/100M Ethernet Media Transport Layer (MAC) and Physical Layer Transceiver (PHY), supporting CAT3, 4, 5 and CAT5, and 6 connections for 10BASE-T and 100BASE-TX, supporting HP Auto-MDIX, low power design, and IEEE 802.3u compliant. The CH390 has a built-in 16K byte SRAM, and supports 3.3V or 2.5V parallel interface and SPI serial interface. More information about the chip can be found in the product [datasheets](https://www.wch.cn/downloads/CH390DS1_PDF.html).

## ESP-IDF Usage

Add this component from [IDF Component Manager](https://components.espressif.com/) to your project using `idf.py add-dependency` and include `esp_eth_phy_ch390.h` and `esp_eth_mac_ch390.h`,

```c
#include "esp_eth_phy_ch390.h"
#include "esp_eth_mac_ch390.h"
```

create a configuration instance by calling `ETH_CH390_DEFAULT_CONFIG`,

```c
// Configure SPI interface for specific SPI module
spi_device_interface_config_t spi_devcfg = {
    .mode = 0,
    .clock_speed_hz = CONFIG_TCPSERVER_ETH_SPI_CLOCK_MHZ * 1000 * 1000,
    .queue_size = 16,
    .spics_io_num = CONFIG_TCPSERVER_ETH_SPI_CS_GPIO
};

eth_ch390_config_t ch390_config = ETH_CH390_DEFAULT_CONFIG(CONFIG_TCPSERVER_ETH_SPI_HOST,&spi_devcfg);
ch390_config.int_gpio_num = CONFIG_TCPSERVER_ETH_SPI_INT_GPIO;

#if CONFIG_TCPSERVER_ETH_SPI_INT_GPIO < 0
ch390_config.poll_period_ms = CONFIG_TCPSERVER_ETH_SPI_POLLING_MS_VAL;
#endif
```

create a `mac` driver instance by calling `esp_eth_mac_new_ch390`,

```c
eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();

// To avoid stack overflow, you could choose a larger value
mac_config.rx_task_stack_size = 4096;

// CH390 has a factory burned MAC. Therefore, configuring MAC address manually is optional.     
esp_eth_mac_t *mac = esp_eth_mac_new_ch390(&ch390_config,&mac_config);
```

create a `phy` driver instance by calling `esp_eth_phy_new_ch390()`.


```c
eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();

esp_eth_phy_t *phy = esp_eth_phy_new_ch390(&phy_config);
```

and use the Ethernet driver as you are used to. For more information of how to use ESP-IDF Ethernet driver, visit [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_eth.html).


## Version History
| **Version** | **Date**   | **Description**                                                                                                        |
|:-----------:|:----------:|--------------------------------------------------------------------------------------------------------------------    |
| 0.1.0       | 2024-03-06 | Initial Release                                                                                                        |
| 0.2.0       | 2024-08-29 | Fix start/stop issue: cannot acquire the ip address after several start/stop loops randomly                            |
| 0.2.1       | 2024-11-25 | Correct some typos in comment; Fix the issue that loopback and auto-negotiation cannot be enabled at the same time     |
| 0.2.2       | 2025-2-14  | Add support of CH390D; Remove beta symbol                                                                              |

## Acknowledgement
In no particular order:
- ***kostaond**
- ***leeebo**
- igrr
- bogdankolendovskyy
- ShunzDai

*Gives **TREMENDOUS** support to the project
