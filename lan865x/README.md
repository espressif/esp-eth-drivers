# Microchip LAN865x Ethernet MAC-PHY Driver

## Overview

The LAN8650/1 combines a Media Access Controller (MAC) and an Ethernet PHY to enable microcontrollers, including those without an on-board MAC, to access 10BASE‑T1S networks. The common standard Serial Peripheral Interface (SPI) of the LAN8650/1 allows interfacing with ESP32-S, ES32-C, or ESP32-H SoC series, so that the transfer of Ethernet packets and LAN8650/1 control/status commands are performed over a single, serial interface. SPI also requires only 4 pins, enabling a simpler hardware interface with fewer pins than MII or RMII.

Ethernet packets are segmented and transferred over the serial interface according to the OPEN Alliance 10BASE‑T1x MAC‑PHY Serial Interface specification.

More information about the chip can be found on the product page: [LAN8650](https://www.microchip.com/en-us/product/lan8650), [LAN8651](https://www.microchip.com/en-us/product/lan8651).

## Hardware Required

To use this driver, you need any ESP32 SoC with SPI interface and either develop your custom board or use any available development board. 

### MICROE Two-Wire ETH Click Board Modification

If you decide to use [Two-Wire ETH Click](https://www.microchip.com/en-us/development-tool/MIKROE-5543) module as offered at the official Microchip webpage, this board requires a modification to be fully functional with ESP32 series SPI interface. The board includes a red signaling LED that monitors the CS line status. This LED is driven by a transistor (Q1), which affects the CS signal's timing characteristics. Specifically, the transistor causes the CS signal's fall time to be too slow, which can interfere with SPI transaction timing. To resolve this issue, you need to de-solder transistor Q1, which is located to the right of the red LED.

## ESP-IDF Usage

> [!TIP]
> Use [Component for Ethernet Initialization](https://components.espressif.com/components/espressif/ethernet_init) for smoother user experience.

Add this component from [IDF Component Manager](https://components.espressif.com/) to your project using `idf.py add-dependency` and include `esp_eth_mac_lan865x.h` and `esp_eth_phy_lan865x.h`,

```c
    #include "esp_eth_mac_lan865x.h"
    #include "esp_eth_phy_lan865x.h"
```

initialize SPI bus, and create `mac` and `phy` driver instance by calling `esp_eth_mac_new_lan865x` and `esp_eth_phy_new_lan865x()`

```c
    // Install GPIO interrupt service (as the SPI-Ethernet module is interrupt-driven)
    gpio_install_isr_service(0);
    // SPI bus configuration
    spi_device_handle_t spi_handle = NULL;
    spi_bus_config_t buscfg = {
        .miso_io_num = CONFIG_EXAMPLE_ETH_SPI_MISO_GPIO,
        .mosi_io_num = CONFIG_EXAMPLE_ETH_SPI_MOSI_GPIO,
        .sclk_io_num = CONFIG_EXAMPLE_ETH_SPI_SCLK_GPIO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(CONFIG_EXAMPLE_ETH_SPI_HOST, &buscfg, 1));
    // Configure SPI device
    spi_device_interface_config_t spi_devcfg = {
        .mode = 0,
        .clock_speed_hz = CONFIG_EXAMPLE_ETH_SPI_CLOCK_MHZ * 1000 * 1000,
        .spics_io_num = CONFIG_EXAMPLE_ETH_SPI_CS_GPIO,
        .queue_size = 20
    };

    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();

    eth_lan865x_config_t lan865x_config = ETH_LAN865X_DEFAULT_CONFIG(CONFIG_EXAMPLE_ETH_SPI_HOST, &spi_devcfg);

    dev_out->mac = esp_eth_mac_new_lan865x(&lan865x_config, &mac_config);
    dev_out->phy = esp_eth_phy_new_lan865x(&phy_config);
```

## Collision avoidance methods: CSMA/CD and PLCA

In most cases you can rely on the default CSMA/CD (Carrier Sense Multiple Access / Collision Detection) provided by the MAC layer to avoid collisions when connecting multiple nodes. It requires zero configuration to work and does a good job of coordinating when data is sent. However, due to its nature, the delays between transmissions are random. For timing-critical applications, deterministic PLCA (PHY-Level Collision Avoidance) provides a guaranteed time period between transmissions along with improved throughput. Read more about PLCA and its usage in the [PLCA FAQ](https://www.ieee802.org/3/cg/public/July2018/PLCA%20FAQ.pdf).

### Configuring PLCA

PLCA configuration is performed using the `esp_eth_ioctl` function with one of the commands described in [LAN86XX PHY Sub-component](../lan86xx_common/README.md).


