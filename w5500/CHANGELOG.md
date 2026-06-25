# Changelog

## [2.0.0](https://github.com/espressif/esp-eth-drivers/compare/w5500@v1.0.1...w5500@v2.0.0) (2026-06-25)


### ⚠ BREAKING CHANGES

* **w5500:** eth_w5500_config_t layout changed. The fields int_gpio_num, poll_period_ms, spi_host_id, spi_devcfg and custom_spi_driver are now nested inside a .base member of type eth_wiznet_config_t.

### Features

* **wiznet_common:** added common Wiznet subcomponent ([a3396df](https://github.com/espressif/esp-eth-drivers/commit/a3396dfaae9b57f0df40ff7add5c2b55de11a5d1))

## [1.0.1](https://github.com/espressif/esp-eth-drivers/compare/w5500@v1.0.0...w5500@v1.0.1) (2025-12-08)


### Bug Fixes

* **w5500:** Fixed W5500 poor performance in 10M mode ([c45be33](https://github.com/espressif/esp-eth-drivers/commit/c45be33caa02c1e80e20fafba80603bbe0bfb26a))

## 1.0.0 (2025-09-24)


### Features

* **eth_drivers:** migrated SPI ETH modules and PHY drivers ([952f637](https://github.com/espressif/esp-eth-drivers/commit/952f63745074569d5b27e4d44263c9055cb6fb64))
* **ethernet_init:** added dependency to use migrated drivers ([952f637](https://github.com/espressif/esp-eth-drivers/commit/952f63745074569d5b27e4d44263c9055cb6fb64))
