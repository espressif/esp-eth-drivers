# ENC28J60 Driver

---

## !!! Warning !!!

Espressif doesn't recommend using ENC28J60 Ethernet controller in new designs based on ESP32 series of chips. This is due to the following facts:
* ENC28J60 has low performance in half-duplex mode and various errata exist to the half-duplex mode.
* ENC28J60 does not support automatic duplex negotiation when configured to full-duplex mode.
* ENC28J60 has high current consumption - up to 180mA in comparison to e.g. 79mA of `W5500` or 75mA of `KSZ8851SNL` @ 10Mbps Tx.

Therefore, we rather recommend using `W5500`, `KSZ8851SNL` or `DM9051`, which are also supported in ESP-IDF.

---

## Overview

The Microchip ENC28J60 is a standalone Ethernet controller that operates at 10 Mbps and communicates with microcontrollers via the SPI interface. It includes a built-in MAC and PHY layer and an internal 8KB RAM buffer for packet storage. The chip operates at 3.3V with 5V tolerant I/O pins and supports the IEEE 802.3 Ethernet standard.

## ESP-IDF Usage

Add this component from [IDF Component Manager](https://components.espressif.com/) to your project using `idf.py add-dependency` and include `esp_eth_enc28j60.h`,

```c
#include "esp_eth_enc28j60.h"
```

Configure SPI interface for specific SPI module, see [Notes](#notes) section:

```c
    spi_device_interface_config_t spi_devcfg = {
        .mode = 0,
        .clock_speed_hz = CONFIG_ETHERNET_SPI_CLOCK_MHZ * 1000 * 1000,
        .queue_size = 20,
        .spics_io_num = CONFIG_ETHERNET_SPI_CS_GPIO_NUM
    };
    spi_devcfg.cs_ena_posttrans = enc28j60_cal_spi_cs_hold_time(CONFIG_ETHERNET_SPI_CLOCK_MHZ);
```

Create a `mac` driver instance by calling `esp_eth_mac_new_enc28j60()` and check if your silicon revision support configured SPI frequency:

```c
    eth_enc28j60_config_t enc28j60_config = ETH_ENC28J60_DEFAULT_CONFIG(CONFIG_ETHERNET_SPI_HOST, &spi_devcfg);
    enc28j60_config.int_gpio_num = spi_eth_module_config->int_gpio;
    esp_eth_mac_t *mac = esp_eth_mac_new_enc28j60(&enc28j60_config, &mac_config);

    // ENC28J60 Errata #1 check
    ESP_GOTO_ON_FALSE(mac, NULL, err, TAG, "creation of ENC28J60 MAC instance failed");
    ESP_GOTO_ON_FALSE(emac_enc28j60_get_chip_info(mac) >= ENC28J60_REV_B5 || CONFIG_ETHERNET_SPI_CLOCK_MHZ >= 8,
                      NULL, err, TAG, "SPI frequency must be at least 8 MHz for chip revision less than 5");
```

Create a `phy` driver instance by calling `esp_eth_phy_new_enc28j60()`:

```c
    phy_config.autonego_timeout_ms = 0; // ENC28J60 doesn't support auto-negotiation
    phy_config.reset_gpio_num = -1; // ENC28J60 doesn't have a pin to reset internal PHY
    esp_eth_phy_t *phy = esp_eth_phy_new_enc28j60(&phy_config);
```

and use the Ethernet driver as you are used to. For more information of how to use ESP-IDF Ethernet driver, visit [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_eth.html).

### Hardware Required

To be able to use ENC28J60 with ESP32 series chips, you need to prepare following hardware:
* Any [ESP32 dev board](https://www.espressif.com/en/products/devkits) (e.g. ESP32-PICO, ESP32 DevKitC, etc)
* ENC28J60 Ethernet module (the latest revision should be 6)
* **!! IMPORTANT !!** Proper input power source since ENC28J60 is quite power consuming device (it consumes more than 200 mA in peaks when transmitting). If improper power source is used, input voltage may drop and ENC28J60 may either provide nonsense response to host controller via SPI (fail to read registers properly) or it may enter to some strange state in the worst case. There are several options how to resolve it:
  * Power ESP32 dev board from `USB 3.0`, if the dev board is used as source of power to the ENC28J60 module.
  * Power ESP32 dev board from external 5 V power supply with current limit at least 1 A, if the dev board is used as source of power to the ENC28J60 module.
  * Power ENC28J60 from external 3.3 V power supply with common GND to ESP32 dev board. Note that there might be some ENC28J60 modules with integrated voltage regulator on market and so powered by 5 V. Please consult documentation of your board for details.

  If an ESP32 dev board is used as the source of power to the ENC28J60 module, ensure that that the particular dev board is assembled with a voltage regulator capable to deliver current of 1 A. This is a case of ESP32-DevKitC or ESP-WROVER-KIT, for example. Such setup was tested and works as expected. Other dev boards may use different voltage regulators and may perform differently.
  **WARNING:** Always consult documentation/schematics associated with particular ENC28J60 and ESP32 dev boards used in your use-case first.

## Notes

1. ENC28J60 hasn't burned any valid MAC address in the chip, you need to write an unique MAC address into its internal MAC address register before any traffic happened on TX and RX line.
2. It is recommended to operate the ENC28J60 in full-duplex mode since various errata exist to the half-duplex mode (even though addressed in the driver component) and due to its poor performance in the half-duplex mode (especially in TCP connections). However, ENC28J60 does not support automatic duplex negotiation. If it is connected to an automatic duplex negotiation enabled network switch or Ethernet controller, then ENC28J60 will be detected as a half-duplex device. To communicate in Full-Duplex mode, ENC28J60 and the remote node (switch, router or Ethernet controller) **must be manually configured for full-duplex operation**:
   * The ENC28J60 can be set to full-duplex by `esp_eth_ioctl` using `ETH_CMD_S_DUPLEX_MODE` command.
   * On Ubuntu/Debian Linux distribution use:
    ```
    sudo ethtool -s YOUR_INTERFACE_NAME speed 10 duplex full autoneg off
    ```
   * On Windows, go to `Network Connections` -> `Change adapter options` -> open `Properties` of selected network card -> `Configure` -> `Advanced` -> `Link Speed & Duplex` -> select `10 Mbps Full Duplex in dropdown menu`.
3. Ensure that your wiring between ESP32 dev board and the ENC28J60 module is realized by short wires with the same length and no wire crossings.
4. According to ENC28J60 data sheet and our internal testing, SPI clock could reach up to 20MHz, but in practice, the clock speed may depend on your PCB layout/wiring/power source. Note that some ENC28J60 silicon revisions may not properly work at frequencies less than 8 MHz!
5. CS Hold Time needs to be configured to be at least 210 ns to properly read MAC and MII registers as defined by ENC28J60 Data Sheet. This can automatically set by `enc28j60_cal_spi_cs_hold_time` function based on selected SPI clock frequency by computing amount of SPI bit-cycles the CS should stay active after the transmission. However, if your PCB design/wiring requires different value, please update `cs_ena_posttrans` member of `devcfg` structure per your actual needs.

## Troubleshooting

(For any technical queries, please open an [issue](https://github.com/espressif/esp-idf/issues) on GitHub. We will get back to you as soon as possible.)
