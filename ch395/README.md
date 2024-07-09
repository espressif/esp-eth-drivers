# WCH CH395Q Ethernet Driver (beta)

## Overview

CH395 is an Ethernet protocol stack management IC which provides Ethernet communication ability for MCU system. CH395 has built-in 10 / 100M Ethernet MAC and PHY and fully compatible with IEEE 802.3 protocol, it also has built-in protocol stack firmware such as PPPOE, IP, DHCP, ARP, ICMP, IGMP, UDP, TCP and etc. A MCU system can easily connect to Ethernet through CH395. CH395 supports 3 types of communication interfaces: 8-bit parallel port, SPI or USART. A MCU/DSP/MPU can use any of the above communication interfaces to operate CH395 for Ethernet communication. More information about the chip can be found in the product [datasheets](https://www.wch-ic.com/downloads/CH395DS1_PDF.html).

## ESP-IDF Usage
### Import
Add this component from [IDF Component Manager](https://components.espressif.com/) to your project using `idf.py add-dependency` and include `esp_eth_phy_ch395.h` and `esp_eth_mac_ch395.h`,

```c
#include "esp_eth_phy_ch395.h"
#include "esp_eth_mac_ch395.h"
```

### Configuration
SPI and UART are both supported by CH395 and this driver. For CH395, communication mode is determined by the level of `SEL` and `TXD` pin when the chip power-on and reset(as the table shows below).

| SEL | TXD | Mode |
|:---:|:---:|:----:|
|  1  |  1  | UART |
|  1  |  0  | SPI  |

Both `TXD` and `SEL` pin are internally pull-up.

For this driver, two modes can be switched between via `CH395 Communication Interface` in `menuconfig`. 

<img src="./docs/image/ModeSelection.png" width="60%">

#### 1. UART Mode(recommend)
A large number of test results have shown that: current version of driver exhibits much **BETTER** performance in UART mode. Just create a configuration instance by calling `ETH_CH395_DEFAULT_CONFIG`
```c
// Configure UART interface for specific UART module
uart_config_t uart_devcfg={
    .baud_rate = CONFIG_TCPSERVER_ETH_UART_BAUDRATE,
    .data_bits = UART_DATA_8_BITS,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    .parity = UART_PARITY_DISABLE,
};

eth_ch395_config_t ch395_config = ETH_CH395_DEFAULT_CONFIG(CONFIG_TCPSERVER_ETH_UART_PORT, &uart_devcfg); 
ch395_config.uart_tx_gpio_num = CONFIG_TCPSERVER_ETH_UART_TX_GPIO;
ch395_config.uart_rx_gpio_num = CONFIG_TCPSERVER_ETH_UART_RX_GPIO;
ch395_config.int_gpio_num = CONFIG_TCPSERVER_ETH_INT_GPIO;
```
It is worth noting that the initial baudrate of CH395 is determined by level of `SDO`, `SDI` and `SCK` pins when the chip power-on and reset(as the table shows below).

| SDO | SDI | SCK | Baudrate(bps) |
|:---:|:---:|:---:|:-------------:|    
|  1  |  1  |  1  |     9600      |
|  1  |  1  |  0  |     57600     |     
|  1  |  0  |  1  |    115200     |     
|  1  |  0  |  0  |    460800     |     
|  0  |  1  |  1  |    250000     |     
|  0  |  1  |  0  |    1000000    |     
|  0  |  0  |  1  |    3000000    |     
|  0  |  0  |  0  |    921600     |      

All of these pins are internally pull-up. We highly recommend you choose a baudrate higher than **460800 bps** to prevent certain operations from taking too long to trigger timeout errors. 

In general, the baudrate you set to the chip should be the same as that you configured in the program. This driver also additionally provides flexibility in baudrate. If you leave all of these pins floating, you can set **whatever baudrate** you want. This driver would automatically doing configuration.

**However, it could also lead to an ERROR if you flash new code that uses different baudrate without cold reboot the chip.** The solution is quite simple, **COLD REBOOT**!

#### 2. SPI Mode

create a configuration instance by calling `ETH_CH395_DEFAULT_CONFIG`

```c
// Configure SPI interface for specific SPI module
spi_device_interface_config_t spi_devcfg = {
    .mode = 0,
    .clock_speed_hz = CONFIG_TCPSERVER_ETH_SPI_CLOCK_MHZ * 1000 * 1000,
    .queue_size = 16,
    .spics_io_num = -1
};

eth_ch395_config_t ch395_config = ETH_CH395_DEFAULT_CONFIG(CONFIG_TCPSERVER_ETH_SPI_HOST, &spi_devcfg);
ch395_config.int_gpio_num = CONFIG_TCPSERVER_ETH_INT_GPIO;
ch395_config.spi_cs_gpio_num = CONFIG_TCPSERVER_ETH_SPI_CS_GPIO;
```

**DO NOT** pass CS pin number into `spi_devcfg`, because the driver need to manually control the CS pin. Instead, assign it to `spi_cs_gpio_num` of `ch395_config`.

### Create MAC and PHY instance
create a `mac` driver instance by calling `esp_eth_mac_new_ch395`,

```c
eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();

// To avoid stack overflow, you could choose a larger value
mac_config.rx_task_stack_size = 4096;

// CH390 has a factory burned MAC. Therefore, configuring MAC address manually is optional.     
esp_eth_mac_t *mac = esp_eth_mac_new_ch395(&ch395_config,&mac_config);
```

create a `phy` driver instance by calling `esp_eth_phy_new_ch395()`.


```c
eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();

esp_eth_phy_t *phy = esp_eth_phy_new_ch395(&phy_config);
```

and use the Ethernet driver as you are used to. For more information of how to use ESP-IDF Ethernet driver, visit [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_eth.html).