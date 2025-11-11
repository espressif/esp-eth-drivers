# Changelog

## [1.2.0](https://github.com/espressif/esp-eth-drivers/compare/ethernet_init@v1.1.0...ethernet_init@v1.2.0) (2025-11-11)


### Features

* **ethernet_init:** streamlined conditional dependencies evaluation ([6645db3](https://github.com/espressif/esp-eth-drivers/commit/6645db345e638b50858e5672663f85e6e088c492))


### Bug Fixes

* **ethernet_init:** fixed build when no Ethernet selected ([da6ad85](https://github.com/espressif/esp-eth-drivers/commit/da6ad850b6901995aa0c4c2cd685f058cc26928e))

## [1.1.0](https://github.com/espressif/esp-eth-drivers/compare/ethernet_init@v1.0.0...ethernet_init@v1.1.0) (2025-10-17)


### Features

* **ethernet_init:** added ETHERNET_INIT_OVERRIDE_DISABLE, ETHERNET_INIT_DEFAULT_ETH_DISABLED ([10ebd4c](https://github.com/espressif/esp-eth-drivers/commit/10ebd4c08d7a9a448576a1e2cd113101eec3a7b8))
* **ethernet_init:** added OPENETH, extended skdconfig default behavior ([10ebd4c](https://github.com/espressif/esp-eth-drivers/commit/10ebd4c08d7a9a448576a1e2cd113101eec3a7b8))
* **ethernet_init:** made Dependencies conditional ([10ebd4c](https://github.com/espressif/esp-eth-drivers/commit/10ebd4c08d7a9a448576a1e2cd113101eec3a7b8))


### Bug Fixes

* **ethernet_init:** fixed ethernet_deinit_all and add return ([10ebd4c](https://github.com/espressif/esp-eth-drivers/commit/10ebd4c08d7a9a448576a1e2cd113101eec3a7b8))

## [1.0.0](https://github.com/espressif/esp-eth-drivers/compare/ethernet_init@v0.7.0...ethernet_init@v1.0.0) (2025-09-24)


### Features

* **eth_drivers:** migrated SPI ETH modules and PHY drivers ([952f637](https://github.com/espressif/esp-eth-drivers/commit/952f63745074569d5b27e4d44263c9055cb6fb64))
* **ethernet_init:** added dependency to use migrated drivers ([952f637](https://github.com/espressif/esp-eth-drivers/commit/952f63745074569d5b27e4d44263c9055cb6fb64))

## [0.7.0](https://github.com/espressif/esp-eth-drivers/compare/ethernet_init@v0.6.1...ethernet_init@v0.7.0) (2025-09-16)


### Features

* **ethernet_init:** extended EMAC kconfig by detailed RMII GPIO configuration ([d314dd0](https://github.com/espressif/esp-eth-drivers/commit/d314dd059dc5ac12f2e30b44919ff06d2aec3c65))

## [0.6.1](https://github.com/espressif/esp-eth-drivers/compare/ethernet_init@v0.6.0...ethernet_init@v0.6.1) (2025-08-06)


### Bug Fixes

* **ethernet_init:** fixed lan865x build issues on older IDFs ([8334c3e](https://github.com/espressif/esp-eth-drivers/commit/8334c3e9cc87618ddc49d0f6c00ef20f7e945ce9))

## [0.6.0](https://github.com/espressif/esp-eth-drivers/compare/ethernet_init@v0.5.0...ethernet_init@v0.6.0) (2025-07-18)


### Features

* **ethernet_init:** added support of LAN865x ([7c4c19f](https://github.com/espressif/esp-eth-drivers/commit/7c4c19fe6a942afd2db56a44c064ad1a9ef4d8a2))
* **ethernet-init:** added option to select different SPI ETH modules at a time ([7c4c19f](https://github.com/espressif/esp-eth-drivers/commit/7c4c19fe6a942afd2db56a44c064ad1a9ef4d8a2))

## [0.5.0](https://github.com/espressif/esp-eth-drivers/compare/ethernet_init@v0.4.0...ethernet_init@v0.5.0) (2025-05-06)


### Features

* **ethernet_init:** Added 10BASE-T1S configuration and board specific config ([7c589f8](https://github.com/espressif/esp-eth-drivers/commit/7c589f8dcf5a9852922927a410c55a0f295c9ad7))


### Bug Fixes

* fixed formatting in all components ([9f0f356](https://github.com/espressif/esp-eth-drivers/commit/9f0f356a4b1402c6c19787619288e0f84310464a))
