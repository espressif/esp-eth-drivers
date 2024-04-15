# Iperf example

This example provides a simple way to measure network performance using iperf.

## About the example
The example uses `iperf` and `iperf-cmd` components for an iperf implementation and provides command line interface for it. It provides DHCP server functionality for connecting to another ESP32, instead of a PC.

## Configuring the example
Using `idf.py menuconfig` set up the Ethernet configuration, in the `Example option` you can enable DHCP server with `Act as a DHCP server`. This will make ESP32 run an instance of DHCP server per interface assignign IP addresses in the subnet 192.168.1.0/24.

## Running the example
You will see `esp>` prompt appear in ESP32 console. Run `iperf -h` to see iperf command options.
