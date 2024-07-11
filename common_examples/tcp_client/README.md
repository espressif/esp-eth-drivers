# TCP client example

This example provides a basic implementation of TCP client for ESP32 with drivers from this repository

## About the example
TCP client is designed to periodically transmit to the server, and accept responses, which are printed to the console.

## Configuring the example
Configure the example using `idf.py menuconfig`, according to your setup, provide settings for Ethernet initialization. Next, go to `Example options` and set the `Server IP address`.

### Determining the server IP address
If you are running a TCP server on your PC using `tcp_server.py` - run a command to see your IP:
* Windows - `ipconfig /all`
* macOS/Linux - `ifconfig`

If you are running a TCP server on another ESP32 along with a DHCP server - see the output of your server for lines such as:
```
I (6349) tcp_server: --------
I (6359) tcp_server: Network Interface 0: 192.168.0.1
I (6359) tcp_server: Network Interface 1: 192.168.1.1
I (6369) tcp_server: --------
```
It will list all your network interfaces and the IP address assigned to it. Depending on the interface you are connected to - set the `Server IP address` accordingly.

## Running the example

After you obtain an IP address you will see a message reading `TCP client is started, waiting for the server to accept a connection.`

If you are connecting the device to a PC - start the server with `tcp_server.py`. The command to do so is `tcp_server.py IP` to start listening on the specified IP address.

**Important**: use the IP address of your PC in the local network to which ESP32 is connected. It **must** match the IP address you've set in the `Example options`.


You will see incoming messages in the ESP32 console or as an output of `tcp_server.py` if you are connecting the ESP32 to your PC and running a TCP server using a script.