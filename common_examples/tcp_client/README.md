# TCP Client Example

This example provides a basic implementation of TCP client for ESP32 SoC family with Ethernet drivers supported by Espressif.

## About the Example

TCP client is designed to periodically transmit *Hello message* to the server, and accept responses, which are printed to the console.

## Configuring the Example

Configure the example using `idf.py menuconfig`, according to your hardware setup, provide settings for Ethernet initialization in `Ethernet Configuration`. Next, go to `Example options` and set the `Server IP address` and `TCP Port number`.

### Determining the Server IP Address

If you are running a **TCP server** on your PC using [`tcp_server.py`](tcp_server.py) script - run the following command to see your PC IP address:
* Windows - `ipconfig /all`
* Linux - `ip`
* macOS - `ifconfig`

If you are running a **TCP server** on [another ESP32](../tcp_server/README.md) in the DHCP server configuration - see the output of the server for lines such as:
```
I (6349) tcp_server: --------
I (6359) tcp_server: Network Interface 0: 192.168.0.1
I (6359) tcp_server: Network Interface 1: 192.168.1.1
I (6369) tcp_server: --------
```
It will list all your network interfaces and the IP address assigned to it. Depending on the interface you are physically connected to - set the `Server IP address` accordingly.

## Build, Flash, and Run

Build the project and flash it to the board, then run monitor tool to view serial output:

```
idf.py -p PORT build flash monitor
```

(Replace PORT with the name of the serial port to use.)

(To exit the serial monitor, type ``Ctrl-]``.)

See the [Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html) for full steps to configure and use ESP-IDF to build projects.

### Running the Example

After you obtain an IP address, you will see a message reading `Trying to connect to server...`

If you are connecting the device to a PC - start the TCP server with `tcp_server.py`. The command to do so is `tcp_server.py <IP>` to start listening on the specified IP address.

You will see incoming messages in the ESP32 console or as an output of [`tcp_server.py`](tcp_server.py) if you are connecting the ESP32 to your PC and running a TCP server using a script.

> [!TIP]
> You can also use common protocol networking tools like `nc` (Netcat) or `socat` as TCP server if they suit you better.

> [!IMPORTANT]
> Use the IP address of your PC in the local network to which ESP32 is connected to. The address **must** match the IP address you've set in the `Example options`.

### Example output

```bash
I (353) main_task: Calling app_main()
I (553) ksz8851snl-mac: Family ID = 0x88         Chip ID = 0x7   Revision ID = 0x1
I (653) esp_eth.netif.netif_glue: 0e:b8:15:83:b0:10
I (653) esp_eth.netif.netif_glue: ethernet attached to netif
I (2553) ethernet_init: Ethernet(KSZ8851SNL[15,4]) Started
I (2553) ethernet_init: Ethernet(KSZ8851SNL[15,4]) Link Up
I (2553) ethernet_init: Ethernet(KSZ8851SNL[15,4]) HW Addr 0e:b8:15:83:b0:10
I (2553) tcp_client: Waiting for IP address...
I (16063) esp_netif_handlers: eth ip: 10.10.20.134, mask: 255.255.255.0, gw: 10.10.20.1
I (16063) tcp_client: Ethernet Got IP Address
I (16063) tcp_client: ~~~~~~~~~~~
I (16063) tcp_client: ETHIP:10.10.20.134
I (16063) tcp_client: ETHMASK:255.255.255.0
I (16073) tcp_client: ETHGW:10.10.20.1
I (16073) tcp_client: ~~~~~~~~~~~
I (16073) tcp_client: Trying to connect to server...
I (16083) tcp_client: Connecting to server 10.10.20.1:8080
E (16093) tcp_client: Failed to connect to server: errno 104
E (16093) tcp_client: Shutting down socket and restarting...
I (67573) tcp_client: Connected to server
I (67573) tcp_client: Sent transmission #1, 46 bytes
I (67573) tcp_client: Received 44 bytes: Transmission 1: Hello from Python TCP Server
```
