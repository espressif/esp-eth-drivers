/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_eth_driver.h" // for esp_eth_handle_t
#include "ksz8863.h"
#include "ksz8863_ctrl.h"
#include "esp_eth_mac_ksz8863.h"
#include "esp_eth_phy_ksz8863.h"


#ifdef __cplusplus
extern "C" {
#endif

#define KSZ8863_PORT_1 (0)
#define KSZ8863_PORT_2 (1)

/**
 * @brief Default configuration for KSZ8863 Ethernet driver
 *
 */
#define ETH_KSZ8863_DEFAULT_CONFIG(emac, ephy)      \
    {                                               \
        .mac = emac,                                \
        .phy = ephy,                                \
        .check_link_period_ms = 2000,               \
        .stack_input = NULL,                        \
        .on_lowlevel_init_done = NULL,              \
        .on_lowlevel_deinit_done = NULL,            \
        .read_phy_reg = ksz8863_phy_reg_read,       \
        .write_phy_reg = ksz8863_phy_reg_write,     \
    }

typedef enum {
    KSZ8863_ETH_CMD_S_START_SWITCH = ETH_CMD_CUSTOM_MAC_CMDS,
    KSZ8863_ETH_CMD_G_START_SWITCH,
    KSZ8863_ETH_CMD_S_FLUSH_MAC_DYN,
    KSZ8863_ETH_CMD_S_RX_EN,
    KSZ8863_ETH_CMD_G_RX_EN,
    KSZ8863_ETH_CMD_S_TX_EN,
    KSZ8863_ETH_CMD_G_TX_EN,
    KSZ8863_ETH_CMD_S_LEARN_DIS,
    KSZ8863_ETH_CMD_G_LEARN_DIS,
    KSZ8863_ETH_CMD_S_MAC_STA_TBL,
    KSZ8863_ETH_CMD_G_MAC_STA_TBL,
    KSZ8863_ETH_CMD_G_MAC_DYN_TBL,
    KSZ8863_ETH_CMD_S_TAIL_TAG,
    KSZ8863_ETH_CMD_G_TAIL_TAG,
    KSZ8863_ETH_CMD_G_PORT_NUM,
} ksz8863_eth_io_cmd_t;

typedef struct {
    uint16_t start_entry;
    uint16_t etries_num;
    union {
        ksz8863_dyn_mac_table_t *dyn_tbls;
        ksz8863_sta_mac_table_t *sta_tbls;
    };
} ksz8863_mac_tbl_info_t;

/**
 * @brief Software reset of KSZ8863
 *
 * @note: Since since multiple MAC/PHY instances exist due to fact that the device has multiple ports, the reset is not
 * called from MAC/PHY but needs to be called separately to be sure, that is called only once and at right time.
 * Otherwise it could reset other already initalized instance.
 *
 * @param port_eth_handle handle of Ethernet driver
 * @return esp_err_t
 *          ESP_OK - when successful
 */
esp_err_t ksz8863_sw_reset(esp_eth_handle_t port_eth_handle);

/**
 * @brief Hardware reset of KSZ8863
 *
 * @note: Since since multiple MAC/PHY instances exist due to fact that the device has multiple ports, the reset is not
 * called from MAC/PHY but needs to be called separately to be sure, that is called only once and at right time.
 * Otherwise it could reset other already initalized instance.
 *
 * @param reset_gpio_num GPIO pin number associated with KSZ8863 reset pin
 * @return esp_err_t
 *          ESP_OK - always
 */
esp_err_t ksz8863_hw_reset(int reset_gpio_num);

/**
 * @brief Configures REFCLKO_3 Output to be connected to REFCLKI_3 internally (looped-back).
 *
 * @param port_eth_handle handle of KSZ8863 non-Host (P1/P2) port Ethernet driver
 * @param rmii_internal_clk true when internal P3 RMII REFCLK loopback to be configured
 * @return esp_err_t
 *          ESP_OK - when registry configuration is successful
 */
esp_err_t ksz8863_p3_rmii_internal_clk(esp_eth_handle_t port_eth_handle, bool rmii_internal_clk);

/**
 * @brief Configures invertion of P3 RMII REFCKL
 *
 * @param port_eth_handle handle of Ethernet driver
 * @param rmii_clk_invert true when to invert invert P3 RMII REFCKL
 * @return esp_err_t
 *          ESP_OK - when registry configuration is successful
 */
esp_err_t ksz8863_p3_rmii_clk_invert(esp_eth_handle_t port_eth_handle, bool rmii_clk_invert);

/**
 * @brief Registers KSZ8863 port Ethernet driver handle and associates it with port number. This information is
 * later used by `ksz8863_eth_tail_tag_port_forward` to decide where to forward frame received at Host (P3) port.
 *
 * @param port_eth_handle handle of KSZ8863 non-Host (P1/P2) port Ethernet driver
 * @param port_num port number
 * @return esp_err_t
 *          ESP_OK - when port info successfully registered
 *          ESP_ERR_NO_MEM - when not enough memory to store port information
 */
esp_err_t ksz8863_register_tail_tag_port(esp_eth_handle_t port_eth_handle, int32_t port_num);

/**
 * @brief Forwards received frames on Host Ethernet interface to Port Ethernet interfaces based on Tail Tagging.
 *
 * @note: this functions is callback to be registered as `stack_input` of Host Ethernet interface by `esp_eth_update_input_path`.
 *
 * @param eth_handle handle of KSZ8863 Host port Ethernet driver
 * @param buffer received frame
 * @param length length of received frame
 * @param priv private info
 * @return esp_err_t
 *          ESP_OK - always
 */
esp_err_t ksz8863_eth_tail_tag_port_forward(esp_eth_handle_t eth_handle, uint8_t *buffer, uint32_t length, void *priv);

/**
 * @brief Registers Host Ethernet interface handle so Port Ethernet interfaces could transmit via it.
 *
 * @param host_eth_handle handle of KSZ8863 Host port Ethernet driver
 * @return esp_err_t
 *          ESP_OK - when successfully registered
 *          ESP_ERR_INVALID_STATE - when Host Ethernet handle is not initialized
 */
esp_err_t ksz8863_register_host_eth_hndl(esp_eth_handle_t host_eth_handle);

/**
 * @brief Used by Port Ethernet interfaces to transmit via Host Ethernet interface (which is data gateway to KSZ8863).
 * The Host Ethernet interface needs to be registered first to be able to use this function.
 *
 * @note This function is intended to be only used internally by the driver.
 *
 * @param buf frame to be transmitted
 * @param length frame's length
 * @param tail_tag Tail Tag number
 * @return esp_err_t
 *          ESP_OK - when success
 *          ESP_ERR_INVALID_STATE - when Host Ethernet interface has not been registered yet
 */
esp_err_t ksz8863_eth_transmit_via_host(void *buf, size_t length, uint8_t tail_tag);

/**
 * @brief Transmit frame via Host Ethernet interface with Tail Tag equal to 0, i.e. normal MAC table lookup in KSZ8863.
 *
 * @param host_eth_handle handle of KSZ8863 Host port Ethernet driver
 * @param buf frame to be transmitted
 * @param length frame's length
 * @return esp_err_t
 *          ESP_OK - when success
 *          ESP_ERR_INVALID_ARG - when invalid input parameter
 */
esp_err_t ksz8863_eth_transmit_normal_lookup(esp_eth_handle_t host_eth_handle, void *buf, size_t length);

#ifdef __cplusplus
}
#endif
