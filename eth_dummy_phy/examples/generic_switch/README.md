| Supported Targets | ESP32 |
| ----------------- | ----- |

# EMAC to EMAC Example - Generic Switch

## Overview

This example demonstrates usage of the `Dummy PHY` component in combination with an integrated multiple-port switch Integrated Circuit (IC). The idea is the majority of switch ICs boots to default "unmanaged" switch state (or they can be configured by bootstrap pins). Host CPU can be then connected to the switch in MAC-MAC fashion with no need for any other additional configuration. The devices are connected directly via RMII.

## How to use example

### Hardware Required

To run this example, you need TODO


### Configure the project

TODO


### Build, Flash, and Run

TODO

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

* There is no speed/duplex auto-negotiation between devices, therefore both devices EMACs must be statically configured to the same speed/duplex. The example default configuration is 100Mbps/full duplex mode. If you need to change it, use `esp_eth_ioctl()` function with `ETH_CMD_S_SPEED` or `ETH_CMD_S_DUPLEX_MODE` command.  
