/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"
#include "driver/spi_master.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief SPI configuration for WIZnet Ethernet controllers
 *
 * This structure contains the SPI-related fields needed to initialize
 * the default SPI driver. It matches the layout of eth_w5500_config_t
 * and eth_w6100_config_t for the SPI-relevant fields.
 */
typedef struct {
    int int_gpio_num;                           /*!< Interrupt GPIO number (unused by SPI layer, but maintains struct layout) */
    uint32_t poll_period_ms;                    /*!< Poll period (unused by SPI layer, but maintains struct layout) */
    spi_host_device_t spi_host_id;              /*!< SPI peripheral */
    spi_device_interface_config_t *spi_devcfg;  /*!< SPI device configuration */
    /* custom_spi_driver field not needed here - handled by chip-specific code */
} wiznet_spi_config_t;

/**
 * @brief Internal SPI driver structure for WIZnet MAC implementations
 */
typedef struct {
    void *ctx;
    void *(*init)(const void *spi_config);
    esp_err_t (*deinit)(void *spi_ctx);
    esp_err_t (*read)(void *spi_ctx, uint32_t cmd, uint32_t addr, void *data, uint32_t data_len);
    esp_err_t (*write)(void *spi_ctx, uint32_t cmd, uint32_t addr, const void *data, uint32_t data_len);
} eth_spi_custom_driver_t;

/**
 * @brief Initialize default SPI driver for WIZnet controllers
 *
 * @param spi_config Pointer to SPI configuration (wiznet_spi_config_t or compatible)
 * @return SPI context pointer on success, NULL on failure
 */
void *wiznet_spi_init(const void *spi_config);

/**
 * @brief Deinitialize default SPI driver
 *
 * @param spi_ctx SPI context returned by wiznet_spi_init
 * @return ESP_OK on success
 */
esp_err_t wiznet_spi_deinit(void *spi_ctx);

/**
 * @brief Read data via SPI
 *
 * @param spi_ctx SPI context
 * @param cmd Command/address phase value (16 bits)
 * @param addr Control phase value (8 bits)
 * @param data Buffer to store read data
 * @param len Number of bytes to read
 * @return ESP_OK on success, ESP_FAIL or ESP_ERR_TIMEOUT on failure
 */
esp_err_t wiznet_spi_read(void *spi_ctx, uint32_t cmd, uint32_t addr, void *data, uint32_t len);

/**
 * @brief Write data via SPI
 *
 * @param spi_ctx SPI context
 * @param cmd Command/address phase value (16 bits)
 * @param addr Control phase value (8 bits)
 * @param data Data to write
 * @param len Number of bytes to write
 * @return ESP_OK on success, ESP_FAIL or ESP_ERR_TIMEOUT on failure
 */
esp_err_t wiznet_spi_write(void *spi_ctx, uint32_t cmd, uint32_t addr, const void *data, uint32_t len);

#ifdef __cplusplus
}
#endif
