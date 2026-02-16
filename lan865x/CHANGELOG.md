# Changelog

## [0.1.4](https://github.com/espressif/esp-eth-drivers/compare/lan865x@v0.1.3...lan865x@v0.1.4) (2026-02-16)


### Bug Fixes

* **lan865x:** fixed 0 bytes copy when Tx frame length is a multiple of 64 [#138](https://github.com/espressif/esp-eth-drivers/issues/138) ([3a142d7](https://github.com/espressif/esp-eth-drivers/commit/3a142d79b8afd9d7d9c6e2cfbd558fe0af31d1e6))

## [0.1.3](https://github.com/espressif/esp-eth-drivers/compare/lan865x@v0.1.2...lan865x@v0.1.3) (2025-12-19)


### Bug Fixes

* **ksz865x:** fixed PRIV_REQUIRES in CMakLists ([c02f13d](https://github.com/espressif/esp-eth-drivers/commit/c02f13df0563e9b6b6603112b8ad8e4def0aa3ec))
* **lan865x:** Fix IRQ issue during reset of lan8651 ([c590d24](https://github.com/espressif/esp-eth-drivers/commit/c590d247137f6fc13ccddcabe5a278a4bf0000e0))

## [0.1.2](https://github.com/espressif/esp-eth-drivers/compare/lan865x@v0.1.1...lan865x@v0.1.2) (2025-08-12)


### Bug Fixes

* **lan865x:** fixed warning caused by 'MAC filter add/rm' on older IDFs ([d051f41](https://github.com/espressif/esp-eth-drivers/commit/d051f411c0e252883a834af78a34346d3567858f))

## [0.1.1](https://github.com/espressif/esp-eth-drivers/compare/lan865x@v0.1.0...lan865x@v0.1.1) (2025-08-04)


### Bug Fixes

* **lan865x:** Fix lan865 build issue for idf 5.4 ([5336c80](https://github.com/espressif/esp-eth-drivers/commit/5336c807cfa470ff39dce12443cb602665273673))

## 0.1.0 (2025-07-18)


### Features

* **lan865x:** Added a new driver to support LAN865x MAC-PHY chips ([9a2cd88](https://github.com/espressif/esp-eth-drivers/commit/9a2cd8826d24644d0334fe7fc6b6b8cefc926eda))
