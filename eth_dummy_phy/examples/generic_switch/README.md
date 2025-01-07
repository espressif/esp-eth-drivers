| Supported Targets | ESP32 | ESP32P4 |
| ----------------- | ----- | ------- |

# EMAC to EMAC Example - Generic Unmanaged Switch

## Overview

This example demonstrates the usage of the `Dummy PHY` component in combination with an integrated multiple-port switch Integrated Circuit (IC). The idea is that the majority of switch ICs boot to a default "unmanaged" switch state (or they can be configured by bootstrap pins). The host CPU can then be connected to the switch in MAC-MAC fashion with no need for any additional configuration - the devices are connected directly via RMII without a management interface (SMI).

## How to Use This Example

### Hardware Required

To run this example, you need an ESP32 SoC with internal EMAC capability (ESP32 or ESP32P4) and an integrated multiple-port switch IC of your choice. You need to interconnect only the RMII data plane signals between the SoC and the multiple-port switch IC, along with the RMII REFCLK source. No control plane connection is needed for unmanaged mode. Note that if your chosen integrated multiple-port switch IC supports any other management interface such as I2C, you can implement your own device management independently of the `esp_eth` driver.

> [!NOTE]
> This example has been tested using [KSZ8863 Test Board](../../../ksz8863/docs/test_board/).

### Configure the project

Configure the example using `idf.py menuconfig`, according to your hardware setup.

#### Example Configuration Options

The following configuration options are available in the `Example Configuration` menu:

- **PHY Reset GPIO number** (`EXAMPLE_PHY_RST_GPIO`): Set the GPIO number used to reset PHY chip. Set to -1 to disable PHY chip hardware reset. Default: 5

- **Enable external RMII clock oscillator** (`EXAMPLE_EXTERNAL_CLK_EN`): Enable external clock oscillator on the test board. Default: enabled

- **External oscillator enable GPIO** (`EXAMPLE_EXTERNAL_CLK_EN_GPIO`): Set GPIO number to enable External clock oscillator. Only available when external RMII clock oscillator is enabled. Default: 2

For more information, see help associated with each option in ESP-IDF Configuration tool.

### Build, Flash, and Run

Build the project and flash it to the board, then run monitor tool to view serial output:

```
idf.py -p PORT build flash monitor
```

(Replace PORT with the name of the serial port to use.)

(To exit the serial monitor, type ``Ctrl-]``.)

See the [Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html) for full steps to configure and use ESP-IDF to build projects.

## Example Output

```bash
I (348) main_task: Returned from app_main()
I (348) generic_switch: Ethernet Started
I (358) generic_switch: Ethernet Link Up
I (358) generic_switch: Ethernet HW Addr 24:d7:eb:b9:03:9f
I (5858) esp_netif_handlers: eth ip: 10.10.10.116, mask: 255.255.255.0, gw: 10.10.10.1
I (5858) generic_switch: Ethernet Got IP Address
I (5858) generic_switch: ~~~~~~~~~~~
I (5858) generic_switch: ETHIP:10.10.10.116
I (5858) generic_switch: ETHMASK:255.255.255.0
I (5868) generic_switch: ETHGW:10.10.10.1
I (5868) generic_switch: ~~~~~~~~~~~
```

## General Notes

* There is no speed/duplex auto-negotiation between devices, therefore both ESP32 EMAC and switch EMAC must be statically configured to the same speed/duplex. The example default configuration is 100Mbps/full duplex mode. If you need to change it, use `esp_eth_ioctl()` function with `ETH_CMD_S_SPEED` or `ETH_CMD_S_DUPLEX_MODE` command.  
