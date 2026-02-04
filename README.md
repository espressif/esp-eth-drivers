# Collection of Ethernet drivers for ESP-IDF

This repository aims to store Ethernet drivers which are not available directly in [ESP-IDF](https://github.com/espressif/esp-idf) but are accessible via [The ESP Component Registry](https://components.espressif.com/).

List of currently supported chips:

Ethernet PHYs:
- [ADIN1200](adin1200/README.md)
- [DP83848](dp83848/README.md)
- [IP101](ip101/README.md)
- [KSZ80XX](ksz80xx/README.md)
- [LAN87XX](lan87xx/README.md)
- [LAN867X](lan867x/README.md) (10BASE-T1S)
- [RTL8201](rtl8201/README.md)

SPI Ethernet modules:
- [CH390](ch390/README.md)
- [DM9051](dm9051/README.md)
- [ENC28J60](enc28j60/README.md)
- [KSZ8851SNL](ksz8851snl/README.md)
- [LAN865X](lan865x/README.md) (10BASE-T1S)
- [W5500](w5500/README.md)
- [W6100](w6100/README.md)

Switch ICs:
- [KSZ8863](ksz8863/README.md)

Special:
- [Dummy PHY (EMAC to EMAC)](eth_dummy_phy/README.md)

## Resources

[IDF Component Manager](https://github.com/espressif/idf-component-manager) repository.

## Contribution

We welcome all contributions to the drivers and Component Manager project.

You can contribute by fixing bugs, adding features, adding documentation or reporting an [issue](https://github.com/espressif/esp-eth-drivers/issues). We accept contributions via [Github Pull Requests](https://docs.github.com/en/pull-requests/collaborating-with-pull-requests/proposing-changes-to-your-work-with-pull-requests/about-pull-requests). See the [project Contributing Guide](CONTRIBUTING.md) for more information.

Before reporting an issue, make sure you've searched for a similar one that was already created.
