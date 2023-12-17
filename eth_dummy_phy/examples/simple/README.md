| Supported Targets | ESP32 |
| ----------------- | ----- |

# Simple EMAC to EMAC Example

## Overview

This example demonstrates basic usage of the `Dummy PHY` component which enables you a direct **EMAC to EMAC** communication between two Ethernet capable devices without usage of external PHY transceiver. The devices EMACs are connected directly via RMII.

Output of the example are two applications with the following workflows:

1. The `RMII CLK Source Device` configured as DHCP Server:
    1. Wait for the `RMII CKL Sink Device` is ready, see [here](#interrupt-pin) for more information
    2. Install Ethernet driver and so enable RMII CLK output
    3. Attach the driver to `esp_netif`
    4. The DHCP Server assigns DHCP lease upon DHCP request from the `RMII CLK Sink Device`
    5. Reset itself when the `RMII CLK Sink Device` is reset

2. The `RMII CLK Sink Device` configured as DHCP Client:
    1. Notify the `RMII CKL Source Device` that we are ready with basic chip initialization (we've entered the `app_main()`)
    2. Install Ethernet driver (perform multiple attempts since the CLK may not be stable yet)
    3. Attach the driver to `esp_netif`
    4. Send DHCP requests and wait for a DHCP lease
    5. Get IP address and ping the `RMII CLK Source Device`

## How to use example

### Hardware Required

To run this example, you need one devkit which consists of [ESP32-WROOM module](https://www.espressif.com/en/products/modules/esp32) and one devkit which consists of [ESP32-WROVER module](https://www.espressif.com/en/products/modules/esp32) (or two ESP32-WROOM module devkits). The ESP32-WROOM module is used as source of the REF RMII CLK. Note that that **ESP32-WROVER can't be used as REF RMII CLK source** since the GPIO16 and GPIO17 (REF RMII CLK outputs) are utilized by SPIRAM.

#### Pin Assignment

Connect devkits' pins as shown in the table below. For demonstration purposes, you can interconnect them just by wires. However, the wires **must be** as short as possible (3-4 cm max!) and with matching length. It is also better to solder the wires to avoid loose contacts. Otherwise, you may get significantly over the [RMII specs.](https://resources.pcb.cadence.com/blog/2019-mii-and-rmii-routing-guidelines-for-ethernet) and face high packet loss or the setup may not work at all.

| RMII CLK Sink Device<br /> (ESP32 WROVER)  | | RMII CLK Source Device<br /> (ESP32 WROOM) | |
| ----- | ----- | ----- | ----- |
| **Func** | **GPIO** | **Func** | **GPIO** |
| RX[0] | GPIO25 | TX[0] | GPIO19 |
| RX[1] | GPIO26 | TX[1] | GPIO22 |
| TX[0] | GPIO19 | RX[0] | GPIO25 |
| TX[1] | GPIO22 | RX[1] | GPIO26 |
| TX_EN | GPIO21 | CRS_DV| GPIO27 |
| CRS_DV| GPIO27 | TX_EN | GPIO21 |
| CLK_IN | GPIO0 | CLK_O | GPIO17 |
| READY_IRQ | GPIO4 | IRQ_IN | GPIO4 |
| GND | GND | GND | GND |

##### Interrupt Pin

The Ready interrupt pin (`READY_IRQ`) is used to synchronize initialization of `RMII CLK Source Device` (ESP32-WROOM) with initialization of `RMII CLK Sink Device` (ESP32-WROVER). The `RMII CLK Source Device` needs to wait with its Ethernet initialization for the `RMII CLK Sink Device` since **the RMII CLK input pin of `RMII CLK Sink Device` (GPIO0) is also used as a boot strap pin for ESP32 chip**. If the `RMII CLK Source Device` didn't wait, the `RMII CLK Sink Device` could boot into incorrect mode. See the code for more information.

Note that there are more options of how to achieve the intended behavior by hardware means:
* You can keep the `RMII CKL Source Device` in reset state by pulling the EN pin low and enable it only after the `RMII CLK Sink Device` is ready (similarly how it's realized by [ESP32-Ethernet-Kit](https://docs.espressif.com/projects/esp-idf/en/latest/hw-reference/get-started-ethernet-kit.html) with PHY as a CLK source). However, make sure that there is still a way to handle EN pin for reset and flash operation. 
* Use external 50 MHz oscillator and enable it only after both ESP32 devices are ready.
* May not be needed at all if `RMII CLK Sink Device` is not ESP32.


### Configure the project

As mentioned above, the output of the example are two applications - the `RMII CLK Source Device` and the `RMII CLK Sink Device`. You can configure each application manually using

```
idf.py menuconfig
```

or build each application *automatically* based on provided `sdkconfig.defaults` files as follows:

```
idf.py -B "build_defaults_clk_source" -DSDKCONFIG="build_defaults_clk_source/sdkconfig" -DSDKCONFIG_DEFAULTS="sdkconfig.defaults.clk_source" build
```

and

```
idf.py -B "build_defaults_clk_sink" -DSDKCONFIG="build_defaults_clk_sink/sdkconfig" -DSDKCONFIG_DEFAULTS="sdkconfig.defaults.clk_sink" build
```


### Build, Flash, and Run

If you configured our applications manually using `menuconfig`, build the project and flash it to the board, then run monitor tool to view serial output:

```
idf.py -p PORT build flash monitor
```

If you built the applications based on on provided `sdkconfig.defaults` files, go to each `build` folder and flash each board by running:

```
python -m esptool --chip esp32 --port PORT -b 460800 --before default_reset --after hard_reset write_flash "@flash_args"
```

(Replace PORT with the name of the serial port to use.)

(To exit the serial monitor, type ``Ctrl-]``.)

See the [Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html) for full steps to configure and use ESP-IDF to build projects.

## Example Output

**The RMII CLK Source Device Output**

```bash
I (360) main_task: Started on CPU0
I (370) main_task: Calling app_main()
W (370) emac2emac: waiting for RMII CLK sink device interrupt
W (370) emac2emac: if RMII CLK sink device is already running, reset it by `EN` button
I (9300) emac2emac: starting Ethernet initialization
I (9310) esp_eth.netif.netif_glue: 94:e6:86:65:9b:27
I (9310) esp_eth.netif.netif_glue: ethernet attached to netif
I (9310) emac2emac: Ethernet Started
I (9320) emac2emac: Ethernet Link Up
I (9320) emac2emac: Ethernet HW Addr 94:e6:86:65:9b:27
I (9820) esp_netif_lwip: DHCP server assigned IP to a client, IP is: 192.168.4.2
```

**The RMII CLK Sink Device Output**

```bash
I (873) main_task: Returned from app_main()
I (873) emac2emac: Ethernet Started
I (883) emac2emac: Ethernet Link Up
I (893) emac2emac: Ethernet HW Addr e0:e2:e6:6a:7c:ff
I (2383) esp_netif_handlers: eth ip: 192.168.4.2, mask: 255.255.255.0, gw: 192.168.4.1
I (2383) emac2emac: Ethernet Got IP Address
I (2383) emac2emac: ~~~~~~~~~~~
I (2383) emac2emac: ETHIP:192.168.4.2
I (2393) emac2emac: ETHMASK:255.255.255.0
I (2393) emac2emac: ETHGW:192.168.4.1
I (2403) emac2emac: ~~~~~~~~~~~
64 bytes from 192.168.4.1 icmp_seq=1 ttl=255 time=1 ms
64 bytes from 192.168.4.1 icmp_seq=2 ttl=255 time=0 ms
64 bytes from 192.168.4.1 icmp_seq=3 ttl=255 time=0 ms
64 bytes from 192.168.4.1 icmp_seq=4 ttl=255 time=0 ms
64 bytes from 192.168.4.1 icmp_seq=5 ttl=255 time=0 ms
--- 192.168.4.1 ping statistics ---
5 packets transmitted, 5 received, 0% packet loss, time 1ms
```

## General Notes

* There is no speed/duplex auto-negotiation between devices, therefore both devices EMACs must be statically configured to the same speed/duplex. The example default configuration is 100Mbps/full duplex mode. If you need to change it, use `esp_eth_ioctl()` function with `ETH_CMD_S_SPEED` or `ETH_CMD_S_DUPLEX_MODE` command.  
