/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * Register definitions for LAN865x Ethernet controller
 *
 * NOTICE: This file was originally automatically generated from the LAN865x datasheet.
 *         The register definitions are not guaranteed to be correct.
 *         Double check the register definitions before the first usage!!!
 */

#pragma once

#include <stdint.h>

/* MMS - Memory Map Selectors */
typedef enum {
    LAN865X_MMS_OA = 0,
    LAN865X_MMS_MAC = 1,
    LAN865X_MMS_PHY_PCS = 2,
    LAN865X_MMS_PHY_PMA = 3,
    LAN865X_MMS_PHY_VENDOR = 4,
    LAN865X_MMS_MISC = 10
} lan865x_mms_t;

/******************************** Open Alliance Registers ********************************/

#define LAN865X_OA_CONFIG0_RECV_FRAME_ALIGN_ANY         (0x0) // Receive Ethernet frames may begin at any word of any receive data block
#define LAN865X_OA_CONFIG0_RECV_FRAME_ALIGN_ZERO        (0x1) // Receive Ethernet frames will begin only at the first word of the receive data payload in any data block
#define LAN865X_OA_CONFIG0_RECV_FRAME_ALIGN_ZERO_CS     (0x2) //Receive Ethernet frames only begin in the first word of the first receive data block payload following assertion

#define LAN865X_OA_CONFIG0_TRANSMIT_CREDIT_THRESHOLD_1_BLK    (0x0) // IRQ_N will be asserted when at least 1 data block available
#define LAN865X_OA_CONFIG0_TRANSMIT_CREDIT_THRESHOLD_4_BLK    (0x1) // IRQ_N will be asserted when at least 4 data blocks available
#define LAN865X_OA_CONFIG0_TRANSMIT_CREDIT_THRESHOLD_8_BLK    (0x2) // IRQ_N will be asserted when at least 8 data blocks available
#define LAN865X_OA_CONFIG0_TRANSMIT_CREDIT_THRESHOLD_16_BLK   (0x3) // IRQ_N will be asserted when at least 16 data blocks available

#define LAN865X_OA_CONFIG0_BLOCK_PAYLOAD_SIZE_32 (0x5) // 32 bytes
#define LAN865X_OA_CONFIG0_BLOCK_PAYLOAD_SIZE_64 (0x6) // 64 bytes

/* OA_PHY_REG_OFFSET - Offset for PHY Registers */
#define LAN865X_OA_PHY_REG_OFFSET (0xFF00)

/* OA_ID - Identification Register */
typedef union {
    struct {
        uint32_t minver : 4;    /*!< Minor Version */
        uint32_t majver : 4;    /*!< Major Version */
        uint32_t reserved : 24; /*!< Reserved */
    };
    uint32_t val;
} lan865x_oa_id_reg_t;
#define LAN865X_OA_ID_REG_ADDR (0x00)

/* OA_PHYID - PHY Identification Register */
typedef union {
    struct {
        uint32_t revision : 4;  /*!< Manufacturer's Revision Number */
        uint32_t model : 6;     /*!< Manufacturer's Model Number */
        uint32_t oui : 22;      /*!< Organizationally Unique Identifier */
    };
    uint32_t val;
} lan865x_oa_phyid_reg_t;
#define LAN865X_OA_PHYID_REG_ADDR (0x01)

/* OA_STDCAP - Standard Capabilities Register */
typedef union {
    struct {
        uint32_t minbps : 3;    /*!< Minimum Block Payload Size Capability */
        uint32_t reserved1 : 1; /*!< Reserved */
        uint32_t seqc : 1;      /*!< Transmit Data Block Sequence and Retry Capability */
        uint32_t aidc : 1;      /*!< Address Increment Disable Capability */
        uint32_t ftsc : 1;      /*!< Frame Timestamp Capability */
        uint32_t ctc : 1;       /*!< Cut-through Capability */
        uint32_t dprac : 1;     /*!< Direct PHY Register Access Capability */
        uint32_t iprac : 1;     /*!< Indirect PHY Register access Capability */
        uint32_t txfcsvc : 1;   /*!< Transmit Frame Check Sequence Validation Capability */
        uint32_t reserved2 : 22; /*!< Reserved */
    };
    uint32_t val;
} lan865x_oa_stdcap_reg_t;
#define LAN865X_OA_STDCAP_REG_ADDR (0x02)

/* OA_RESET - Reset Control and Status Register */
typedef union {
    struct {
        uint32_t swreset : 1;   /*!< Software Reset */
        uint32_t reserved : 31; /*!< Reserved */
    };
    uint32_t val;
} lan865x_oa_reset_reg_t;
#define LAN865X_OA_RESET_REG_ADDR (0x03)

/* OA_CONFIG0 - Configuration 0 Register */
typedef union {
    struct {
        uint32_t bps : 3;       /*!< Block Payload Size */
        uint32_t reserved1 : 1; /*!< Reserved */
        uint32_t seqe : 1;      /*!< Transmit data block header Sequence bit support Enable */
        uint32_t prote : 1;     /*!< Control Data Read/Write Protection Enable */
        uint32_t ftss : 1;      /*!< Frame Timestamp Select */
        uint32_t ftse : 1;      /*!< Frame Timestamp Enable */
        uint32_t rxcte : 1;     /*!< Receive Cut-Through Enable */
        uint32_t txcte : 1;     /*!< Transmit Cut-Through Enable */
        uint32_t txcthresh : 2; /*!< Transmit Credit Threshold */
        uint32_t rfa : 2;       /*!< Receive Frame Alignment */
        uint32_t txfcsve : 1;   /*!< Transmit Frame Check Sequence Validation Enable */
        uint32_t sync : 1;      /*!< Configuration Synchronization */
        uint32_t reserved2 : 16; /*!< Reserved */
    };
    uint32_t val;
} lan865x_oa_config0_reg_t;
#define LAN865X_OA_CONFIG0_REG_ADDR (0x04)

/* OA_STATUS0 - Status 0 Register */
typedef union {
    struct {
        uint32_t txpe : 1;      /*!< Transmit Protocol Error Status */
        uint32_t txboe : 1;     /*!< Transmit Buffer Overflow Error Status */
        uint32_t txbue : 1;     /*!< Transmit Buffer Underflow Error Status */
        uint32_t rxboe : 1;     /*!< Receive Buffer Overflow Error Status */
        uint32_t lofe : 1;      /*!< Loss of Framing Error Status */
        uint32_t hdre : 1;      /*!< Header Error Status */
        uint32_t resetc : 1;    /*!< Reset Complete */
        uint32_t phyint : 1;    /*!< PHY Interrupt */
        uint32_t ttscaa : 1;    /*!< Transmit Timestamp Capture Available A */
        uint32_t ttscab : 1;    /*!< Transmit Timestamp Capture Available B */
        uint32_t ttscac : 1;    /*!< Transmit Timestamp Capture Available C */
        uint32_t txfcse : 1;    /*!< Transmit Frame Check Sequence Error */
        uint32_t cpde : 1;      /*!< Control Data Protection Error */
        uint32_t reserved : 19; /*!< Reserved */
    };
    uint32_t val;
} lan865x_oa_status0_reg_t;
#define LAN865X_OA_STATUS0_REG_ADDR (0x08)

/* OA_STATUS1 - Status 1 Register */
typedef union {
    struct {
        uint32_t rxner : 1;     /*!< Receive Non-Recoverable Error Status */
        uint32_t txner : 1;     /*!< Transmit Non-Recoverable Error Status */
        uint32_t reserved1 : 15; /*!< Reserved */
        uint32_t fsmster : 1;   /*!< FSM State Error Status */
        uint32_t ecc : 1;       /*!< SRAM ECC Error Status */
        uint32_t uv18 : 1;      /*!< 1.8V supply Under-Voltage Status */
        uint32_t buser : 1;     /*!< Internal Bus Error */
        uint32_t ttscofa : 1;   /*!< Transmit Timestamp Capture Overflow A */
        uint32_t ttscofb : 1;   /*!< Transmit Timestamp Capture Overflow B */
        uint32_t ttscofc : 1;   /*!< Transmit Timestamp Capture Overflow C */
        uint32_t ttscma : 1;    /*!< Transmit Timestamp Capture Missed A */
        uint32_t ttscmb : 1;    /*!< Transmit Timestamp Capture Missed B */
        uint32_t ttscmc : 1;    /*!< Transmit Timestamp Capture Missed C */
        uint32_t reserved2 : 1; /*!< Reserved */
        uint32_t sev : 1;       /*!< Synchronization Event */
        uint32_t reserved3 : 3; /*!< Reserved */
    };
    uint32_t val;
} lan865x_oa_status1_reg_t;
#define LAN865X_OA_STATUS1_REG_ADDR (0x09)

/* OA_BUFSTS - Buffer Status Register */
typedef union {
    struct {
        uint32_t rba : 8;       /*!< Receive Blocks Available */
        uint32_t txc : 8;       /*!< Transmit Credits Available */
        uint32_t reserved : 16; /*!< Reserved */
    };
    uint32_t val;
} lan865x_oa_bufsts_reg_t;
#define LAN865X_OA_BUFSTS_REG_ADDR (0x0B)

/* OA_IMASK0 - Interrupt Mask 0 Register */
typedef union {
    struct {
        uint32_t txpem : 1;     /*!< Transmit Protocol Error Interrupt Mask */
        uint32_t txboem : 1;    /*!< Transmit Buffer Overflow Error Interrupt Mask */
        uint32_t txbuem : 1;    /*!< Transmit Buffer Underflow Error Interrupt Mask */
        uint32_t rxboem : 1;    /*!< Receive Buffer Overflow Error Interrupt Mask */
        uint32_t lofem : 1;     /*!< Loss of Framing Error Interrupt Mask */
        uint32_t hdrem : 1;     /*!< Header Error Interrupt Mask */
        uint32_t resetcm : 1;   /*!< Reset Complete Interrupt Mask */
        uint32_t phyintm : 1;   /*!< PHY Interrupt Mask */
        uint32_t ttscaam : 1;   /*!< Transmit Timestamp Capture Available A Interrupt Mask */
        uint32_t ttscabm : 1;   /*!< Transmit Timestamp Capture Available B Interrupt Mask */
        uint32_t ttscacm : 1;   /*!< Transmit Timestamp Capture Available C Interrupt Mask */
        uint32_t txfcsem : 1;   /*!< Transmit Frame Check Sequence Error Interrupt Mask */
        uint32_t cpdem : 1;     /*!< Control Data Protection Error Interrupt Mask */
        uint32_t reserved : 19; /*!< Reserved */
    };
    uint32_t val;
} lan865x_oa_imask0_reg_t;
#define LAN865X_OA_IMASK0_REG_ADDR (0x0C)

/* OA_IMASK1 - Interrupt Mask 1 Register */
typedef union {
    struct {
        uint32_t rxnerm : 1;    /*!< Receive Non-Recoverable Error Interrupt Mask */
        uint32_t txnerm : 1;    /*!< Transmit Non-Recoverable Error Interrupt Mask */
        uint32_t reserved1 : 15; /*!< Reserved */
        uint32_t fsmsterm : 1;  /*!< FSM State Error Interrupt Mask */
        uint32_t eccm : 1;      /*!< SRAM ECC Error Interrupt Mask */
        uint32_t uv18m : 1;     /*!< 1.8V supply Under-Voltage Interrupt Mask */
        uint32_t buserm : 1;    /*!< Internal Bus Error Interrupt Mask */
        uint32_t ttscofam : 1;  /*!< Transmit Timestamp Capture Overflow A Interrupt Mask */
        uint32_t ttscofbm : 1;  /*!< Transmit Timestamp Capture Overflow B Interrupt Mask */
        uint32_t ttscofcm : 1;  /*!< Transmit Timestamp Capture Overflow C Interrupt Mask */
        uint32_t ttscmam : 1;   /*!< Transmit Timestamp Capture Missed A Interrupt Mask */
        uint32_t ttscmbm : 1;   /*!< Transmit Timestamp Capture Missed B Interrupt Mask */
        uint32_t ttscmcm : 1;   /*!< Transmit Timestamp Capture Missed C Interrupt Mask */
        uint32_t reserved2 : 1; /*!< Reserved */
        uint32_t sevm : 1;      /*!< Synchronization Event Interrupt Mask */
        uint32_t reserved3 : 3; /*!< Reserved */
    };
    uint32_t val;
} lan865x_oa_imask1_reg_t;
#define LAN865X_OA_IMASK1_REG_ADDR (0x0D)

/* TTSCAH - Transmit Timestamp Capture A (High) */
typedef union {
    struct {
        uint32_t timestampa_high : 32; /*!< Upper 32 bits of 64-bit captured egress timestamp A */
    };
    uint32_t val;
} lan865x_ttscah_reg_t;
#define LAN865X_TTSCAH_REG_ADDR (0x10)

/* TTSCAL - Transmit Timestamp Capture A (Low) */
typedef union {
    struct {
        uint32_t timestampa_low : 32; /*!< Lower 32 bits of 64-bit captured egress timestamp A */
    };
    uint32_t val;
} lan865x_ttscal_reg_t;
#define LAN865X_TTSCAL_REG_ADDR (0x11)

/* TTSCBH - Transmit Timestamp Capture B (High) */
typedef union {
    struct {
        uint32_t timestampb_high : 32; /*!< Upper 32 bits of 64-bit captured egress timestamp B */
    };
    uint32_t val;
} lan865x_ttscbh_reg_t;
#define LAN865X_TTSCBH_REG_ADDR (0x12)

/* TTSCBL - Transmit Timestamp Capture B (Low) */
typedef union {
    struct {
        uint32_t timestampb_low : 32; /*!< Lower 32 bits of 64-bit captured egress timestamp B */
    };
    uint32_t val;
} lan865x_ttscbl_reg_t;
#define LAN865X_TTSCBL_REG_ADDR (0x13)

/* TTSCCH - Transmit Timestamp Capture C (High) */
typedef union {
    struct {
        uint32_t timestampc_high : 32; /*!< Upper 32 bits of 64-bit captured egress timestamp C */
    };
    uint32_t val;
} lan865x_ttscch_reg_t;
#define LAN865X_TTSCCH_REG_ADDR (0x14)

/* TTSCCL - Transmit Timestamp Capture C (Low) */
typedef union {
    struct {
        uint32_t timestampc_low : 32; /*!< Lower 32 bits of 64-bit captured egress timestamp C */
    };
    uint32_t val;
} lan865x_ttsccl_reg_t;
#define LAN865X_TTSCCL_REG_ADDR (0x15)

/* BASIC_CONTROL - Basic Control Register */
typedef union {
    struct {
        uint32_t reserved1 : 8;  /*!< Reserved */
        uint32_t duplexmd : 1;   /*!< Duplex Mode */
        uint32_t reautoneg : 1;  /*!< Restart Auto-Negotiation */
        uint32_t pd : 1;         /*!< Power Down */
        uint32_t autonegen : 1;  /*!< Auto-Negotiation Enable */
        uint32_t spd_sel0 : 1;   /*!< PHY Speed Select bit 0 */
        uint32_t loopback : 1;   /*!< Near-End Loopback */
        uint32_t sw_reset : 1;   /*!< PHY Soft Reset */
        uint32_t reserved2 : 17; /*!< Reserved */
    };
    uint32_t val;
} lan865x_basic_control_reg_t;
#define LAN865X_BASIC_CONTROL_REG_ADDR (0xFF00)

/* BASIC_STATUS - Basic Status Register */
typedef union {
    struct {
        uint32_t extcapa : 1;    /*!< Extended Capabilities Ability */
        uint32_t jabdet : 1;     /*!< Jabber Detection Status */
        uint32_t lnksts : 1;     /*!< Link Status */
        uint32_t autonega : 1;   /*!< Auto-Negotiation Ability */
        uint32_t rmtfltd : 1;    /*!< Remote Fault Detection */
        uint32_t autonegc : 1;   /*!< Auto-Negotiation Complete */
        uint32_t reserved1 : 2;  /*!< Reserved */
        uint32_t extsts : 1;     /*!< Extended status information ability */
        uint32_t bt2hda : 1;     /*!< 100BASE-T2 Half Duplex Ability */
        uint32_t bt2fda : 1;     /*!< 100BASE-T2 Full Duplex Ability */
        uint32_t bthda : 1;      /*!< 10BASE-T Half Duplex Ability */
        uint32_t btfda : 1;      /*!< 10BASE-T Full Duplex Ability */
        uint32_t btxhda : 1;     /*!< 100BASE-TX Half Duplex Ability */
        uint32_t btxfda : 1;     /*!< 100BASE-TX Full Duplex Ability */
        uint32_t bt4a : 1;       /*!< 100BASE-T4 Ability */
        uint32_t reserved2 : 16; /*!< Reserved */
    };
    uint32_t val;
} lan865x_basic_status_reg_t;
#define LAN865X_BASIC_STATUS_REG_ADDR (0xFF01)

/* PHY_ID1 - PHY Identifier 1 Register */
typedef union {
    struct {
        uint32_t oui_bits_10_17 : 8; /*!< Organizationally Unique Identifier bits 10 to 17 */
        uint32_t oui_bits_2_9 : 8;   /*!< Organizationally Unique Identifier bits 2 to 9 */
        uint32_t reserved : 16;      /*!< Reserved */
    };
    uint32_t val;
} lan865x_phy_id1_reg_t;
#define LAN865X_PHY_ID1_REG_ADDR (0xFF02)

/* PHY_ID2 - PHY Identifier 2 Register */
typedef union {
    struct {
        uint32_t rev : 4;        /*!< Manufacturer's Revision Number */
        uint32_t model : 6;      /*!< Manufacturer's Model Number */
        uint32_t oui_bits_18_23 : 6; /*!< Organizationally Unique Identifier bits 18 to 23 */
        uint32_t reserved : 16;  /*!< Reserved */
    };
    uint32_t val;
} lan865x_phy_id2_reg_t;
#define LAN865X_PHY_ID2_REG_ADDR (0xFF03)

/* MMDCTRL - MMD Access Control Register */
typedef union {
    struct {
        uint32_t devad : 5;     /*!< Device Address */
        uint32_t reserved1 : 9; /*!< Reserved */
        uint32_t fnctn : 2;     /*!< MMD Function */
        uint32_t reserved2 : 16; /*!< Reserved */
    };
    uint32_t val;
} lan865x_mmdctrl_reg_t;
#define LAN865X_MMDCTRL_REG_ADDR (0xFF0D)

/* MMDAD - MMD Access Address/Data Register */
typedef union {
    struct {
        uint32_t adr_data : 16; /*!< MMD Address / Data */
        uint32_t reserved : 16; /*!< Reserved */
    };
    uint32_t val;
} lan865x_mmdad_reg_t;
#define LAN865X_MMDAD_REG_ADDR (0xFF0E)

/******************************** MAC Registers ********************************/

/* MAC_NCR - MAC Network Control Register */
typedef union {
    struct {
        uint32_t reserved1 : 1; /*!< Reserved */
        uint32_t lbl : 1;       /*!< Loop Back Local */
        uint32_t rxen : 1;      /*!< Receive Enable */
        uint32_t txen : 1;      /*!< Transmit Enable */
        uint32_t reserved2 : 28; /*!< Reserved */
    };
    uint32_t val;
} lan865x_mac_ncr_reg_t;
#define LAN865X_MAC_NCR_REG_ADDR (0x00)

/* MAC_NCFGR - MAC Network Configuration Register */
typedef union {
    struct {
        uint32_t reserved1 : 2; /*!< Reserved */
        uint32_t dnvlan : 1;    /*!< Discard Non-VLAN Frames */
        uint32_t reserved2 : 1; /*!< Reserved */
        uint32_t calf : 1;       /*!< Copy All Frames */
        uint32_t nbc : 1;       /*!< No Broadcast */
        uint32_t mtihen : 1;    /*!< Multicast Hash Enable */
        uint32_t unihen : 1;    /*!< Unicast Hash Enable */
        uint32_t maxfs : 1;     /*!< 1536 Maximum Frame Size */
        uint32_t reserved3 : 7; /*!< Reserved */
        uint32_t lferd : 1;     /*!< Length Field Error Frame Discard */
        uint32_t rfcs : 1;      /*!< Remove FCS */
        uint32_t reserved4 : 7; /*!< Reserved */
        uint32_t efrhd : 1;     /*!< Enable Frames Received in half-duplex */
        uint32_t irxfcs : 1;    /*!< Ignore RX FCS */
        uint32_t reserved5 : 2; /*!< Reserved */
        uint32_t rxbp : 1;      /*!< Receive Bad Preamble */
        uint32_t reserved6 : 2; /*!< Reserved */
    };
    uint32_t val;
} lan865x_mac_ncfgr_reg_t;
#define LAN865X_MAC_NCFGR_REG_ADDR (0x01)

/* MAC_HRB - MAC Hash Register Bottom */
typedef union {
    struct {
        uint32_t addr : 32;     /*!< Hash Address (bits 31:0) */
    };
    uint32_t val;
} lan865x_mac_hrb_reg_t;
#define LAN865X_MAC_HRB_REG_ADDR (0x20)

/* MAC_HRT - MAC Hash Register Top */
typedef union {
    struct {
        uint32_t addr : 32;     /*!< Hash Address (bits 63:32) */
    };
    uint32_t val;
} lan865x_mac_hrt_reg_t;
#define LAN865X_MAC_HRT_REG_ADDR (0x21)

/* MAC_SAB1 - MAC Specific Address 1 Bottom Register */
typedef union {
    struct {
        uint32_t addr : 32;     /*!< Specific Address 1 (bits 31:0) */
    };
    uint32_t val;
} lan865x_mac_sab1_reg_t;
#define LAN865X_MAC_SAB1_REG_ADDR (0x22)

/* MAC_SAT1 - MAC Specific Address 1 Top Register */
typedef union {
    struct {
        uint32_t addr_39_32 : 8; /*!< Specific Address 1 (bits 39:32) */
        uint32_t addr_47_40 : 8; /*!< Specific Address 1 (bits 47:40) */
        uint32_t flttyp : 1;     /*!< Filter Type 1 */
        uint32_t reserved : 15;  /*!< Reserved */
    };
    uint32_t val;
} lan865x_mac_sat1_reg_t;
#define LAN865X_MAC_SAT1_REG_ADDR (0x23)

/* MAC_SAB2 - MAC Specific Address 2 Bottom Register */
typedef union {
    struct {
        uint32_t addr : 32;     /*!< Specific Address 2 (bits 31:0) */
    };
    uint32_t val;
} lan865x_mac_sab2_reg_t;
#define LAN865X_MAC_SAB2_REG_ADDR (0x24)

/* MAC_SAT2 - MAC Specific Address 2 Top Register */
typedef union {
    struct {
        uint32_t addr_39_32 : 8; /*!< Specific Address 2 (bits 39:32) */
        uint32_t addr_47_40 : 8; /*!< Specific Address 2 (bits 47:40) */
        uint32_t flttyp : 1;     /*!< Filter Type 2 */
        uint32_t reserved : 9;   /*!< Reserved */
        uint32_t fltbm : 6;      /*!< Filter Byte Mask 2 */
    };
    uint32_t val;
} lan865x_mac_sat2_reg_t;
#define LAN865X_MAC_SAT2_REG_ADDR (0x25)

/* MAC_SAB3 - MAC Specific Address 3 Bottom Register */
typedef union {
    struct {
        uint32_t addr : 32;     /*!< Specific Address 3 (bits 31:0) */
    };
    uint32_t val;
} lan865x_mac_sab3_reg_t;
#define LAN865X_MAC_SAB3_REG_ADDR (0x26)

/* MAC_SAT3 - MAC Specific Address 3 Top Register */
typedef union {
    struct {
        uint32_t addr_39_32 : 8; /*!< Specific Address 3 (bits 39:32) */
        uint32_t addr_47_40 : 8; /*!< Specific Address 3 (bits 47:40) */
        uint32_t flttyp : 1;     /*!< Filter Type 3 */
        uint32_t reserved : 9;   /*!< Reserved */
        uint32_t fltbm : 6;      /*!< Filter Byte Mask 3 */
    };
    uint32_t val;
} lan865x_mac_sat3_reg_t;
#define LAN865X_MAC_SAT3_REG_ADDR (0x27)

/* MAC_SAB4 - MAC Specific Address 4 Bottom Register */
typedef union {
    struct {
        uint32_t addr : 32;     /*!< Specific Address 4 (bits 31:0) */
    };
    uint32_t val;
} lan865x_mac_sab4_reg_t;
#define LAN865X_MAC_SAB4_REG_ADDR (0x28)

/* MAC_SAT4 - MAC Specific Address 4 Top Register */
typedef union {
    struct {
        uint32_t addr_39_32 : 8; /*!< Specific Address 4 (bits 39:32) */
        uint32_t addr_47_40 : 8; /*!< Specific Address 4 (bits 47:40) */
        uint32_t flttyp : 1;     /*!< Filter Type 4 */
        uint32_t reserved : 9;   /*!< Reserved */
        uint32_t fltbm : 6;      /*!< Filter Byte Mask 4 */
    };
    uint32_t val;
} lan865x_mac_sat4_reg_t;
#define LAN865X_MAC_SAT4_REG_ADDR (0x29)

/* MAC_TIDM1 - MAC Type ID Match 1 Register */
typedef union {
    struct {
        uint32_t tid : 16;      /*!< Type ID Match 1 */
        uint32_t reserved : 15; /*!< Reserved */
        uint32_t enid : 1;      /*!< Enable Copying of TID 1 Matched Frames */
    };
    uint32_t val;
} lan865x_mac_tidm1_reg_t;
#define LAN865X_MAC_TIDM1_REG_ADDR (0x2A)

/* MAC_TIDM2 - MAC Type ID Match 2 Register */
typedef union {
    struct {
        uint32_t tid : 16;      /*!< Type ID Match 2 */
        uint32_t reserved : 15; /*!< Reserved */
        uint32_t enid : 1;      /*!< Enable Copying of TID 2 Matched Frames */
    };
    uint32_t val;
} lan865x_mac_tidm2_reg_t;
#define LAN865X_MAC_TIDM2_REG_ADDR (0x2B)

/* MAC_TIDM3 - MAC Type ID Match 3 Register */
typedef union {
    struct {
        uint32_t tid : 16;      /*!< Type ID Match 3 */
        uint32_t reserved : 15; /*!< Reserved */
        uint32_t enid : 1;      /*!< Enable Copying of TID 3 Matched Frames */
    };
    uint32_t val;
} lan865x_mac_tidm3_reg_t;
#define LAN865X_MAC_TIDM3_REG_ADDR (0x2C)

/* MAC_TIDM4 - MAC Type ID Match 4 Register */
typedef union {
    struct {
        uint32_t tid : 16;      /*!< Type ID Match 4 */
        uint32_t reserved : 15; /*!< Reserved */
        uint32_t enid : 1;      /*!< Enable Copying of TID 4 Matched Frames */
    };
    uint32_t val;
} lan865x_mac_tidm4_reg_t;
#define LAN865X_MAC_TIDM4_REG_ADDR (0x2D)

/* MAC_SAMB1 - MAC Specific Address 1 Mask Bottom */
typedef union {
    struct {
        uint32_t addr : 32;     /*!< Specific Address 1 Mask (bits 31:0) */
    };
    uint32_t val;
} lan865x_mac_samb1_reg_t;
#define LAN865X_MAC_SAMB1_REG_ADDR (0x32)

/* MAC_SAMT1 - MAC Specific Address Mask 1 Top */
typedef union {
    struct {
        uint32_t addr_39_32 : 8; /*!< Specific Address 1 Mask (bits 39:32) */
        uint32_t addr_47_40 : 8; /*!< Specific Address 1 Mask (bits 47:40) */
        uint32_t reserved : 16;  /*!< Reserved */
    };
    uint32_t val;
} lan865x_mac_samt1_reg_t;
#define LAN865X_MAC_SAMT1_REG_ADDR (0x33)

/* MAC_TISUBN - TSU Timer Increment Sub-nanoseconds Register */
typedef union {
    struct {
        uint32_t msbtir : 16;    /*!< Most Significant Bits of Timer Increment Register */
        uint32_t reserved : 8;   /*!< Reserved */
        uint32_t lsbtir : 8;     /*!< Lower Significant Bits of Timer Increment Register */
    };
    uint32_t val;
} lan865x_mac_tisubn_reg_t;
#define LAN865X_MAC_TISUBN_REG_ADDR (0x6F)

/* MAC_TSH - TSU Timer Seconds High Register */
typedef union {
    struct {
        uint32_t tcs_39_32 : 8;  /*!< Timer Count in Seconds (bits 39:32) */
        uint32_t tcs_47_40 : 8;  /*!< Timer Count in Seconds (bits 47:40) */
        uint32_t reserved : 16;  /*!< Reserved */
    };
    uint32_t val;
} lan865x_mac_tsh_reg_t;
#define LAN865X_MAC_TSH_REG_ADDR (0x70)

/* MAC_TSL - TSU Timer Seconds Low Register */
typedef union {
    struct {
        uint32_t tcs : 32;      /*!< Timer Count in Seconds (bits 31:0) */
    };
    uint32_t val;
} lan865x_mac_tsl_reg_t;
#define LAN865X_MAC_TSL_REG_ADDR (0x74)

/* MAC_TN - TSU Timer Nanoseconds Register */
typedef union {
    struct {
        uint32_t tns : 30;      /*!< Timer Count in Nanoseconds */
        uint32_t reserved : 2;  /*!< Reserved */
    };
    uint32_t val;
} lan865x_mac_tn_reg_t;
#define LAN865X_MAC_TN_REG_ADDR (0x75)

/* MAC_TA - TSU Timer Adjust Register */
typedef union {
    struct {
        uint32_t itdt : 30;     /*!< Increment/Decrement */
        uint32_t reserved : 1;  /*!< Reserved */
        uint32_t adj : 1;       /*!< TSU Timer Adjust */
    };
    uint32_t val;
} lan865x_mac_ta_reg_t;
#define LAN865X_MAC_TA_REG_ADDR (0x76)

/* MAC_TI - TSU Timer Increment Register */
typedef union {
    struct {
        uint32_t cns : 8;       /*!< Count Nanoseconds */
        uint32_t reserved : 24; /*!< Reserved */
    };
    uint32_t val;
} lan865x_mac_ti_reg_t;
#define LAN865X_MAC_TI_REG_ADDR (0x77)

/* BMGR_CTL - MAC Buffer Manager Control */
typedef union {
    struct {
        uint32_t reserved1 : 4; /*!< Reserved */
        uint32_t clrstats : 1;  /*!< Clear Statistics Counters */
        uint32_t snapstats : 1; /*!< Snapshot Statistics Counters */
        uint32_t reserved2 : 26; /*!< Reserved */
    };
    uint32_t val;
} lan865x_bmgr_ctl_reg_t;
#define LAN865X_BMGR_CTL_REG_ADDR (0x0200)

/******************************** Miscellaneous Register Descriptions Registers ********************************/

/* QTXCFG - Queue Transmit Configuration Register */
typedef union {
    struct {
        uint32_t reserved_0_18 : 19;    /*!< Reserved */
        uint32_t macfcsdis : 1;         /*!< MAC Frame Check Sequence Disable */
        uint32_t bufsz : 3;             /*!< Buffer Size */
        uint32_t reserved_23 : 1;       /*!< Reserved */
        uint32_t reserved_24_29 : 6;    /*!< Reserved */
        uint32_t ctthr : 2;             /*!< Cut-through threshold */
    };
    uint32_t val;
} lan865x_qtxcfg_reg_t;
#define LAN865X_QTXCFG_REG_ADDR (0x81)

/* QRXCFG - Queue Receive Configuration Register */
typedef union {
    struct {
        uint32_t reserved_0_19 : 20;    /*!< Reserved */
        uint32_t bufsz : 3;             /*!< Buffer Size */
        uint32_t reserved_23_31 : 9;    /*!< Reserved */
    };
    uint32_t val;
} lan865x_qrxcfg_reg_t;
#define LAN865X_QRXCFG_REG_ADDR (0x82)

/* PADCTRL - Pad Control Register */
typedef union {
    struct {
        uint32_t a0sel : 2;             /*!< DIOA0 Signal Select */
        uint32_t a1sel : 2;             /*!< DIOA1 Signal Select */
        uint32_t a2sel : 2;             /*!< DIOA2 Signal Select */
        uint32_t a3sel : 2;             /*!< DIOA3 Signal Select */
        uint32_t a4sel : 2;             /*!< DIOA4 Signal Select */
        uint32_t b0sel : 2;             /*!< DIOB0 Signal Select */
        uint32_t reserved_12_15 : 4;    /*!< Reserved */
        uint32_t acmasel : 2;           /*!< ACMA Input Select */
        uint32_t reserved_18_22 : 5;    /*!< Reserved */
        uint32_t refclksel : 1;         /*!< Reference Clock Select */
        uint32_t reserved_24_25 : 2;    /*!< Reserved */
        uint32_t pdrv1 : 2;             /*!< Digital Output Pad Drive Strength */
        uint32_t pdrv2 : 2;             /*!< Digital Output Pad Drive Strength */
        uint32_t pdrv3 : 2;             /*!< Digital Output Pad Drive Strength */
    };
    uint32_t val;
} lan865x_padctrl_reg_t;
#define LAN865X_PADCTRL_REG_ADDR (0x88)

/* CLKOCTL - Clock Output Control Register */
typedef union {
    struct {
        uint32_t clkosel : 2;           /*!< Clock Output Select */
        uint32_t reserved_2_3 : 2;      /*!< Reserved */
        uint32_t clkoen : 1;            /*!< Clock Output Enable */
        uint32_t reserved_5_31 : 27;    /*!< Reserved */
    };
    uint32_t val;
} lan865x_clkoctl_reg_t;
#define LAN865X_CLKOCTL_REG_ADDR (0x89)

/* MISC - Miscellaneous Register */
typedef union {
    struct {
        uint32_t uv18ftm : 12;          /*!< 1.8V supply Under-Voltage Filter Time */
        uint32_t uv18fen : 1;           /*!< 1.8V Supply Under-Voltage Filter Enable */
        uint32_t reserved_13_31 : 19;   /*!< Reserved */
    };
    uint32_t val;
} lan865x_misc_reg_t;
#define LAN865X_MISC_REG_ADDR (0x8C)

/* DEVID - Device Identification Register */
typedef union {
    struct {
        uint32_t rev : 4;               /*!< Revision Number */
        uint32_t model : 16;            /*!< Model Number */
        uint32_t reserved_20_31 : 12;   /*!< Reserved */
    };
    uint32_t val;
} lan865x_devid_reg_t;
#define LAN865X_DEVID_REG_ADDR (0x94)

/* BUSPCS - Bus Parity Control and Status Register */
typedef union {
    struct {
        uint32_t pargcen : 1;           /*!< Parity Generation and Check Enable */
        uint32_t bmgraperi : 1;         /*!< Buffer Manager Address Parity Error Injection */
        uint32_t mbmdperi : 1;          /*!< Buffer Manager Data Parity Error Injection */
        uint32_t sramdperi : 1;         /*!< SRAM Controller Parity Error Injection */
        uint32_t csrbdperg : 1;         /*!< Bridge Parity Error Injection */
        uint32_t reserved_5_15 : 11;    /*!< Reserved */
        uint32_t sramper : 1;           /*!< SRAM Controller Parity Error Status */
        uint32_t csrbper : 1;           /*!< Control/Status Register Bridge Parity Error Status */
        uint32_t spiper : 1;            /*!< SPI Parity Error Status */
        uint32_t mbmper : 1;            /*!< MAC Buffer Manager Parity Error Status */
        uint32_t reserved_20_31 : 12;   /*!< Reserved */
    };
    uint32_t val;
} lan865x_buspcs_reg_t;
#define LAN865X_BUSPCS_REG_ADDR (0x96)

/* CFGPRTCTL - Configuration Protection Control Register */
typedef union {
    struct {
        uint32_t wren : 1;              /*!< Configuration Write Enable */
        uint32_t reserved_1_29 : 29;    /*!< Reserved */
        uint32_t key1 : 1;              /*!< Key #1 Accepted */
        uint32_t key2 : 1;              /*!< Key #2 Accepted */
    };
    uint32_t val;
} lan865x_cfgprtctl_reg_t;
#define LAN865X_CFGPRTCTL_REG_ADDR (0x99)

/* ECCCTRL - SRAM Error Correction Code Control Register */
typedef union {
    struct {
        uint32_t bercnten : 1;          /*!< Bit Error Count Enable */
        uint32_t erclr : 1;             /*!< Error Clear */
        uint32_t eronesht : 1;          /*!< Error One Shot */
        uint32_t reserved_3_7 : 5;      /*!< Reserved */
        uint32_t sberlmt : 8;           /*!< Single Bit Error Limit */
        uint32_t encdis : 1;            /*!< ECC Encoder Disable */
        uint32_t decdis : 1;            /*!< ECC Decoder Disable */
        uint32_t reserved_18_31 : 14;   /*!< Reserved */
    };
    uint32_t val;
} lan865x_eccctrl_reg_t;
#define LAN865X_ECCCTRL_REG_ADDR (0x100)

/* ECCSTS - SRAM Error Correction Code Status Register */
typedef union {
    struct {
        uint32_t sbercnt : 8;           /*!< Single Bit Error Count */
        uint32_t dbercnt : 8;           /*!< Double Bit Error Count */
        uint32_t errsyn : 6;            /*!< Error Syndrome */
        uint32_t ersts : 2;             /*!< Error Status */
        uint32_t reserved_24_31 : 8;    /*!< Reserved */
    };
    uint32_t val;
} lan865x_eccsts_reg_t;
#define LAN865X_ECCSTS_REG_ADDR (0x101)

/* ECCFLTCTRL - SRAM Error Correction Code Fault Injection Control Register */
typedef union {
    struct {
        uint32_t fltinjbit1 : 6;        /*!< Fault Injection Bit 1 */
        uint32_t fltinjen : 1;          /*!< Fault Injection Enable */
        uint32_t fltinjaeq : 1;         /*!< Fault Injection Address Equal */
        uint32_t fltinjbit2 : 6;        /*!< Fault Injection Bit 2 */
        uint32_t fltinjonce : 1;        /*!< Fault Injection Once */
        uint32_t reserved_15 : 1;       /*!< Reserved */
        uint32_t fltinjadr : 13;        /*!< Fault Injection Address */
        uint32_t reserved_29_31 : 3;    /*!< Reserved */
    };
    uint32_t val;
} lan865x_eccfltctrl_reg_t;
#define LAN865X_ECCFLTCTRL_REG_ADDR (0x102)

/* Event Capture Control Register (EC0CTRL, EC1CTRL, EC2CTRL, EC3CTRL) */
typedef union {
    struct {
        uint32_t en : 1;                /*!< Event Capture Unit Enable */
        uint32_t da : 1;                /*!< Data Available */
        uint32_t reserved_2 : 1;        /*!< Reserved */
        uint32_t clr : 1;               /*!< Clear event capture unit */
        uint32_t src : 3;               /*!< Capture Signal Source */
        uint32_t edge : 2;              /*!< Capture Edge Selection */
        uint32_t reserved_9_10 : 2;     /*!< Reserved */
        uint32_t max : 4;               /*!< Maximum number of captured timestamps */
        uint32_t reserved_15_31 : 17;   /*!< Reserved */
    };
    uint32_t val;
} lan865x_ecctrl_reg_t;
#define LAN865X_EC0CTRL_REG_ADDR (0x200)
#define LAN865X_EC1CTRL_REG_ADDR (0x201)
#define LAN865X_EC2CTRL_REG_ADDR (0x202)
#define LAN865X_EC3CTRL_REG_ADDR (0x203)

/* ECRDSTS - Event Capture Read Status Register */
typedef union {
    struct {
        uint32_t ec0ct : 4;             /*!< Event capture unit 0 timestamp count */
        uint32_t ec1ct : 4;             /*!< Event capture unit 1 timestamp count */
        uint32_t ec2ct : 4;             /*!< Event capture unit 2 timestamp count */
        uint32_t ec3ct : 4;             /*!< Event capture unit 3 timestamp count */
        uint32_t reserved_16_23 : 8;    /*!< Reserved */
        uint32_t ec0ov : 1;             /*!< Event capture unit 0 overflow */
        uint32_t ec1ov : 1;             /*!< Event capture unit 1 overflow */
        uint32_t ec2ov : 1;             /*!< Event capture unit 2 overflow */
        uint32_t ec3ov : 1;             /*!< Event capture unit 3 overflow */
        uint32_t reserved_28_31 : 4;    /*!< Reserved */
    };
    uint32_t val;
} lan865x_ecrdsts_reg_t;
#define LAN865X_ECRDSTS_REG_ADDR (0x204)

/* ECTOT - Event Capture Total Counts Register */
typedef union {
    struct {
        uint32_t ec0tot : 8;            /*!< Event Capture 0 Total */
        uint32_t ec1tot : 8;            /*!< Event Capture 1 Total */
        uint32_t ec2tot : 8;            /*!< Event Capture 2 Total */
        uint32_t ec3tot : 8;            /*!< Event Capture 3 Total */
    };
    uint32_t val;
} lan865x_ectot_reg_t;
#define LAN865X_ECTOT_REG_ADDR (0x205)

/* ECCLKSH - Event Capture Clock Seconds High Register */
typedef union {
    struct {
        uint32_t clksec_39_32 : 8;      /*!< Clock time in seconds [39:32] */
        uint32_t clksec_47_40 : 8;      /*!< Clock time in seconds [47:40] */
        uint32_t reserved_16_31 : 16;   /*!< Reserved */
    };
    uint32_t val;
} lan865x_ecclksh_reg_t;
#define LAN865X_ECCLKSH_REG_ADDR (0x206)

/* ECCLKSL - Event Capture Clock Seconds Low Register */
typedef union {
    struct {
        uint32_t clksec_31_0;           /*!< Clock time in seconds [31:0] */
    };
    uint32_t val;
} lan865x_ecclksl_reg_t;
#define LAN865X_ECCLKSL_REG_ADDR (0x207)

/* ECCLKNS - Event Capture Clock Nanoseconds Register */
typedef union {
    struct {
        uint32_t clock_ns : 30;         /*!< Clock time in nanoseconds */
        uint32_t reserved_30_31 : 2;    /*!< Reserved */
    };
    uint32_t val;
} lan865x_ecclkns_reg_t;
#define LAN865X_ECCLKNS_REG_ADDR (0x208)

/* ECRDTSn - Event Capture Read Timestamp Register n */
typedef union {
    struct {
        uint32_t tsns : 30;             /*!< Timestamp Nanoseconds */
        uint32_t tssec : 2;             /*!< Timestamp Seconds */
    };
    uint32_t val;
} lan865x_ecrdts_reg_t;
#define LAN865X_ECRDTS0_REG_ADDR (0x209)
#define LAN865X_ECRDTS1_REG_ADDR (0x20A)
#define LAN865X_ECRDTS2_REG_ADDR (0x20B)
#define LAN865X_ECRDTS3_REG_ADDR (0x20C)
#define LAN865X_ECRDTS4_REG_ADDR (0x20D)
#define LAN865X_ECRDTS5_REG_ADDR (0x20E)
#define LAN865X_ECRDTS6_REG_ADDR (0x20F)
#define LAN865X_ECRDTS7_REG_ADDR (0x210)
#define LAN865X_ECRDTS8_REG_ADDR (0x211)
#define LAN865X_ECRDTS9_REG_ADDR (0x212)
#define LAN865X_ECRDTS10_REG_ADDR (0x213)
#define LAN865X_ECRDTS11_REG_ADDR (0x214)
#define LAN865X_ECRDTS12_REG_ADDR (0x215)
#define LAN865X_ECRDTS13_REG_ADDR (0x216)
#define LAN865X_ECRDTS14_REG_ADDR (0x217)
#define LAN865X_ECRDTS15_REG_ADDR (0x218)

/* PACYC - Phase Adjuster Cycles Register */
typedef union {
    struct {
        uint32_t cyc : 30;              /*!< Number of clock cycles between phase adjustments */
        uint32_t reserved_30_31 : 2;    /*!< Reserved */
    };
    uint32_t val;
} lan865x_pacyc_reg_t;
#define LAN865X_PACYC_REG_ADDR (0x21F)

/* PACTRL - Phase Adjuster Control Register */
typedef union {
    struct {
        uint32_t dif : 10;              /*!< Time difference */
        uint32_t reserved_10_29 : 20;   /*!< Reserved */
        uint32_t dec : 1;               /*!< Decrement */
        uint32_t sec : 1;               /*!< Units are seconds */
        uint32_t act : 1;               /*!< Phase adjust active */
    };
    uint32_t val;
} lan865x_pactrl_reg_t;
#define LAN865X_PACTRL_REG_ADDR (0x220)

/* Event Generator Start Time Nanoseconds Register (EG0STNS, EG1STNS, EG2STNS, EG3STNS) */
typedef union {
    struct {
        uint32_t stns : 30;             /*!< Pulse Start Time in Nanoseconds */
        uint32_t reserved_30_31 : 2;    /*!< Reserved */
    };
    uint32_t val;
} lan865x_egstns_reg_t;
#define LAN865X_EG0STNS_REG_ADDR (0x221)
#define LAN865X_EG1STNS_REG_ADDR (0x227)
#define LAN865X_EG2STNS_REG_ADDR (0x22D)
#define LAN865X_EG3STNS_REG_ADDR (0x233)

/* Event Generator Start Time Seconds Low Register (EG0STSECL, EG1STSECL, EG2STSECL, EG3STSECL) */
typedef union {
    struct {
        uint32_t stsec_31_0;            /*!< Start Time in Seconds [31:0] */
    };
    uint32_t val;
} lan865x_egstsecl_reg_t;
#define LAN865X_EG0STSECL_REG_ADDR (0x222)
#define LAN865X_EG1STSECL_REG_ADDR (0x228)
#define LAN865X_EG2STSECL_REG_ADDR (0x22E)
#define LAN865X_EG3STSECL_REG_ADDR (0x234)

/* Event Generator Start Time Seconds High Register (EG0STSECH, EG1STSECH, EG2STSECH, EG3STSECH) */
typedef union {
    struct {
        uint32_t stsec_39_32 : 8;       /*!< Start Time in Seconds [39:32] */
        uint32_t stsec_47_40 : 8;       /*!< Start Time in Seconds [47:40] */
        uint32_t reserved_16_31 : 16;   /*!< Reserved */
    };
    uint32_t val;
} lan865x_egstsech_reg_t;
#define LAN865X_EG0STSECH_REG_ADDR (0x223)
#define LAN865X_EG1STSECH_REG_ADDR (0x229)
#define LAN865X_EG2STSECH_REG_ADDR (0x22F)
#define LAN865X_EG3STSECH_REG_ADDR (0x235)

/* Event Generator Pulse Width Register (EG0PW, EG1PW, EG2PW, EG3PW) */
typedef union {
    struct {
        uint32_t pw : 30;               /*!< Pulse Width in Nanoseconds */
        uint32_t reserved_30_31 : 2;    /*!< Reserved */
    };
    uint32_t val;
} lan865x_egpw_reg_t;
#define LAN865X_EG0PW_REG_ADDR (0x224)
#define LAN865X_EG1PW_REG_ADDR (0x22A)
#define LAN865X_EG2PW_REG_ADDR (0x230)
#define LAN865X_EG3PW_REG_ADDR (0x236)

/* Event Generator Idle Time Register (EG0IT, EG1IT, EG2IT, EG3IT) */
typedef union {
    struct {
        uint32_t it : 30;               /*!< Idle Time in Nanoseconds */
        uint32_t reserved_30_31 : 2;    /*!< Reserved */
    };
    uint32_t val;
} lan865x_egit_reg_t;
#define LAN865X_EG0IT_REG_ADDR (0x225)
#define LAN865X_EG1IT_REG_ADDR (0x22B)
#define LAN865X_EG2IT_REG_ADDR (0x231)
#define LAN865X_EG3IT_REG_ADDR (0x237)

/* Event Generator Control Register (EG0CTL, EG1CTL, EG2CTL, EG3CTL) */
typedef union {
    struct {
        uint32_t start : 1;             /*!< Start Event */
        uint32_t stop : 1;              /*!< Stop Event */
        uint32_t ah : 1;                /*!< Event Output is Active High */
        uint32_t rep : 1;               /*!< Event Repeats Periodically */
        uint32_t isrel : 1;             /*!< Event Uses Relative Timing */
        uint32_t reserved_5_31 : 27;    /*!< Reserved */
    };
    uint32_t val;
} lan865x_egctl_reg_t;
#define LAN865X_EG0CTL_REG_ADDR (0x226)
#define LAN865X_EG1CTL_REG_ADDR (0x22C)
#define LAN865X_EG2CTL_REG_ADDR (0x232)
#define LAN865X_EG3CTL_REG_ADDR (0x238)

/* PPSCTL - One Pulse-per-Second Control Register */
typedef union {
    struct {
        uint32_t ppsen : 1;             /*!< 1 pulse-per-second signal enable */
        uint32_t ppsdis : 1;            /*!< 1 pulse-per-second signal disable */
        uint32_t ppspw : 5;             /*!< Pulse-per-second pulse width */
        uint32_t reserved_7_31 : 25;    /*!< Reserved */
    };
    uint32_t val;
} lan865x_ppsctl_reg_t;
#define LAN865X_PPSCTL_REG_ADDR (0x239)

/* SEVINTEN - Synchronization Event Interrupt Enable Register */
typedef union {
    struct {
        uint32_t ec0of : 1;             /*!< Event capture unit 0 overflow */
        uint32_t ec0da : 1;             /*!< Event capture unit 0 data available */
        uint32_t ec1of : 1;             /*!< Event capture unit 1 overflow */
        uint32_t ec1da : 1;             /*!< Event capture unit 1 data available */
        uint32_t ec2of : 1;             /*!< Event capture unit 2 overflow */
        uint32_t ec2da : 1;             /*!< Event capture unit 2 data available */
        uint32_t ec3of : 1;             /*!< Event capture unit 3 overflow */
        uint32_t ec3da : 1;             /*!< Event capture unit 3 data available */
        uint32_t reserved_8_15 : 8;     /*!< Reserved */
        uint32_t eg0done : 1;           /*!< Event generator 0 done */
        uint32_t eg1done : 1;           /*!< Event generator 1 done */
        uint32_t eg2done : 1;           /*!< Event generator 2 done */
        uint32_t eg3done : 1;           /*!< Event generator 3 done */
        uint32_t reserved_20_29 : 10;   /*!< Reserved */
        uint32_t ppsdone : 1;           /*!< One pulse-per-second signal generation done */
        uint32_t padone : 1;            /*!< Phase Adjust Done */
    };
    uint32_t val;
} lan865x_sevinten_reg_t;
#define LAN865X_SEVINTEN_REG_ADDR (0x23A)

/* SEVINTDIS - Synchronization Event Interrupt Disable Register */
typedef union {
    struct {
        uint32_t ec0of : 1;             /*!< Event capture unit 0 overflow */
        uint32_t ec0da : 1;             /*!< Event capture unit 0 data available */
        uint32_t ec1of : 1;             /*!< Event capture unit 1 overflow */
        uint32_t ec1da : 1;             /*!< Event capture unit 1 data available */
        uint32_t ec2of : 1;             /*!< Event capture unit 2 overflow */
        uint32_t ec2da : 1;             /*!< Event capture unit 2 data available */
        uint32_t ec3of : 1;             /*!< Event capture unit 3 overflow */
        uint32_t ec3da : 1;             /*!< Event capture unit 3 data available */
        uint32_t reserved_8_15 : 8;     /*!< Reserved */
        uint32_t eg0done : 1;           /*!< Event generator 0 done */
        uint32_t eg1done : 1;           /*!< Event generator 1 done */
        uint32_t eg2done : 1;           /*!< Event generator 2 done */
        uint32_t eg3done : 1;           /*!< Event generator 3 done */
        uint32_t reserved_20_29 : 10;   /*!< Reserved */
        uint32_t ppsdone : 1;           /*!< One pulse-per-second signal generation done */
        uint32_t padone : 1;            /*!< Phase Adjust Done */
    };
    uint32_t val;
} lan865x_sevintdis_reg_t;
#define LAN865X_SEVINTDIS_REG_ADDR (0x23B)

/* SEVIM - Synchronization Event Interrupt Mask Status Register */
typedef union {
    struct {
        uint32_t ec0of : 1;             /*!< Event capture unit 0 overflow */
        uint32_t ec0da : 1;             /*!< Event capture unit 0 data available */
        uint32_t ec1of : 1;             /*!< Event capture unit 1 overflow */
        uint32_t ec1da : 1;             /*!< Event capture unit 1 data available */
        uint32_t ec2of : 1;             /*!< Event capture unit 2 overflow */
        uint32_t ec2da : 1;             /*!< Event capture unit 2 data available */
        uint32_t ec3of : 1;             /*!< Event capture unit 3 overflow */
        uint32_t ec3da : 1;             /*!< Event capture unit 3 data available */
        uint32_t reserved_8_15 : 8;     /*!< Reserved */
        uint32_t eg0done : 1;           /*!< Event generator 0 done */
        uint32_t eg1done : 1;           /*!< Event generator 1 done */
        uint32_t eg2done : 1;           /*!< Event generator 2 done */
        uint32_t eg3done : 1;           /*!< Event generator 3 done */
        uint32_t reserved_20_28 : 9;    /*!< Reserved */
        uint32_t paer : 1;              /*!< Phase adjust error */
        uint32_t ppsdone : 1;           /*!< One pulse-per-second signal generation done */
        uint32_t padone : 1;            /*!< Phase Adjust Done */
    };
    uint32_t val;
} lan865x_sevim_reg_t;
#define LAN865X_SEVIM_REG_ADDR (0x23C)

/* SEVSTS - Synchronization Event Status Register */
typedef union {
    struct {
        uint32_t ec0of : 1;             /*!< Event capture unit 0 overflow */
        uint32_t ec0da : 1;             /*!< Event capture unit 0 data available */
        uint32_t ec1of : 1;             /*!< Event capture unit 1 overflow */
        uint32_t ec1da : 1;             /*!< Event capture unit 1 data available */
        uint32_t ec2of : 1;             /*!< Event capture unit 2 overflow */
        uint32_t ec2da : 1;             /*!< Event capture unit 2 data available */
        uint32_t ec3of : 1;             /*!< Event capture unit 3 overflow */
        uint32_t ec3da : 1;             /*!< Event capture unit 3 data available */
        uint32_t reserved_8_15 : 8;     /*!< Reserved */
        uint32_t eg0done : 1;           /*!< Event generator 0 done */
        uint32_t eg1done : 1;           /*!< Event generator 1 done */
        uint32_t eg2done : 1;           /*!< Event generator 2 done */
        uint32_t eg3done : 1;           /*!< Event generator 3 done */
        uint32_t reserved_20_28 : 9;    /*!< Reserved */
        uint32_t paer : 1;              /*!< Phase adjust error */
        uint32_t ppsdone : 1;           /*!< One pulse-per-second signal generation done */
        uint32_t padone : 1;            /*!< Phase Adjust Done */
    };
    uint32_t val;
} lan865x_sevsts_reg_t;
#define LAN865X_SEVSTS_REG_ADDR (0x23D)
