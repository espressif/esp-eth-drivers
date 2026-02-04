/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_eth_mac.h"
#include "esp_eth_mac_spi.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "wiznet_spi.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration */
typedef struct emac_wiznet_s emac_wiznet_t;

/**
 * @brief SPI frame encoding constants (identical for W5500 and W6100)
 *
 * Both chips use the same SPI frame format:
 * - 16-bit address phase (offset within block)
 * - 8-bit control phase (BSB[4:0] + RWB + OM[1:0])
 */
#define WIZNET_ADDR_OFFSET       16    /*!< Address bits are in upper 16 bits of 32-bit value */
#define WIZNET_BSB_OFFSET        3     /*!< Block Select Bits offset in control phase */
#define WIZNET_RWB_OFFSET        2     /*!< Read/Write bit offset in control phase */
#define WIZNET_ACCESS_MODE_READ  0     /*!< Read access (RWB=0) */
#define WIZNET_ACCESS_MODE_WRITE 1     /*!< Write access (RWB=1) */
#define WIZNET_SPI_OP_MODE_VDM   0x00  /*!< Variable Data Length Mode */

#define WIZNET_BSB_SOCK_REG(s)   ((s) * 4 + 1)  /*!< Socket n Register block */
#define WIZNET_MAKE_MAP(offset, bsb) ((offset) << WIZNET_ADDR_OFFSET | (bsb) << WIZNET_BSB_OFFSET)

/**
 * @brief Logical register identifiers for translation table
 *
 * These provide a chip-agnostic way to reference registers. Each chip's ops
 * structure contains a translation table mapping these IDs to actual addresses.
 *
 * Note: Not all registers exist on all chips. Check for 0 (invalid) before use.
 */
typedef enum {
    /* Common registers */
    WIZNET_REG_MAC_ADDR,        /*!< MAC address register (W5500: SHAR, W6100: SHAR) */
    WIZNET_REG_SOCK_MR,         /*!< Socket mode register (W5500: Sn_MR, W6100: Sn_MR) */
    WIZNET_REG_SOCK_IMR,        /*!< Socket interrupt mask register (W5500: Sn_IMR, W6100: Sn_IMR) */
    WIZNET_REG_SOCK_RXBUF_SIZE, /*!< Socket RX buffer size (W5500: Sn_RXBUF_SIZE, W6100: Sn_RX_BSR) */
    WIZNET_REG_SOCK_TXBUF_SIZE, /*!< Socket TX buffer size (W5500: Sn_TXBUF_SIZE, W6100: Sn_TX_BSR) */
    WIZNET_REG_INT_LEVEL,       /*!< Interrupt level timer (W5500: INTLEVEL, W6100: INTPTMR) */
    WIZNET_REG_COUNT            /*!< Number of register IDs (must be last) */
} wiznet_reg_id_t;

/**
 * @brief Chip-specific operations structure for WIZnet Ethernet controllers
 *
 * This structure abstracts the register address and protocol differences
 * between W5500, W6100, and other WIZnet chips, allowing shared TX/RX logic.
 */
typedef struct {
    /* Register translation table for common registers */
    uint32_t regs[WIZNET_REG_COUNT];  /*!< Maps wiznet_reg_id_t to chip-specific addresses */

    /* Register addresses (pre-computed for socket 0) - used by TX/RX code */
    uint32_t reg_sock_cr;           /*!< Socket command register */
    uint32_t reg_sock_ir;           /*!< Socket interrupt register */
    uint32_t reg_sock_tx_fsr;       /*!< Socket TX free size register */
    uint32_t reg_sock_tx_wr;        /*!< Socket TX write pointer register */
    uint32_t reg_sock_rx_rsr;       /*!< Socket RX received size register */
    uint32_t reg_sock_rx_rd;        /*!< Socket RX read pointer register */
    uint32_t reg_simr;              /*!< Socket interrupt mask register */

    /* Memory address macros (offset added at runtime) */
    uint32_t mem_sock_tx_base;      /*!< Socket TX buffer base (add offset to get address) */
    uint32_t mem_sock_rx_base;      /*!< Socket RX buffer base (add offset to get address) */

    /* Interrupt clear handling */
    uint32_t reg_sock_irclr;        /*!< Socket interrupt clear register (same as IR for W5500) */

    /* Command values */
    uint8_t cmd_send;               /*!< Send command value */
    uint8_t cmd_recv;               /*!< Receive command value */
    uint8_t cmd_open;               /*!< Open socket command value */
    uint8_t cmd_close;              /*!< Close socket command value */

    /* Interrupt bits */
    uint8_t sir_send;               /*!< Send complete interrupt bit */
    uint8_t sir_recv;               /*!< Receive interrupt bit */
    uint8_t simr_sock0;             /*!< Socket 0 interrupt mask bit */

    /* Bit masks for socket mode register */
    uint8_t smr_mac_filter;         /*!< MAC filter enable bit */
    uint8_t smr_mac_raw;            /*!< MAC RAW mode enable bit */
    uint8_t smr_default;            /*!< Default SOCK0 mode (MACRAW + filter + mcast block) */

    /* PHY status checking */
    uint32_t reg_phy_status;        /*!< PHY status register address */
    uint8_t phy_link_mask;          /*!< Mask to check link status in PHY status register */

    /* Chip-specific function pointers */
    esp_err_t (*reset)(emac_wiznet_t *emac);        /*!< Chip-specific software reset */
    esp_err_t (*verify_id)(emac_wiznet_t *emac);    /*!< Verify chip ID/version */
    esp_err_t (*setup_default)(emac_wiznet_t *emac);/*!< Chip-specific default register setup */
} wiznet_chip_ops_t;

/**
 * @brief Common TX timeout constants (identical for W5500 and W6100)
 */
#define WIZNET_100M_TX_TMO_US (200)
#define WIZNET_10M_TX_TMO_US  (1500)

/**
 * @brief Common base structure for WIZnet EMAC implementations
 *
 * This structure contains fields common to all WIZnet Ethernet MAC drivers
 * (W5500, W6100, etc.). Chip-specific structures should embed this as their
 * first member to allow safe casting between base and derived types.
 *
 * Usage:
 * @code
 * typedef struct {
 *     emac_wiznet_t base;        // Must be first member
 *     uint8_t chip_specific;     // Chip-specific fields follow
 * } emac_w6100_t;
 * @endcode
 */
struct emac_wiznet_s {
    esp_eth_mac_t parent;           /*!< ESP-IDF MAC vtable (must be first for __containerof) */
    esp_eth_mediator_t *eth;        /*!< Mediator for callbacks to ESP-ETH layer */
    eth_spi_custom_driver_t spi;    /*!< SPI driver interface */
    TaskHandle_t rx_task_hdl;       /*!< RX task handle */
    const char *tag;                /*!< Logging tag (e.g., "w6100.mac") */
    const wiznet_chip_ops_t *ops;   /*!< Chip-specific operations */
    uint32_t sw_reset_timeout_ms;   /*!< Software reset timeout */
    int int_gpio_num;               /*!< Interrupt GPIO number, or -1 for polling mode */
    esp_timer_handle_t poll_timer;  /*!< Poll timer handle (polling mode only) */
    uint32_t poll_period_ms;        /*!< Poll period in milliseconds */
    uint8_t addr[6];                /*!< MAC address (ETH_ADDR_LEN) */
    bool packets_remain;            /*!< Flag indicating more packets in RX buffer */
    uint8_t *rx_buffer;             /*!< RX buffer for incoming frames */
    uint32_t tx_tmo;                /*!< TX timeout in microseconds (speed-dependent) */
};

/**
 * @brief Set mediator for Ethernet MAC
 *
 * @param mac Ethernet MAC instance
 * @param eth Ethernet mediator
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if eth is NULL
 */
esp_err_t emac_wiznet_set_mediator(esp_eth_mac_t *mac, esp_eth_mediator_t *eth);

/**
 * @brief Get MAC address
 *
 * @param mac Ethernet MAC instance
 * @param addr Buffer to store MAC address (6 bytes)
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if addr is NULL
 */
esp_err_t emac_wiznet_get_addr(esp_eth_mac_t *mac, uint8_t *addr);

/**
 * @brief Set duplex mode (informational only, WIZnet chips auto-negotiate)
 *
 * @param mac Ethernet MAC instance
 * @param duplex Duplex mode
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG for unknown duplex
 */
esp_err_t emac_wiznet_set_duplex(esp_eth_mac_t *mac, eth_duplex_t duplex);

/**
 * @brief Enable flow control (not supported by WIZnet chips)
 *
 * @param mac Ethernet MAC instance
 * @param enable Enable flag (ignored)
 * @return ESP_ERR_NOT_SUPPORTED always
 */
esp_err_t emac_wiznet_enable_flow_ctrl(esp_eth_mac_t *mac, bool enable);

/**
 * @brief Set peer pause ability (not supported by WIZnet chips)
 *
 * @param mac Ethernet MAC instance
 * @param ability Pause ability (ignored)
 * @return ESP_ERR_NOT_SUPPORTED always
 */
esp_err_t emac_wiznet_set_peer_pause_ability(esp_eth_mac_t *mac, uint32_t ability);

/**
 * @brief Set link state and start/stop MAC accordingly
 *
 * @param mac Ethernet MAC instance
 * @param link Link state (ETH_LINK_UP or ETH_LINK_DOWN)
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG for unknown link state
 */
esp_err_t emac_wiznet_set_link(esp_eth_mac_t *mac, eth_link_t link);

/**
 * @brief Common start function for WIZnet chips
 *
 * Opens socket 0 and enables interrupts.
 *
 * @param mac Ethernet MAC instance
 * @return ESP_OK on success
 */
esp_err_t emac_wiznet_start(esp_eth_mac_t *mac);

/**
 * @brief Common stop function for WIZnet chips
 *
 * Disables interrupts and closes socket 0.
 *
 * @param mac Ethernet MAC instance
 * @return ESP_OK on success
 */
esp_err_t emac_wiznet_stop(esp_eth_mac_t *mac);

/**
 * @brief Set promiscuous mode
 *
 * Enables or disables MAC filtering.
 *
 * @param mac Ethernet MAC instance
 * @param enable true to enable promiscuous mode (disable filtering)
 * @return ESP_OK on success
 */
esp_err_t emac_wiznet_set_promiscuous(esp_eth_mac_t *mac, bool enable);

/**
 * @brief Set MAC address
 *
 * Copies address to internal storage and writes to chip register.
 *
 * @param mac Ethernet MAC instance
 * @param addr 6-byte MAC address
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if addr is NULL
 */
esp_err_t emac_wiznet_set_addr(esp_eth_mac_t *mac, uint8_t *addr);

/**
 * @brief Set link speed
 *
 * Updates TX timeout based on link speed.
 *
 * @param mac Ethernet MAC instance
 * @param speed ETH_SPEED_10M or ETH_SPEED_100M
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG for unknown speed
 */
esp_err_t emac_wiznet_set_speed(esp_eth_mac_t *mac, eth_speed_t speed);

/**
 * @brief Write PHY register
 *
 * WIZnet PHY registers are 8-bit and mapped directly in the chip's register space.
 * The phy_reg parameter contains the full chip register address.
 *
 * @param mac Ethernet MAC instance
 * @param phy_addr PHY address (unused for WIZnet chips)
 * @param phy_reg Register address (chip-specific, e.g., W5500_REG_PHYCFGR)
 * @param reg_value Value to write (only lower 8 bits used)
 * @return ESP_OK on success
 */
esp_err_t emac_wiznet_write_phy_reg(esp_eth_mac_t *mac, uint32_t phy_addr, uint32_t phy_reg, uint32_t reg_value);

/**
 * @brief Read PHY register
 *
 * WIZnet PHY registers are 8-bit and mapped directly in the chip's register space.
 * The phy_reg parameter contains the full chip register address.
 *
 * @param mac Ethernet MAC instance
 * @param phy_addr PHY address (unused for WIZnet chips)
 * @param phy_reg Register address (chip-specific, e.g., W5500_REG_PHYCFGR)
 * @param reg_value Pointer to store read value (only 1 byte written)
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if reg_value is NULL
 */
esp_err_t emac_wiznet_read_phy_reg(esp_eth_mac_t *mac, uint32_t phy_addr, uint32_t phy_reg, uint32_t *reg_value);

/**
 * @brief Initialize MAC
 *
 * Installs GPIO ISR, notifies mediator, resets chip, verifies ID, and sets up defaults.
 * Uses chip-specific ops function pointers for reset, verify_id, and setup_default.
 *
 * @param mac Ethernet MAC instance
 * @return ESP_OK on success
 */
esp_err_t emac_wiznet_init(esp_eth_mac_t *mac);

/**
 * @brief Deinitialize MAC
 *
 * Stops the MAC, removes ISR handler, stops poll timer, and notifies mediator.
 *
 * @param mac Ethernet MAC instance
 * @return ESP_OK always
 */
esp_err_t emac_wiznet_deinit(esp_eth_mac_t *mac);

/**
 * @brief Delete MAC instance and free resources
 *
 * Deletes poll timer, RX task, deinitializes SPI, frees RX buffer and struct.
 *
 * @param mac Ethernet MAC instance
 * @return ESP_OK always
 */
esp_err_t emac_wiznet_del(esp_eth_mac_t *mac);

/**
 * @brief Install GPIO ISR handler for interrupt mode
 *
 * Configures the interrupt GPIO pin and installs the ISR handler.
 * Does nothing if emac->int_gpio_num < 0 (polling mode).
 *
 * @param emac WIZnet EMAC instance
 * @return ESP_OK on success
 */
esp_err_t wiznet_install_gpio_isr(emac_wiznet_t *emac);

/**
 * @brief Create poll timer for polling mode
 *
 * Creates the esp_timer for polling mode operation.
 * Does nothing if emac->int_gpio_num >= 0 (interrupt mode).
 *
 * @param emac WIZnet EMAC instance
 * @return ESP_OK on success, or error from esp_timer_create()
 */
esp_err_t wiznet_create_poll_timer(emac_wiznet_t *emac);

/**
 * @brief Common default setup for WIZnet chips
 *
 * Configures socket buffer allocation, SOCK0 mode, and interrupt settings.
 * This is called from chip-specific setup_default() after any chip-specific
 * pre-setup (e.g., W6100 register unlock).
 *
 * Operations performed:
 * - Allocate all 16KB RX and TX buffer to SOCK0
 * - Disable all socket interrupts (SIMR=0)
 * - Set SOCK0 mode register to ops->smr_default
 * - Enable SOCK0 receive interrupt
 * - Set interrupt level timer to maximum (~1.5ms)
 *
 * @param emac WIZnet EMAC instance
 * @return ESP_OK on success
 */
esp_err_t wiznet_setup_default(emac_wiznet_t *emac);

/*******************************************************************************
 * Register Access Functions
 *
 * These functions provide unified register access for all WIZnet chips.
 * The SPI frame encoding is identical for W5500 and W6100.
 ******************************************************************************/

/**
 * @brief Read data from WIZnet chip register or memory
 *
 * Performs SPI read with the standard WIZnet frame format:
 * - 16-bit address phase
 * - 8-bit control phase (BSB + RWB=0 + OM)
 *
 * @param emac WIZnet EMAC instance
 * @param address Pre-encoded register address (includes BSB in bits 15:11)
 * @param data Buffer to store read data
 * @param len Number of bytes to read
 * @return ESP_OK on success, or SPI error code
 */
esp_err_t wiznet_read(emac_wiznet_t *emac, uint32_t address, void *data, uint32_t len);

/**
 * @brief Write data to WIZnet chip register or memory
 *
 * Performs SPI write with the standard WIZnet frame format:
 * - 16-bit address phase
 * - 8-bit control phase (BSB + RWB=1 + OM)
 *
 * @param emac WIZnet EMAC instance
 * @param address Pre-encoded register address (includes BSB in bits 15:11)
 * @param data Data to write
 * @param len Number of bytes to write
 * @return ESP_OK on success, or SPI error code
 */
esp_err_t wiznet_write(emac_wiznet_t *emac, uint32_t address, const void *data, uint32_t len);

/**
 * @brief Send a socket command and wait for completion
 *
 * Writes the command to the socket command register and polls until
 * the command is acknowledged (register reads 0).
 *
 * @param emac WIZnet EMAC instance
 * @param command Command value to send
 * @param timeout_ms Timeout in milliseconds
 * @return ESP_OK on success, ESP_ERR_TIMEOUT if command not acknowledged
 */
esp_err_t wiznet_send_command(emac_wiznet_t *emac, uint8_t command, uint32_t timeout_ms);

/**
 * @brief Common transmit function using chip ops
 *
 * Transmits an Ethernet frame using the chip-specific register addresses
 * from the ops structure.
 *
 * @param mac Ethernet MAC instance
 * @param buf Frame buffer to transmit
 * @param length Frame length in bytes
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if frame too large,
 *         ESP_ERR_NO_MEM if TX buffer full, ESP_FAIL on timeout
 */
esp_err_t emac_wiznet_transmit(esp_eth_mac_t *mac, uint8_t *buf, uint32_t length);

/**
 * @brief Common receive function using chip ops
 *
 * Receives an Ethernet frame using the chip-specific register addresses
 * from the ops structure.
 *
 * @param mac Ethernet MAC instance
 * @param buf Buffer to store received frame
 * @param[in,out] length On input, buffer size or 0 for auto-alloc mode;
 *                       on output, actual frame length
 * @return ESP_OK on success
 */
esp_err_t emac_wiznet_receive(esp_eth_mac_t *mac, uint8_t *buf, uint32_t *length);

/**
 * @brief Common RX task function using chip ops
 *
 * This is the main receive task loop that handles interrupts and receives
 * frames. It uses chip-specific registers via the ops structure.
 *
 * @param arg Pointer to emac_wiznet_t instance
 */
void emac_wiznet_task(void *arg);

/*******************************************************************************
 * Common Constructor Helper
 ******************************************************************************/

/**
 * @brief Common WIZnet chip configuration (identical for W5500/W6100)
 *
 * This can be cast from eth_w5500_config_t or eth_w6100_config_t since
 * they have identical layouts.
 */
typedef struct {
    int int_gpio_num;                                   /*!< Interrupt GPIO number, set -1 for polling */
    uint32_t poll_period_ms;                            /*!< Poll period in ms (when int_gpio_num < 0) */
    spi_host_device_t spi_host_id;                      /*!< SPI peripheral */
    spi_device_interface_config_t *spi_devcfg;          /*!< SPI device configuration */
    eth_spi_custom_driver_config_t custom_spi_driver;   /*!< Custom SPI driver definitions */
} eth_wiznet_config_t;

/**
 * @brief Initialize emac_wiznet_t base structure
 *
 * Common initialization for W5500/W6100 constructors. Sets up all common
 * fields in emac_wiznet_t: SPI driver, RX task, poll timer, RX buffer.
 *
 * @param emac Pointer to pre-allocated emac_wiznet_t (first member of chip-specific struct)
 * @param wiznet_config WIZnet configuration (can cast from eth_w5500_config_t/eth_w6100_config_t)
 * @param mac_config ESP-IDF MAC configuration
 * @param ops Chip-specific operations table
 * @param tag Logging tag (e.g., "w5500.mac")
 * @param task_name Task name (e.g., "w5500_tsk")
 * @return ESP_OK on success, ESP_ERR_NO_MEM if allocation fails, or other error
 */
esp_err_t emac_wiznet_init_common(emac_wiznet_t *emac,
                                  const eth_wiznet_config_t *wiznet_config,
                                  const eth_mac_config_t *mac_config,
                                  const wiznet_chip_ops_t *ops,
                                  const char *tag,
                                  const char *task_name);

/**
 * @brief Cleanup after failed emac_wiznet_init_common()
 *
 * Frees resources allocated by emac_wiznet_init_common().
 *
 * @param emac WIZnet EMAC instance
 */
void emac_wiznet_cleanup_common(emac_wiznet_t *emac);

#ifdef __cplusplus
}
#endif
