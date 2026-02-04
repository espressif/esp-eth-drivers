# Ethernet PHY Tester

**!!!WORK IN PROGRESS!!!**

The Ethernet PHY tester is a tool to help you debug issues with Ethernet PHY. It's especially handy when you're in process of bringing up a custom board with Ethernet and you face issues.

## Features

* PHY Registry dump
* PHY Registry write
* Near-end loopback test
* Far-end loopback test
* Ethernet L2 loopback server
* Dummy Ethernet frames transmitter

## How to use

1) Configure PHY and Ethernet component based on your actual board's needs in sdkconfig.
2) Build & Flash.
3) Connect to the board and run commands from console (type `help` to list available commands).

or run automatic test to identify Ethernet related issue of your board:

1) Configure PHY and Ethernet component based on your actual board's needs in sdkconfig.
2) Build.
3) Find your network interface (NIC) name the DUT is connected to (e.g. use `ip` command).
4) Run:

    `pytest -s --embedded-services esp,idf --target esp32 --eth-nic YOUR_NIC_NAME` (change target and NIC per your setup!).

    If this command doesn't work (you are probably on older IDF version), try to run:

    `pytest -s --embedded-services esp,idf --tb=no --add-target-as-marker y -m esp32 --eth-nic YOUR_NIC_NAME`

Note: you need `pytest` installed, see [pytest in ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/contribute/esp-idf-tests-with-pytest.html) for more information.

## Known Limitations

* The `pytest_eth_phy.py` can be run on Linux only and you need to:
  * have root permission to run the test app (since RAW socket is used)
  * or set correct capacity through ``sudo setcap 'CAP_NET_RAW+eip' $(readlink -f $(which python))``

## Example of pytest Output
```
loopback server: PASS
. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .
. DUT                                                                                   .
.                          RMII                             Tr.                         .
.  +------------------+            +---------------+                  +-----------+     .     +---------------+
.  |              +---|            |               |        8|8       |           |     .     |               |
.  |          .-<-| M |-----Rx-----|-----------<---|------- 8|8 ------|           |     .     |               |
.  |   ESP32  |   | A |            |      PHY      |                  |    RJ45   |===========|    Test PC    |
.  |          '->-| C |-----Tx-----|----------->---|------- 8|8 ------|           |     .     |               |
.  |              +---|            |               |        8|8       |           |     .     |               |
.  +------------------+            +---------------+                  +-----------+     .     +---------------+
.                                                                                       .
. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .
```
