/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileContributor: 2024 Sergey Kharenko
 * SPDX-FileContributor: 2024 Espressif Systems (Shanghai) CO LTD
 */

#pragma once
#include <stdint.h>

#define CH182_INFO_OUI                       0x1CDC64

#define ETH_PHY_PAGE_SEL_REG_ADDR            0x1F

/**
 * @defgroup PHY_CTL1
 * @brief PHY Customize Control Register PHY_CTL1
 * @addtogroup PHY_CTL1
 * @{
 */
typedef union {
    struct {
        uint32_t reserved1 : 7;
        uint32_t sqe_en : 1;            //
        uint32_t jabber_en : 1;
        uint32_t pma_lpbk : 1;
        uint32_t pcs_lpbk : 1;
        uint32_t remote_lpbk : 1;
        uint32_t force_link : 1;
        uint32_t reserved2 : 3;
    };
    uint32_t val;
} phy_ctl1_reg_t;

#define ETH_PHY_CTL1_REG_ADDR               0x19
#define ETH_PHY_CTL1_REG_PAGE               0x00
/**
 * @}
 */


/**
 * @defgroup RMII_MODE_SET1
 * @brief RMII Mode Setting Register RMII_MODE_SET1
 * @addtogroup RMII_MODE_SET1
 * @{
 */
typedef union {
    struct {
        uint32_t reserved1 : 1;
        uint32_t rg_rmii_rxsel: 1;
        uint32_t rg_rmii_rxdv_set: 1;
        uint32_t rmii_mode : 1;
        uint32_t rg_rmii_rx_offset : 4;
        uint32_t rg_rmii_tx_offset : 4;
        uint32_t rg_rmii_clk_dir : 1;
        uint32_t reserved2 : 3;
    };
    uint32_t val;
} rmii_mode_set1_reg_t;

#define ETH_RMII_MODE_SET1_REG_ADDR         0x10
#define ETH_RMII_MODE_SET1_REG_PAGE         0x07

#define ETH_RMII_CLK_DIR_OUT                0x00
#define ETH_RMII_CLK_DIR_IN                 0x01

#define ETH_RMII_MODE_MII                   0x00
#define ETH_RMII_MODE_RMII                  0x01
/**
 * @}
 */


/**
 * @defgroup INTERRUPT_MASK
 * @brief Interrupt, WOL Enable and LED Function Registers INTERRUPT_MASK
 * @addtogroup INTERRUPT_MASK
 * @{
 */
typedef union {
    struct {
        uint32_t reserved1 : 3;
        uint32_t customized_led : 1;
        uint32_t led_sel : 2;
        uint32_t reserved2 : 4;
        uint32_t rg_led_wol_sel : 1;
        uint32_t int_anerr : 1;
        uint32_t int_dupchg : 1;
        uint32_t int_linkchg : 1;
        uint32_t int_spdchg : 1;
        uint32_t reserved3 : 1;
    };
    uint32_t val;
} interrupt_mask_reg_t;

#define ETH_INTERRUPT_MASK_REG_ADDR         0x13
#define ETH_INTERRUPT_MASK_REG_PAGE         0x07

/**
 * @note LED_MODE   |       0       |          1         |          2        |          3
 *      LED0        |    ACT(all)   | LINK(all)/ACT(all) | LINK(10)/ACT(all) |  LINK(10)/ACT(10)
 *      LED1        |    LINK(100)  |       LINK(100)    |      LINK(100)    | LINK(100)/ACT(100)
 */
#define ETH_LED_MODE0                       0x00
#define ETH_LED_MODE1                       0x01
#define ETH_LED_MODE2                       0x02
#define ETH_LED_MODE3                       0x03
/**
 * @}
 */

/**
 * @defgroup LED_CONTROL
 * @brief LED Control Register LED_CONTROL
 * @addtogroup LED_CONTROL
 * @{
 */
typedef union {
    struct {
        uint32_t duty_cycle : 2;
        uint32_t led_freq_ctrl : 2;
        uint32_t reserved : 12;
    };
    uint32_t val;
} led_control_reg_t;

#define ETH_LED_CONTROL_REG_ADDR            0x15
#define ETH_LED_CONTROL_REG_PAGE            0x07

#define ETH_LED_FREQ_240MS                  0x00
#define ETH_LED_FREQ_160MS                  0x01
#define ETH_LED_FREQ_80MS                   0x02

#define ETH_LED_DUTY_12_5                   0x00
#define ETH_LED_DUTY_25                     0x01
#define ETH_LED_DUTY_50                     0x02
#define ETH_LED_DUTY_75                     0x03
/**
 * @}
 */

/**
 * @note Default Settings, change them if necessary. Try to avoid touching src file!
 */
#define ETH_DEFAULT_LED_MODE                ETH_LED_MODE0
#define ETH_DEFAULT_LED_FREQ                ETH_LED_FREQ_80MS
#define ETH_DEFAULT_LED_DUTY                ETH_LED_DUTY_75
