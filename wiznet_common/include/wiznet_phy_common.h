/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_eth_phy.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration */
typedef struct phy_wiznet_s phy_wiznet_t;

/**
 * @brief Entry mapping opmode register value to speed/duplex configuration
 *
 * Used by table-driven get_mode/set_mode implementations. Each chip provides
 * a table of these entries for its fixed-mode opmodes.
 */
typedef struct {
    uint8_t opmode;           /**< Register value for this mode */
    eth_speed_t speed;        /**< Speed for this mode */
    eth_duplex_t duplex;      /**< Duplex for this mode */
} wiznet_opmode_entry_t;

/**
 * @brief Configuration for creating a WIZnet PHY instance
 *
 * This structure contains all chip-specific configuration needed
 * by phy_wiznet_new() to create and configure a PHY instance.
 */
typedef struct {
    /* Basic config from esp_phy_config_t */
    int phy_addr;                       /**< PHY address */
    uint32_t reset_timeout_ms;          /**< Reset timeout in milliseconds */
    uint32_t autonego_timeout_ms;       /**< Auto-negotiation timeout in milliseconds */
    int reset_gpio_num;                 /**< Hardware reset GPIO, or -1 if not used */

    /* PHY status register configuration */
    uint32_t phy_status_reg;            /**< Register address for PHY status (link/speed/duplex) */
    eth_speed_t speed_when_bit_set;     /**< Speed value when status register speed bit is 1 */
    eth_speed_t speed_when_bit_clear;   /**< Speed value when status register speed bit is 0 */
    eth_duplex_t duplex_when_bit_set;   /**< Duplex value when status register duplex bit is 1 */
    eth_duplex_t duplex_when_bit_clear; /**< Duplex value when status register duplex bit is 0 */

    /* Table-driven get_mode configuration */
    const wiznet_opmode_entry_t *opmode_table;  /**< Table of fixed-mode entries for get_mode lookup */
    uint8_t opmode_table_size;          /**< Number of entries in opmode_table */
    uint32_t opmode_status_reg;         /**< Register to read current opmode from */
    uint8_t opmode_shift;               /**< Bit shift for opmode field in status register */
    uint8_t opmode_mask;                /**< Mask for opmode field after shifting */

    /* Chip-specific function pointers */
    esp_err_t (*is_autoneg_enabled)(phy_wiznet_t *wiznet, bool *enabled);
    esp_err_t (*set_mode)(phy_wiznet_t *wiznet, bool autoneg, eth_speed_t speed, eth_duplex_t duplex);

    /* Chip-specific vtable overrides */
    esp_err_t (*reset)(esp_eth_phy_t *phy);     /**< Chip-specific reset function */
    esp_err_t (*pwrctl)(esp_eth_phy_t *phy, bool enable);  /**< Chip-specific power control */
} phy_wiznet_config_t;

/**
 * @brief Create a new WIZnet PHY instance (factory function)
 *
 * Allocates and initializes a new phy_wiznet_t instance with all common
 * fields and vtable entries.
 *
 * @param config PHY configuration including chip-specific settings
 * @return Pointer to esp_eth_phy_t vtable, or NULL on failure
 */
esp_eth_phy_t *phy_wiznet_new(const phy_wiznet_config_t *config);

/**
 * @brief Set mediator for PHY
 *
 * @param phy Ethernet PHY instance
 * @param eth Ethernet mediator
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if eth is NULL
 */
esp_err_t phy_wiznet_set_mediator(esp_eth_phy_t *phy, esp_eth_mediator_t *eth);

/**
 * @brief Set link state
 *
 * Called by MAC layer to inform PHY of link state changes.
 *
 * @param phy Ethernet PHY instance
 * @param link Link state (ETH_LINK_UP or ETH_LINK_DOWN)
 * @return ESP_OK on success
 */
esp_err_t phy_wiznet_set_link(esp_eth_phy_t *phy, eth_link_t link);

/**
 * @brief Set PHY address
 *
 * @param phy Ethernet PHY instance
 * @param addr PHY address
 * @return ESP_OK always
 */
esp_err_t phy_wiznet_set_addr(esp_eth_phy_t *phy, uint32_t addr);

/**
 * @brief Get PHY address
 *
 * @param phy Ethernet PHY instance
 * @param[out] addr Pointer to store PHY address
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if addr is NULL
 */
esp_err_t phy_wiznet_get_addr(esp_eth_phy_t *phy, uint32_t *addr);

/**
 * @brief Delete PHY instance and free resources
 *
 * @param phy Ethernet PHY instance
 * @return ESP_OK always
 */
esp_err_t phy_wiznet_del(esp_eth_phy_t *phy);

/**
 * @brief Advertise pause ability (not supported by WIZnet internal PHYs)
 *
 * @param phy Ethernet PHY instance
 * @param ability Pause ability (ignored)
 * @return ESP_OK always (no-op)
 */
esp_err_t phy_wiznet_advertise_pause_ability(esp_eth_phy_t *phy, uint32_t ability);

/**
 * @brief Enable/disable loopback (not supported by WIZnet internal PHYs)
 *
 * @param phy Ethernet PHY instance
 * @param enable Enable flag (ignored)
 * @return ESP_ERR_NOT_SUPPORTED always
 */
esp_err_t phy_wiznet_loopback(esp_eth_phy_t *phy, bool enable);

/**
 * @brief Hardware reset PHY chip via GPIO
 *
 * Asserts reset pin low for 100us then releases.
 * If reset_gpio_num is negative, this is a no-op.
 *
 * @param phy Ethernet PHY instance
 * @return ESP_OK always
 */
esp_err_t phy_wiznet_reset_hw(esp_eth_phy_t *phy);

/**
 * @brief Get link status and update speed/duplex
 *
 * Reads PHY status register and notifies MAC layer of any link state changes.
 * Uses phy_status_reg, speed_when_bit_set/clear, and duplex_when_bit_set/clear
 * fields from phy_wiznet_t to interpret register bits.
 *
 * @param phy Ethernet PHY instance
 * @return ESP_OK on success
 */
esp_err_t phy_wiznet_get_link(esp_eth_phy_t *phy);

/**
 * @brief Initialize PHY
 *
 * Powers on PHY and performs software reset via vtable calls.
 *
 * @param phy Ethernet PHY instance
 * @return ESP_OK on success
 */
esp_err_t phy_wiznet_init(esp_eth_phy_t *phy);

/**
 * @brief Deinitialize PHY
 *
 * Powers off PHY via vtable call.
 *
 * @param phy Ethernet PHY instance
 * @return ESP_OK on success
 */
esp_err_t phy_wiznet_deinit(esp_eth_phy_t *phy);

/**
 * @brief Control auto-negotiation
 *
 * Common implementation that uses chip_ops for register-level operations.
 * Supports restart, enable, disable, and get status commands.
 *
 * @param phy Ethernet PHY instance
 * @param cmd Auto-negotiation command
 * @param[in,out] autonego_en_stat Pointer to store/return autoneg status
 * @return ESP_OK on success
 */
esp_err_t phy_wiznet_autonego_ctrl(esp_eth_phy_t *phy, eth_phy_autoneg_cmd_t cmd, bool *autonego_en_stat);

/**
 * @brief Set PHY speed
 *
 * Reads current duplex from status register, sets fixed mode with
 * new speed and current duplex, then resets PHY.
 *
 * @param phy Ethernet PHY instance
 * @param speed Speed to set
 * @return ESP_OK on success
 */
esp_err_t phy_wiznet_set_speed(esp_eth_phy_t *phy, eth_speed_t speed);

/**
 * @brief Set PHY duplex mode
 *
 * Reads current speed from status register, sets fixed mode with
 * current speed and new duplex, then resets PHY.
 *
 * @param phy Ethernet PHY instance
 * @param duplex Duplex mode to set
 * @return ESP_OK on success
 */
esp_err_t phy_wiznet_set_duplex(esp_eth_phy_t *phy, eth_duplex_t duplex);

/**
 * @brief Get current PHY mode and speed/duplex settings
 *
 * Reads opmode from status register, looks up in opmode_table.
 * In autoneg mode (not found in table), reads speed/duplex from link status bits.
 *
 * @param wiznet PHY instance
 * @param[out] autoneg Pointer to store autoneg status
 * @param[out] speed Pointer to store current speed
 * @param[out] duplex Pointer to store current duplex
 * @return ESP_OK on success
 */
esp_err_t phy_wiznet_get_mode(phy_wiznet_t *wiznet, bool *autoneg, eth_speed_t *speed, eth_duplex_t *duplex);

/**
 * @brief Get phy_wiznet_t from esp_eth_phy_t pointer
 *
 * Performs __containerof to recover the WIZnet PHY instance from
 * the ESP-IDF PHY vtable pointer.
 *
 * @param phy ESP-IDF PHY instance
 * @return Pointer to WIZnet PHY instance
 */
phy_wiznet_t *phy_wiznet_from_parent(esp_eth_phy_t *phy);

/**
 * @brief Get Ethernet mediator
 *
 * @param wiznet WIZnet PHY instance
 * @return Pointer to mediator
 */
esp_eth_mediator_t *phy_wiznet_get_eth(phy_wiznet_t *wiznet);

/**
 * @brief Get PHY address as uint32_t
 *
 * @param wiznet WIZnet PHY instance
 * @return PHY address
 */
uint32_t phy_wiznet_get_addr_val(phy_wiznet_t *wiznet);

/**
 * @brief Set link status to ETH_LINK_DOWN
 *
 * @param wiznet WIZnet PHY instance
 */
void phy_wiznet_set_link_down(phy_wiznet_t *wiznet);

#ifdef __cplusplus
}
#endif
