/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

/**
 * @brief W6100 SPI Frame Format
 *
 * The W6100 SPI frame consists of:
 * - Address Phase: 16-bit offset address
 * - Control Phase: 8-bit (BSB[4:0] + RWB + OM[1:0])
 * - Data Phase: Variable length (VDM) or fixed 1/2/4 bytes (FDM)
 */

#define W6100_ADDR_OFFSET (16)  // Address length in bits
#define W6100_BSB_OFFSET  (3)   // Block Select Bits offset in control phase
#define W6100_RWB_OFFSET  (2)   // Read/Write Bit offset in control phase

/**
 * @brief Chip identification
 */
#define W6100_CHIP_ID      (0x6100)  // Chip ID from CIDR register
#define W6100_CHIP_VERSION (0x4661)  // Version from VER register

/**
 * @brief Block Select Bits (BSB) values
 * W6100 has 1 common register block, 8 socket register blocks,
 * 8 socket TX buffer blocks, and 8 socket RX buffer blocks.
 */
#define W6100_BSB_COM_REG        (0x00)       // Common Register block
#define W6100_BSB_SOCK_REG(s)    (((s) << 2) + 1)  // Socket n Register block
#define W6100_BSB_SOCK_TX_BUF(s) (((s) << 2) + 2)  // Socket n TX Buffer block
#define W6100_BSB_SOCK_RX_BUF(s) (((s) << 2) + 3)  // Socket n RX Buffer block

/**
 * @brief SPI Access Mode
 */
#define W6100_ACCESS_MODE_READ  (0)  // Read access
#define W6100_ACCESS_MODE_WRITE (1)  // Write access

/**
 * @brief SPI Operation Mode (OM bits in control phase)
 */
#define W6100_SPI_OP_MODE_VDM   (0x00)  // Variable Data Length Mode
#define W6100_SPI_OP_MODE_FDM_1 (0x01)  // Fixed Data Length Mode, 1 byte
#define W6100_SPI_OP_MODE_FDM_2 (0x02)  // Fixed Data Length Mode, 2 bytes
#define W6100_SPI_OP_MODE_FDM_4 (0x03)  // Fixed Data Length Mode, 4 bytes

/**
 * @brief Macro to create register map address from offset and BSB
 */
#define W6100_MAKE_MAP(offset, bsb) (((offset) << W6100_ADDR_OFFSET) | ((bsb) << W6100_BSB_OFFSET))

/*******************************************************************************
 * Common Register Definitions (BSB = 0x00)
 ******************************************************************************/

/**
 * @brief Chip Identification Register
 */
#define W6100_REG_CIDR      W6100_MAKE_MAP(0x0000, W6100_BSB_COM_REG)  // Chip ID (0x6100)

/**
 * @brief Version Register
 */
#define W6100_REG_VER       W6100_MAKE_MAP(0x0002, W6100_BSB_COM_REG)  // Version (0x4661)

/**
 * @brief System Status Register (SYSR)
 */
#define W6100_REG_SYSR      W6100_MAKE_MAP(0x2000, W6100_BSB_COM_REG)

/**
 * @brief System Config Register 0 (SYCR0) - Software Reset
 */
#define W6100_REG_SYCR0     W6100_MAKE_MAP(0x2004, W6100_BSB_COM_REG)

/**
 * @brief System Config Register 1 (SYCR1) - Interrupt Enable, Clock Select
 */
#define W6100_REG_SYCR1     W6100_MAKE_MAP(0x2005, W6100_BSB_COM_REG)

/**
 * @brief Interrupt Register (IR)
 */
#define W6100_REG_IR        W6100_MAKE_MAP(0x2100, W6100_BSB_COM_REG)

/**
 * @brief Socket Interrupt Register (SIR)
 */
#define W6100_REG_SIR       W6100_MAKE_MAP(0x2101, W6100_BSB_COM_REG)

/**
 * @brief Socket-less Interrupt Register (SLIR)
 */
#define W6100_REG_SLIR      W6100_MAKE_MAP(0x2102, W6100_BSB_COM_REG)

/**
 * @brief Interrupt Mask Register (IMR)
 */
#define W6100_REG_IMR       W6100_MAKE_MAP(0x2104, W6100_BSB_COM_REG)

/**
 * @brief IR Clear Register (IRCLR)
 */
#define W6100_REG_IRCLR     W6100_MAKE_MAP(0x2108, W6100_BSB_COM_REG)

/**
 * @brief Socket Interrupt Mask Register (SIMR)
 */
#define W6100_REG_SIMR      W6100_MAKE_MAP(0x2114, W6100_BSB_COM_REG)

/**
 * @brief PHY Status Register (PHYSR)
 */
#define W6100_REG_PHYSR     W6100_MAKE_MAP(0x3000, W6100_BSB_COM_REG)

/**
 * @brief PHY Register Address Register (PHYRAR)
 */
#define W6100_REG_PHYRAR    W6100_MAKE_MAP(0x3008, W6100_BSB_COM_REG)

/**
 * @brief PHY Data Input Register (PHYDIR)
 */
#define W6100_REG_PHYDIR    W6100_MAKE_MAP(0x300C, W6100_BSB_COM_REG)

/**
 * @brief PHY Data Output Register (PHYDOR)
 */
#define W6100_REG_PHYDOR    W6100_MAKE_MAP(0x3010, W6100_BSB_COM_REG)

/**
 * @brief PHY Access Control Register (PHYACR)
 */
#define W6100_REG_PHYACR    W6100_MAKE_MAP(0x3014, W6100_BSB_COM_REG)

/**
 * @brief PHY Division Register (PHYDIVR)
 */
#define W6100_REG_PHYDIVR   W6100_MAKE_MAP(0x3018, W6100_BSB_COM_REG)

/**
 * @brief PHY Control Register 0 (PHYCR0)
 */
#define W6100_REG_PHYCR0    W6100_MAKE_MAP(0x301C, W6100_BSB_COM_REG)

/**
 * @brief PHY Control Register 1 (PHYCR1)
 */
#define W6100_REG_PHYCR1    W6100_MAKE_MAP(0x301D, W6100_BSB_COM_REG)

/**
 * @brief Network IPv4 Mode Register (NET4MR)
 */
#define W6100_REG_NET4MR    W6100_MAKE_MAP(0x4000, W6100_BSB_COM_REG)

/**
 * @brief Network IPv6 Mode Register (NET6MR)
 */
#define W6100_REG_NET6MR    W6100_MAKE_MAP(0x4004, W6100_BSB_COM_REG)

/**
 * @brief Network Mode Register (NETMR)
 */
#define W6100_REG_NETMR     W6100_MAKE_MAP(0x4008, W6100_BSB_COM_REG)

/**
 * @brief Network Mode Register 2 (NETMR2)
 */
#define W6100_REG_NETMR2    W6100_MAKE_MAP(0x4009, W6100_BSB_COM_REG)

/**
 * @brief Source Hardware Address Register (SHAR) - MAC Address
 */
#define W6100_REG_SHAR      W6100_MAKE_MAP(0x4120, W6100_BSB_COM_REG)

/**
 * @brief Gateway IP Address Register (GAR)
 */
#define W6100_REG_GAR       W6100_MAKE_MAP(0x4130, W6100_BSB_COM_REG)

/**
 * @brief Subnet Mask Register (SUBR)
 */
#define W6100_REG_SUBR      W6100_MAKE_MAP(0x4134, W6100_BSB_COM_REG)

/**
 * @brief IPv4 Source Address Register (SIPR)
 */
#define W6100_REG_SIPR      W6100_MAKE_MAP(0x4138, W6100_BSB_COM_REG)

/**
 * @brief Link Local Address Register (LLAR) - IPv6
 */
#define W6100_REG_LLAR      W6100_MAKE_MAP(0x4140, W6100_BSB_COM_REG)

/**
 * @brief Global Unicast Address Register (GUAR) - IPv6
 */
#define W6100_REG_GUAR      W6100_MAKE_MAP(0x4150, W6100_BSB_COM_REG)

/**
 * @brief IPv6 Subnet Prefix Register (SUB6R)
 */
#define W6100_REG_SUB6R     W6100_MAKE_MAP(0x4160, W6100_BSB_COM_REG)

/**
 * @brief IPv6 Gateway Address Register (GA6R)
 */
#define W6100_REG_GA6R      W6100_MAKE_MAP(0x4170, W6100_BSB_COM_REG)

/**
 * @brief Interrupt Pending Time Register (INTPTMR)
 */
#define W6100_REG_INTPTMR   W6100_MAKE_MAP(0x41C5, W6100_BSB_COM_REG)

/**
 * @brief Chip Lock Register (CHPLCKR)
 */
#define W6100_REG_CHPLCKR   W6100_MAKE_MAP(0x41F4, W6100_BSB_COM_REG)

/**
 * @brief Network Lock Register (NETLCKR)
 */
#define W6100_REG_NETLCKR   W6100_MAKE_MAP(0x41F5, W6100_BSB_COM_REG)

/**
 * @brief PHY Lock Register (PHYLCKR)
 */
#define W6100_REG_PHYLCKR   W6100_MAKE_MAP(0x41F6, W6100_BSB_COM_REG)

/**
 * @brief Retransmission Time Register (RTR)
 */
#define W6100_REG_RTR       W6100_MAKE_MAP(0x4200, W6100_BSB_COM_REG)

/**
 * @brief Retransmission Count Register (RCR)
 */
#define W6100_REG_RCR       W6100_MAKE_MAP(0x4204, W6100_BSB_COM_REG)

/*******************************************************************************
 * Socket Register Definitions (per socket)
 ******************************************************************************/

/**
 * @brief Socket Mode Register (Sn_MR)
 */
#define W6100_REG_SOCK_MR(s)         W6100_MAKE_MAP(0x0000, W6100_BSB_SOCK_REG(s))

/**
 * @brief Socket Prefer Source IPv6 Address Register (Sn_PSR)
 */
#define W6100_REG_SOCK_PSR(s)        W6100_MAKE_MAP(0x0004, W6100_BSB_SOCK_REG(s))

/**
 * @brief Socket Command Register (Sn_CR)
 */
#define W6100_REG_SOCK_CR(s)         W6100_MAKE_MAP(0x0010, W6100_BSB_SOCK_REG(s))

/**
 * @brief Socket Interrupt Register (Sn_IR)
 */
#define W6100_REG_SOCK_IR(s)         W6100_MAKE_MAP(0x0020, W6100_BSB_SOCK_REG(s))

/**
 * @brief Socket Interrupt Mask Register (Sn_IMR)
 */
#define W6100_REG_SOCK_IMR(s)        W6100_MAKE_MAP(0x0024, W6100_BSB_SOCK_REG(s))

/**
 * @brief Socket IR Clear Register (Sn_IRCLR)
 */
#define W6100_REG_SOCK_IRCLR(s)      W6100_MAKE_MAP(0x0028, W6100_BSB_SOCK_REG(s))

/**
 * @brief Socket Status Register (Sn_SR)
 */
#define W6100_REG_SOCK_SR(s)         W6100_MAKE_MAP(0x0030, W6100_BSB_SOCK_REG(s))

/**
 * @brief Socket Extension Status Register (Sn_ESR)
 */
#define W6100_REG_SOCK_ESR(s)        W6100_MAKE_MAP(0x0031, W6100_BSB_SOCK_REG(s))

/**
 * @brief Socket TX Buffer Size Register (Sn_TX_BSR)
 */
#define W6100_REG_SOCK_TX_BSR(s)     W6100_MAKE_MAP(0x0200, W6100_BSB_SOCK_REG(s))

/**
 * @brief Socket TX Free Size Register (Sn_TX_FSR)
 */
#define W6100_REG_SOCK_TX_FSR(s)     W6100_MAKE_MAP(0x0204, W6100_BSB_SOCK_REG(s))

/**
 * @brief Socket TX Read Pointer Register (Sn_TX_RD)
 */
#define W6100_REG_SOCK_TX_RD(s)      W6100_MAKE_MAP(0x0208, W6100_BSB_SOCK_REG(s))

/**
 * @brief Socket TX Write Pointer Register (Sn_TX_WR)
 */
#define W6100_REG_SOCK_TX_WR(s)      W6100_MAKE_MAP(0x020C, W6100_BSB_SOCK_REG(s))

/**
 * @brief Socket RX Buffer Size Register (Sn_RX_BSR)
 */
#define W6100_REG_SOCK_RX_BSR(s)     W6100_MAKE_MAP(0x0220, W6100_BSB_SOCK_REG(s))

/**
 * @brief Socket RX Received Size Register (Sn_RX_RSR)
 */
#define W6100_REG_SOCK_RX_RSR(s)     W6100_MAKE_MAP(0x0224, W6100_BSB_SOCK_REG(s))

/**
 * @brief Socket RX Read Pointer Register (Sn_RX_RD)
 */
#define W6100_REG_SOCK_RX_RD(s)      W6100_MAKE_MAP(0x0228, W6100_BSB_SOCK_REG(s))

/**
 * @brief Socket RX Write Pointer Register (Sn_RX_WR)
 */
#define W6100_REG_SOCK_RX_WR(s)      W6100_MAKE_MAP(0x022C, W6100_BSB_SOCK_REG(s))

/*******************************************************************************
 * TX/RX Buffer Access
 ******************************************************************************/

/**
 * @brief Socket TX Buffer access
 */
#define W6100_MEM_SOCK_TX(s, addr)   W6100_MAKE_MAP(addr, W6100_BSB_SOCK_TX_BUF(s))

/**
 * @brief Socket RX Buffer access
 */
#define W6100_MEM_SOCK_RX(s, addr)   W6100_MAKE_MAP(addr, W6100_BSB_SOCK_RX_BUF(s))

/*******************************************************************************
 * Register Bit Definitions
 ******************************************************************************/

/**
 * @brief SYCR0 (System Config Register 0) bits
 */
#define W6100_SYCR0_RST     (1 << 7)  // Software Reset (write 0 to reset)

/**
 * @brief SYCR1 (System Config Register 1) bits
 */
#define W6100_SYCR1_IEN     (1 << 7)  // Interrupt Enable
#define W6100_SYCR1_CLKSEL  (1 << 0)  // Clock Select (0=100MHz, 1=25MHz)

/**
 * @brief SYSR (System Status Register) bits
 */
#define W6100_SYSR_CHPL     (1 << 7)  // Chip Lock status
#define W6100_SYSR_NETL     (1 << 6)  // Network Lock status
#define W6100_SYSR_PHYL     (1 << 5)  // PHY Lock status
#define W6100_SYSR_IND      (1 << 1)  // Indirect BUS mode
#define W6100_SYSR_SPI      (1 << 0)  // SPI mode

/**
 * @brief PHYSR (PHY Status Register) bits
 */
#define W6100_PHYSR_CAB     (1 << 7)  // Cable OFF (1=unplugged)
#define W6100_PHYSR_MODE_MASK (0x38) // MODE[2:0] at bits 5:3
#define W6100_PHYSR_MODE_SHIFT (3)
#define W6100_PHYSR_DPX     (1 << 2)  // Duplex (1=half, 0=full)
#define W6100_PHYSR_SPD     (1 << 1)  // Speed (1=10Mbps, 0=100Mbps)
#define W6100_PHYSR_LNK     (1 << 0)  // Link (1=up, 0=down)

/**
 * @brief PHYCR0 (PHY Control Register 0) - Operation Mode
 */
#define W6100_PHYCR0_AUTO   (0x00)  // Auto Negotiation
#define W6100_PHYCR0_100FDX (0x04)  // 100BASE-TX Full Duplex
#define W6100_PHYCR0_100HDX (0x05)  // 100BASE-TX Half Duplex
#define W6100_PHYCR0_10FDX  (0x06)  // 10BASE-T Full Duplex
#define W6100_PHYCR0_10HDX  (0x07)  // 10BASE-T Half Duplex

/**
 * @brief PHYCR1 (PHY Control Register 1) bits
 */
#define W6100_PHYCR1_PWDN   (1 << 5)  // PHY Power Down
#define W6100_PHYCR1_TE     (1 << 3)  // 10BASE-Te Mode
#define W6100_PHYCR1_RST    (1 << 0)  // PHY Reset

/**
 * @brief NET4MR (Network IPv4 Mode Register) bits
 */
#define W6100_NET4MR_PB     (1 << 0)  // PINGv4 Reply Block

/**
 * @brief NET6MR (Network IPv6 Mode Register) bits
 */
#define W6100_NET6MR_PB     (1 << 0)  // PINGv6 Reply Block

/**
 * @brief NETMR (Network Mode Register) bits
 */
#define W6100_NETMR_ANB     (1 << 5)  // IPv6 All-Node Block
#define W6100_NETMR_M6B     (1 << 4)  // IPv6 Multicast Block
#define W6100_NETMR_WOL     (1 << 2)  // Wake On LAN
#define W6100_NETMR_IP6B    (1 << 1)  // IPv6 Block
#define W6100_NETMR_IP4B    (1 << 0)  // IPv4 Block

/**
 * @brief Lock Register values
 */
#define W6100_CHPLCKR_UNLOCK  (0xCE)  // Unlock chip config
#define W6100_NETLCKR_UNLOCK  (0x3A)  // Unlock network config
#define W6100_PHYLCKR_UNLOCK  (0x53)  // Unlock PHY config

/**
 * @brief SIMR (Socket Interrupt Mask Register) bits
 */
#define W6100_SIMR_SOCK0    (1 << 0)  // Socket 0 interrupt mask

/**
 * @brief Socket Mode Register (Sn_MR) bits for MACRAW mode Note: This driver
 * only uses a single socket in MACRAW mode. Other modes like TCP/UDP have
 * different bit meanings at the same positions.
 */
#define W6100_SMR_MF        (1 << 7)  // MAC Filter: 0=recv all, 1=filter by SHAR/bcast/mcast
#define W6100_SMR_MMB       (1 << 5)  // IPv4 Multicast Block: 0=recv, 1=block
#define W6100_SMR_MMB6      (1 << 4)  // IPv6 Multicast Block: 0=recv, 1=block
#define W6100_SMR_MACRAW    (0x07)    // MACRAW protocol mode

/**
 * @brief Socket Command Register (Sn_CR) commands for MACRAW mode
 */
#define W6100_SCR_OPEN      (0x01)  // Open socket
#define W6100_SCR_CLOSE     (0x10)  // Close socket
#define W6100_SCR_SEND      (0x20)  // Send data from TX buffer
#define W6100_SCR_RECV      (0x40)  // Update RX read pointer after reading

/**
 * @brief Socket Interrupt Register (Sn_IR) bits
 * Note: MACRAW mode uses RECV (for RX notification) and SENDOK (to confirm TX).
 * TIMEOUT/CON/DISCON are for TCP/UDP connection management.
 */
#define W6100_SIR_SENDOK    (1 << 4)  // Send completed
#define W6100_SIR_RECV      (1 << 2)  // Data received in RX buffer

/*******************************************************************************
 * Memory Configuration
 ******************************************************************************/

#define W6100_TX_MEM_SIZE   (0x4000)  // 16KB TX memory
#define W6100_RX_MEM_SIZE   (0x4000)  // 16KB RX memory
