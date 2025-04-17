# TCP Server Example

This example provides a basic implementation of TCP server for ESP32 SoC family with Ethernet drivers supported by Espressif.

## About the Example

TCP server is designed to accept transmissions from the client, print them to the console and respond with another message "Transmission #XX. Hello from ESP32 TCP server".

## Configuring the Example

Configure the example using `idf.py menuconfig`, according to your hardware setup, provide settings for Ethernet initialization in `Ethernet Configuration`.

If you want to connect multiple ESP32 devices in one network without a router or any other device which would host DHCP server, set the example to `Act as DHCP server` in the `Example options`. Each device IP address will then be assigned in `192.168.n.0/24` network address space, where `n` is the number of the interface.

> [!TIP]
> Configuring example to act as DHCP server can be particularly useful when you are testing 10BASE-T1S devices such as LAN867x.

## Build, Flash, and Run

Build the project and flash it to the board, then run monitor tool to view serial output:

```
idf.py -p PORT build flash monitor
```

(Replace PORT with the name of the serial port to use.)

(To exit the serial monitor, type ``Ctrl-]``.)

See the [Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html) for full steps to configure and use ESP-IDF to build projects.

### Running the example

To transmit data between your PC and ESP32 you need to wait until it obtains an IP address from DHCP server and run tcp client script.

If you are connecting the device to a PC you will need to run a client script - the minimal command to do it is `tcp_client.py <IP>`. `IP` is TCP server address. Script will run until it is stopped by user. Additional parameters are:
* `-c <COUNT>` to set the amount of transmission after which the client stops transmitting, terminates the connection and presents stats
* `-i <INTERVAL>` to set interval between each transmission in msec (default: 1000ms)
* `-s` to run silently, without printing debug messages. In this mode count is set to 10 if not specified otherwise

You will see the output both in ESP32's console, and as the output of [`tcp_client.py`](tcp_client.py) if you have ran it.

> [!TIP]
> You can also use common protocol networking tools like `nc` (Netcat) or `socat` as TCP client if they suit you better.

### Example output

```bash
I (359) main_task: Calling app_main()
I (559) ksz8851snl-mac: Family ID = 0x88         Chip ID = 0x7   Revision ID = 0x1
I (659) esp_eth.netif.netif_glue: 0e:b8:15:83:b0:10
I (659) esp_eth.netif.netif_glue: ethernet attached to netif
I (2659) ethernet_init: Ethernet(KSZ8851SNL[15,4]) Started
I (2659) ethernet_init: Ethernet(KSZ8851SNL[15,4]) Link Up
I (2659) ethernet_init: Ethernet(KSZ8851SNL[15,4]) HW Addr 0e:b8:15:83:b0:10
I (2669) tcp_server: Server listening on port 8080
I (3669) esp_netif_handlers: eth0 ip: 10.10.20.134, mask: 255.255.255.0, gw: 10.10.20.1
I (3669) tcp_server: Ethernet Got IP Address
I (3669) tcp_server: ~~~~~~~~~~~
I (3669) tcp_server: ETHIP:10.10.20.134
I (3669) tcp_server: ETHMASK:255.255.255.0
I (3679) tcp_server: ETHGW:10.10.20.1
I (3679) tcp_server: ~~~~~~~~~~~
I (10749) tcp_server: New connection accepted from 10.10.20.1:55830, socket fd: 55
I (10749) tcp_server: Received 18 bytes from 10.10.20.1
Hello from netcat!
```
