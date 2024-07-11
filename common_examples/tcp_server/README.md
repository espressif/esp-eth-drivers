# TCP server example

This example provides a basic implementation of TCP server for ESP32 with drivers from this repository.

## About the example
TCP server is designed to accept transmissions from the client, print them to the console and respond with another message "Transmission #XX. Hello from ESP32 TCP server".

## Configuring the example
Configure the example using `idf.py menuconfig`, according to your setup, provide settings for Ethernet initialization. If you want to connect to another ESP32, set the 
`Act as DHCP server` option in the `Example options`, each IP addres will be assigned an IP 192.168.n.1, where `n` is the number of the interface.

## Running the example
To transmit data between your PC and ESP32 you need to wait until it obtains an IP address from DHCP server and run tcp client script.

If you are connecting the device to a PC you will need to run a client script - the minimal command to do it is `tcp_client.py IP` and it will run until the script is stopped. Additional parameters are:
* `-c COUNT` to set the amount of transmission after which the client stops transmitting, terminates the connection and presents stats
* `-t TIME` to set periods between transmissions (default: 500ms)
* `-s` to run silently, without printing debug messages. In this mode count is set to 10 if not specified otherwise

You will see the ouput both in ESP32's console, and as the output of `tcp_client.py` if you have ran it.