/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileContributor: 2021 Benoît Roehr
 */
#pragma once

#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

/*---------- KSZ8863 DEFINITIONS -----------*/
#define KSZ8863_PORT1_ADDR_OFFSET (0x0)
#define KSZ8863_PORT2_ADDR_OFFSET (0x10)
#define KSZ8863_PORT3_ADDR_OFFSET (0x20)

#define KSZ8863_TO_PORT1 (1 << 0)
#define KSZ8863_TO_PORT2 (1 << 1)
#define KSZ8863_TO_PORT3 (1 << 2)

/**
 * @brief Register 0 (0x00): Chip ID0
 *
 */
typedef union {
    struct {
        uint32_t family_id : 8;    /*!< Chip family */
    };
    uint32_t val;
} ksz8863_chipid0_reg_t;
#define KSZ8863_CHIPID0_REG_ADDR (0x00)

/**
 * @brief Register 1 (0x01): Chip ID1/Start Switch
 *
 */
typedef union {
    struct {
        uint32_t start_switch : 1; /*!< Start the switch*/
        uint32_t revision_id : 3;  /*!< Revision ID */
        uint32_t chip_id : 4;      /*!< Chip ID (0x3 is assigned to M series) */
    };
    uint32_t val;
} ksz8863_chipid1_reg_t;
#define KSZ8863_CHIPID1_REG_ADDR (0x01)

/**
 * @brief Register 2 (0x02): Global Control 0
 *
 */
typedef union {
    struct {
        uint32_t reserved_0_3 : 3;         /*!< Reserved */
        uint32_t pass_flow_ctrl : 1;       /*!< Pass Flow Control Packets*/
        uint32_t flush_sta_mac_tbl : 1;    /*!< Flush static MAC table for spanning tree application*/
        uint32_t flush_dyn_mac_tbl : 1;    /*!< Flush dynamic MAC table for spanning tree application*/
        uint32_t reserved_6 : 1;           /*!< Reserved */
        uint32_t new_backoff_en : 1;       /*!< Enable New back-off algorithm designed for UNH */
    };
    uint32_t val;
} ksz8863_gcr0_reg_t;
#define KSZ8863_GCR0_ADDR (0x02)

/**
 * @brief Register 3 (0x03): Global Control 1
 *
 */
typedef union {
    struct {
        uint32_t aggr_backoff_en : 1;    /*!< Aggressive Back-Off Enable */
        uint32_t fast_age_en : 1;        /*!< Fast Age Enable */
        uint32_t aging_en : 1;           /*!< Aging Enable */
        uint32_t check_len : 1;          /*!< Frame Length Field Check */
        uint32_t rx_flow_ctrl_en : 1;    /*!< IEEE 802.3x Receive Direction Flow Control Enable*/
        uint32_t tx_flow_ctrl_en : 1;    /*!< IEEE 802.3x Transmit Direction Flow Control Enable*/
        uint32_t tail_tag_en : 1;        /*!< Port 3 Tail Tag Mode Enable */
        uint32_t pass_all : 1;           /*!< Pass All Frames */
    };
    uint32_t val;
} ksz8863_gcr1_reg_t;
#define KSZ8863_GCR1_ADDR (0x03)

/**
 * @brief Register 4 (0x04): Global Control 2
 *
 */
typedef union {
    struct {
        uint32_t reserved_0 : 1;                /*!< Reserved */
        uint32_t legal_len_check_en : 1;        /*!< Legal Maximum Packet Size Check Enable */
        uint32_t huge_packet_support : 1;       /*!< Huge Packet Support */
        uint32_t no_exc_collision_drop : 1;     /*!< No Excessive Collision Drop */
        uint32_t fair_flow_ctrl_mode : 1;       /*!< Flow Control and Back Pressure Fair Mode*/
        uint32_t back_pressure_mode : 1;        /*!< Back Pressure Mode */
        uint32_t multicast_storm_dis : 1;       /*!< Multicast Storm Protection Disable */
        uint32_t unicast_vlan_boundary : 1;     /*!< Unicast Port-VLAN Mismatch Discard */
    };
    uint32_t val;
} ksz8863_gcr2_reg_t;
#define KSZ8863_GCR2_ADDR (0x04)

/**
 * @brief Register 5 (0x05): Global Control 3
 *
 */
typedef union {
    struct {
        uint32_t sniff_mode_sel : 1;         /*!< Sniff Mode Select */
        uint32_t reserved_1_2 : 2;           /*!< Reserved */
        uint32_t weigh_fair_queue_en : 1;    /*!< Weighted Fair Queue Enable*/
        uint32_t reserved_4_5 : 2;           /*!< Reserved */
        uint32_t igmp_snoop_en : 1;          /*!< IGMP Snoop Enable on Switch MII Interface*/
        uint32_t vlan_en : 1;                /*!< 802.1Q VLAN Enable*/
    };
    uint32_t val;
} ksz8863_gcr3_reg_t;
#define KSZ8863_GCR3_ADDR (0x05)

/**
 * @brief Register 6 (0x06): Global Control 4
 *
 */
typedef union {
    struct {
        uint32_t brdcast_storm_rate_8_10 : 3;  /*!< Broadcast Storm Protection Rate Bit [10:8] */
        uint32_t replace_null_vid : 1;         /*!< Null VID Replacement*/
        uint32_t switch_10base_t : 1;          /*!< Switch MII 10BT */
        uint32_t switch_flow_ctrl_en : 1;      /*!< Switch MII Flow Control Enable*/
        uint32_t switch_half_duplex : 1;       /*!< Switch MII HalfDuplex Mode */
        uint32_t reserved_7 : 1;               /*!< Reserved */
    };
    uint32_t val;
} ksz8863_gcr4_reg_t;
#define KSZ8863_GCR4_ADDR (0x06)

/**
 * @brief Register 7 (0x07): Global Control 5
 *
 */
typedef union {
    struct {
        uint32_t brdcast_storm_rate_0_7 : 8;  /*!< Broadcast Storm Protection Rate Bit [7:0] */
    };
    uint32_t val;
} ksz8863_gcr5_reg_t;
#define KSZ8863_GCR5_ADDR (0x07)

/**
 * @brief Register 8 (0x08): Global Control 6
 *
 */
typedef union {
    struct {
        uint32_t factory_test : 8; /*!< Factory Testing (0x00) */
    };
    uint32_t val;
} ksz8863_gcr6_reg_t;
#define KSZ8863_GCR6_ADDR (0x08)

/**
 * @brief Register 9 (0x09): Global Control 7
 *
 */
typedef union {
    struct {
        uint32_t factory_test : 8; /*!< Factory Testing (0x24) */
    };
    uint32_t val;
} ksz8863_gcr7_reg_t;
#define KSZ8863_GCR7_ADDR (0x09)

/**
 * @brief Register 10 (0x0A): Global Control 8
 *
 */
typedef union {
    struct {
        uint32_t factory_test : 8; /*!< Factory Testing (0x35) */
    };
    uint32_t val;
} ksz8863_gcr8_reg_t;
#define KSZ8863_GCR8_ADDR (0x0A)

/**
 * @brief Register 11 (0x0B): Global Control 9
 *
 */
typedef union {
    struct {
        uint32_t reserved_0_5 : 6;    /*!< Reserved */
        uint32_t cpu_if_clk_sel : 2;  /*!< CPU interface Clock Selection*/
    };
    uint32_t val;
} ksz8863_gcr9_reg_t;
#define KSZ8863_GCR9_ADDR (0x0B)

#define KSZ8863_SPI_CLK_125_MHZ   (0x02)
#define KSZ8863_SPI_CLK_62_5_MHZ  (0x01)
#define KSZ8863_SPI_CLK_31_25_MHZ (0x00)

/**
 * @brief Register 12 (0x0C): Global Control 10
 *
 */
typedef union {
    struct {
        uint32_t tag_0x0 : 2; /*!< Frame priority when IEEE 802.1p mapping Tag has 0x0 */
        uint32_t tag_0x1 : 2; /*!< Frame priority when IEEE 802.1p mapping Tag has 0x1 */
        uint32_t tag_0x2 : 2; /*!< Frame priority when IEEE 802.1p mapping Tag has 0x2 */
        uint32_t tag_0x3 : 2; /*!< Frame priority when IEEE 802.1p mapping Tag has 0x3 */
    };
    uint32_t val;
} ksz8863_gcr10_reg_t;
#define KSZ8863_GCR10_ADDR (0x0C)

/**
 * @brief Register 13 (0x0D): Global Control 11
 *
 */
typedef union {
    struct {
        uint32_t tag_0x4 : 2; /*!< Frame priority when IEEE 802.1p mapping Tag has 0x4 */
        uint32_t tag_0x5 : 2; /*!< Frame priority when IEEE 802.1p mapping Tag has 0x5 */
        uint32_t tag_0x6 : 2; /*!< Frame priority when IEEE 802.1p mapping Tag has 0x6 */
        uint32_t tag_0x7 : 2; /*!< Frame priority when IEEE 802.1p mapping Tag has 0x7 */
    };
    uint32_t val;
} ksz8863_gcr11_reg_t;
#define KSZ8863_GCR11_ADDR (0x0D)

/**
 * @brief Register 14 (0x0E): Global Control 12
 *
 */
typedef union {
    struct {
        uint32_t unknown_da_to_port : 3;     /*!< Unknown Packet Default Port */
        uint32_t reserved_3_5 : 3;           /*!< Reserved */
        uint32_t drive_strength : 1;         /*!< Drive Strength of I/O Pad */
        uint32_t unknown_da_to_port_en : 1;  /*!< Unknown Packet Default Port Enable */
    };
    uint32_t val;
} ksz8863_gcr12_reg_t;
#define KSZ8863_GCR12_ADDR (0x0E)

/**
 * @brief Register 15 (0x0F): Global Control 13
 *
 */
typedef union {
    struct {
        uint32_t reserved_0_2 : 3;    /*!< Reserved */
        uint32_t phy_addr : 5;        /*!< PHY Address */
    };
    uint32_t val;
} ksz8863_gcr13_reg_t;
#define KSZ8863_GCR13_ADDR (0x0F)

/**
 * @brief Register 16/32/48: Port x Control 0
 *
 */
typedef union {
    struct {
        uint32_t txq_split_en : 1;           /*!< TXQ Split Enable */
        uint32_t remove_tag : 1;             /*!< Tag Removal */
        uint32_t insert_tag : 1;             /*!< Tag Insertion */
        uint32_t port_based_priority : 2;    /*!< Port-based Priority Classification */
        uint32_t priority_802_1p_en : 1;     /*!< 802.1p Priority Classification Enable */
        uint32_t priority_diffserv_en : 1;   /*!< DiffServ Priority Classification Enable */
        uint32_t broadcast_storm_en : 1;     /*!< Broadcast Storm Protection Enable*/
    };
    uint32_t val;
} ksz8863_pcr0_reg_t;
#define KSZ8863_PCR0_BASE_ADDR (0x10)
#define KSZ8863_P1CR0_ADDR (0x10)
#define KSZ8863_P2CR0_ADDR (0x20)
#define KSZ8863_P3CR0_ADDR (0x30)

/**
 * @brief Register 17/33/49: Port x Control 1
 *
 */
typedef union {
    struct {
        uint32_t port_vlan_membership : 3;   /*!< Port VLAN Membership */
        uint32_t user_priority_ceiling : 1;  /*!< User Priority Ceiling */
        uint32_t double_tag : 1;             /*!< Double Tag */
        uint32_t tx_sniff : 1;               /*!< Transmit Sniff */
        uint32_t rx_sniff : 1;               /*!< Receive Sniff */
        uint32_t sniff_port : 1;             /*!< Sniffer Port */
    };
    uint32_t val;
} ksz8863_pcr1_reg_t;
#define KSZ8863_PCR1_BASE_ADDR (0x11)
#define KSZ8863_P1CR1_ADDR (0x11)
#define KSZ8863_P2CR1_ADDR (0x21)
#define KSZ8863_P3CR1_ADDR (0x21)

/**
 * @brief Register 18/34/50: Port x Control 2
 *
 */
typedef union {
    struct {
        uint32_t learn_dis : 1;             /*!< Learning Disable */
        uint32_t rx_en : 1;                 /*!< Receive Enable */
        uint32_t tx_en : 1;                 /*!< Transmit Enable */
        uint32_t back_pressure_en : 1;      /*!< Back Pressure Enable */
        uint32_t force_flow_ctrl : 1;       /*!< Force Flow Control */
        uint32_t discard_non_pvid : 1;      /*!< Discard non-PVID Packets */
        uint32_t ingres_VLAN_filter : 1;    /*!< Ingress VLAN Filtering */
        uint32_t tx_2_queues_split_en : 1;  /*!< Enable 2 Queue Split of Tx Queue */
    };
    uint32_t val;
} ksz8863_pcr2_reg_t;
#define KSZ8863_PCR2_BASE_ADDR (0x12)
#define KSZ8863_P1CR2_ADDR (0x12)
#define KSZ8863_P2CR2_ADDR (0x22)
#define KSZ8863_P3CR2_ADDR (0x32)

/**
 * @brief Register 19/35/51: Port x Control 3
 *
 */
typedef union {
    struct {
        uint32_t default_tag_15_8 : 8; /*!< Port Default Tag [15:8] */
    };
    uint32_t val;
} ksz8863_pcr3_reg_t;
#define KSZ8863_PCR3_BASE_ADDR (0x13)
#define KSZ8863_P1CR3_ADDR (0x13)
#define KSZ8863_P2CR3_ADDR (0x23)
#define KSZ8863_P3CR3_ADDR (0x33)

/**
 * @brief Register 20/36/52: Port x Control 4
 *
 */
typedef union {
    struct {
        uint32_t default_tag_7_0 : 8; /*!< Port Default Tag [7:0] */
    };
    uint32_t val;
} ksz8863_pcr4_reg_t;
#define KSZ8863_PCR4_BASE_ADDR (0x14)
#define KSZ8863_P1CR4_ADDR (0x14)
#define KSZ8863_P2CR4_ADDR (0x24)
#define KSZ8863_P3CR4_ADDR (0x34)

/**
 * @brief Register 21/37/53: Port x Control 5
 *
 */
typedef union {
    struct {
        uint32_t count_pre : 1;            /*!< Count Preamble bytes*/
        uint32_t count_ifg : 1;            /*!< Count IFG bytes */
        uint32_t limit_mode : 2;           /*!< Ingress Limit Mode */
        uint32_t drop_tagged_frame : 1;    /*!< Drop Ingress Tagged Frame */
        uint32_t filter_maca2_en : 1;      /*!< Self-Address Filtering Enable MACA2 (not for P3) */
        uint32_t filter_maca1_en : 1;      /*!< Self-Address Filtering Enable MACA1 (not for P3) */
        uint32_t port3_mii_mode : 1;       /*!< Port 3 MII Mode Selection (should be set to 1 for P1/not applicable for RMII) */
    };
    uint32_t val;
} ksz8863_pcr5_reg_t;
#define KSZ8863_PCR5_BASE_ADDR (0x15)
#define KSZ8863_P1CR5_ADDR (0x15)
#define KSZ8863_P2CR5_ADDR (0x25)
#define KSZ8863_P3CR5_ADDR (0x35)

/**
 * @brief Register 22/38/54 [6:0]: Port x Q0 Ingress Data Rate Limit
 *
 */
typedef union {
    struct {
        uint32_t q0_in_rate_limit : 7;      /*!< Q0 Ingress Data Rate Limit */
        uint32_t rmii_reflck_invert : 1;    /*!< RMII REFCLK Invert (Port 3 only) */
    };
    uint32_t val;
} ksz8863_idrlq0_reg_t;
#define KSZ8863_IDRLQ0_BASE_ADDR (0x16)
#define KSZ8863_P1IDRLQ0_ADDR (0x16)
#define KSZ8863_P2IDRLQ0_ADDR (0x26)
#define KSZ8863_P3IDRLQ0_ADDR (0x36)

/**
 * @brief Register 23/39/55 [6:0]: Port x Q1 Ingress Data Rate Limit
 *
 */
typedef union {
    struct {
        uint32_t q1_in_rate_limit : 7;    /*!< Q1 Ingress Data Rate Limit */
        uint32_t reserved_7 : 1;          /*!< Reserved */
    };
    uint32_t val;
} ksz8863_idrlq1_reg_t;
#define KSZ8863_IDRLQ1_BASE_ADDR (0x17)
#define KSZ8863_P1IDRLQ1_ADDR (0x17)
#define KSZ8863_P2IDRLQ1_ADDR (0x27)
#define KSZ8863_P3IDRLQ1_ADDR (0x37)

/**
 * @brief Register 24/40/56 [6:0]: Port x Q2 Ingress Data Rate Limit
 *
 */
typedef union {
    struct {
        uint32_t q2_in_rate_limit : 7;    /*!< Q2 Ingress Data Rate Limit */
        uint32_t reserved_7 : 1;          /*!< Reserved */
    };
    uint32_t val;
} ksz8863_idrlq2_reg_t;
#define KSZ8863_IDRLQ2_BASE_ADDR (0x18)
#define KSZ8863_P1IDRLQ2_ADDR (0x18)
#define KSZ8863_P2IDRLQ2_ADDR (0x28)
#define KSZ8863_P3IDRLQ2_ADDR (0x38)

/**
 * @brief Register 25/41/57 [6:0]: Port x Q3 Ingress Data Rate Limit
 *
 */
typedef union {
    struct {
        uint32_t q3_in_rate_limit : 7;    /*!< Q3 Ingress Data Rate Limit */
        uint32_t reserved_7 : 1;          /*!< Reserved */
    };
    uint32_t val;
} ksz8863_idrlq3_reg_t;
#define KSZ8863_IDRLQ3_BASE_ADDR (0x19)
#define KSZ8863_P1IDRLQ3_ADDR (0x19)
#define KSZ8863_P2IDRLQ3_ADDR (0x29)
#define KSZ8863_P3IDRLQ3_ADDR (0x39)

/**
 * @brief Register 26/42: Port 1/2 PHY Special Control/Status
 *
 */
typedef union {
    struct {
        uint32_t vct_fault_count_8 : 1;   /*!< Vct_fault_count[8] */
        uint32_t remote_loopback : 1;     /*!< Remote Loopback */
        uint32_t reserved_2 : 1;          /*!< Reserved */
        uint32_t force_link : 1;          /*!< Force link */
        uint32_t vct_en : 1;              /*!< Enable cable diagnostic test */
        uint32_t vct_result : 2;          /*!< Vct_result */
        uint32_t vct_10M_short : 1;       /*!< Vct 10M Short */
    };
    uint16_t val;
} ksz8863_scsr_reg_t;
#define KSZ8863_SCSR_BASE_ADDR (0x1A)
#define KSZ8863_P1SCSR_ADDR (0x1A)
#define KSZ8863_P2SCSR_ADDR (0x2A)

/**
 * @brief Register 27/43: Port 1/2 LinkMD Result
 *
 */
typedef union {
    struct {
        uint32_t vct_fault_count_0_7 : 8;    /*!< Vct_fault_count[7:0] */
    };
    uint16_t val;
} ksz8863_lmdrr_reg_t;
#define KSZ8863_LMDRR_BASE_ADDR (0x1B)
#define KSZ8863_P1LMDRR_ADDR (0x1B)
#define KSZ8863_P2LMDRR_ADDR (0x2B)

/**
 * @brief Register 28/44: Port 1/2 Control 12
 *
 */
typedef union {
    struct {
        uint32_t advertise_10bt_halfdupx : 1;   /*!< Advertise 10BT HalfDuplex Capability */
        uint32_t advertise_10bt_fulldupx : 1;   /*!< Advertise 10BT FullDuplex Capability */
        uint32_t advertise_100bt_halfdupx : 1;  /*!< Advertise 100BT Half-Duplex Capability */
        uint32_t advertise_100bt_fulldupx : 1;  /*!< Advertise 100BT Full-Duplex Capability */
        uint32_t advertise_flow_ctrl : 1;       /*!< Advertise Flow Control Capability */
        uint32_t force_full_duplex : 1;         /*!< Force Full Duplex */
        uint32_t force_100bt : 1;               /*!< Force 100BT*/
        uint32_t en_auto_nego : 1;              /*!< Auto Negotiation Enable */
    };
    uint32_t val;
} ksz8863_pcr12_reg_t;
#define KSZ8863_PCR12_BASE_ADDR (0x1C)
#define KSZ8863_P1CR12_ADDR (0x1C)
#define KSZ8863_P2CR12_ADDR (0x2C)

/**
 * @brief Register 29/45: Port 1/2 Control 13
 *
 */
typedef union {
    struct {
        uint32_t loopback : 1;             /*!< Loopback */
        uint32_t force_mdi : 1;            /*!< Force MDI */
        uint32_t auto_mdi_mdi_x_dis : 1;   /*!< Disable Auto MDI/MDI-X */
        uint32_t power_down : 1;           /*!< Power Down */
        uint32_t far_end_fault_dis : 1;    /*!< Disable Far-End Fault */
        uint32_t restart_auto_nego : 1;    /*!< Restart auto negotiation */
        uint32_t tx_dis : 1;               /*!< Disable the port’s transmitter */
        uint32_t led_off : 1;              /*!< LED Off */
    };
    uint32_t val;
} ksz8863_pcr13_reg_t;
#define KSZ8863_PCR13_BASE_ADDR (0x1D)
#define KSZ8863_P1CR13_ADDR (0x1D)
#define KSZ8863_P2CR13_ADDR (0x2D)

/**
 * @brief Register 30/46: Port 1/2 Status 0
 *
 */
typedef union {
    struct {
        uint32_t partner_10bt_halfdupx : 1;   /*!< Partner 10BT HalfDuplex Capability*/
        uint32_t partner_10bt_fulldupx : 1;   /*!< Partner 10BT FullDuplex Capability */
        uint32_t partner_100bt_halfdupx : 1;  /*!< Partner 100BT HalfDuplex Capability */
        uint32_t partner_100bt_fulldupx : 1;  /*!< Partner 100BT FullDuplex Capability */
        uint32_t partner_flow_control : 1;    /*!< Partner Flow Control Capability */
        uint32_t link_good : 1;               /*!< Link Good */
        uint32_t auto_nego_done : 1;          /*!< AN Done = auto-negotiation completed */
        uint32_t mdi_x_status : 1;            /*!< MDI-X Status */
    };
    uint32_t val;
} ksz8863_psr0_reg_t;
#define KSZ8863_PSR0_BASE_ADDR (0x1E)
#define KSZ8863_P1SR0_ADDR (0x1E)
#define KSZ8863_P2SR0_ADDR (0x2E)

/**
 * @brief Register 31/47/63: Port x Status 1
 *
 */
typedef union {
    struct {
        uint32_t far_end_fault : 1;               /*!< Far-End Fault (applicable to port 1 only) */
        uint32_t duplex : 1;                      /*!< Link Operation Duplex */
        uint32_t speed : 1;                       /*!< Link Operation Speed */
        uint32_t rx_flow_ctrl_en : 1;             /*!< Receive Flow Control Enable */
        uint32_t tx_flow_ctrl_en : 1;             /*!< Transmit Flow Control Enable */
        uint32_t pol_rvs : 1;                     /*!< polarity is reversed (not applicable to port3) */
        uint32_t reserved_6 : 1;                  /*!< Reserved */
        uint32_t hp_or_microchip_mdix_mode : 1;   /*!< HP Auto MDI/MDI-X mode (not applicable to port3) */
    };
    uint32_t val;
} ksz8863_psr1_reg_t;
#define KSZ8863_PSR1_BASE_ADDR (0x1F)
#define KSZ8863_P1SR1_ADDR (0x1F)
#define KSZ8863_P2SR1_ADDR (0x2F)
#define KSZ8863_P3SR1_ADDR (0x3F)

/**
 * @brief Register 67 (0x43): Reset
 *
 */
typedef union {
    struct {
        uint32_t pcs_reset : 1;      /*!< PCS Reset */
        uint32_t reserved_1_3 : 3;   /*!< Reserved */
        uint32_t sw_reset : 1;       /*!< Software Reset */
    };
    uint32_t val;
} ksz8863_reset_reg_t;
#define KSZ8863_RESET_ADDR (0x43)

/**
 * @brief Register 96-111 (0x60-0x6F): TOS Priority Control Register 0-15
 *
 */
#define KSZ8863_TOS_PCR_BASE_ADDR (0x60)

/**
 * @brief Register 112-117 (0x70-0x75): MAC Address Register 0-5
 *
 */
#define KSZ8863_MACAR_BASE_ADDR (0x70)

/**
 * @brief Register 118-120 (0x76-0x78): User Defined Register 1-3
 *
 */
typedef uint8_t ksz8863_udr_reg_t;
#define KSZ8863_UDR1_ADDR (0x76)
#define KSZ8863_UDR2_ADDR (0x77)
#define KSZ8863_UDR3_ADDR (0x78)

/**
 * @brief Register 121-122 (0x79-0x7A): Indirect Access Control 0-1
 *
 * @note This register structure is intended to be used in multibyte access. However, since ESP32
 *       is little endian architecture, do not forget to swap bytes prior sending to serial bus.
 *
 */
typedef union {
    struct {
        uint16_t addr : 10;         /*!< Indirect Address */
        uint16_t table_sel : 2;     /*!< Table Select */
        uint16_t read_write : 1;    /*!< Read High/Write Low */
        uint16_t reserved : 3;      /*!< Reserved */
    };
    uint16_t val;
} ksz8863_iacr0_1_reg_t;
#define KSZ8863_IACR0_ADDR (0x79)
#define KSZ8863_IACR1_ADDR (0x7A)

#define KSZ8863_INDIR_ACCESS_READ (1)
#define KSZ8863_INDIR_ACCESS_WRITE (0)
typedef enum {
    KSZ8863_STA_MAC_TABLE,
    KSZ8863_VLAN_TABLE,
    KSZ8863_DYN_MAC_TABLE,
    KSZ8863_MIB_CNTRS
} ksz8863_indir_access_tbls_t;

/**
 * @brief Register 123-131 (0x7B-0x83): Indirect Data Register 8-0
 *
 */
#define KSZ8863_IDR8_ADDR (0x7B)
#define KSZ8863_IDR7_ADDR (0x7C)
#define KSZ8863_IDR6_ADDR (0x7D)
#define KSZ8863_IDR5_ADDR (0x7E)
#define KSZ8863_IDR4_ADDR (0x7F)
#define KSZ8863_IDR3_ADDR (0x80)
#define KSZ8863_IDR2_ADDR (0x81)
#define KSZ8863_IDR1_ADDR (0x82)
#define KSZ8863_IDR0_ADDR (0x83)
#define KSZ8863_INDIR_DATA_MAX_SIZE (KSZ8863_IDR0_ADDR - KSZ8863_IDR8_ADDR + 1)

/**
 * @brief Register 147~142 (0x93~0x8E): Station Address 1 MACA1
 *
 */
#define KSZ8863_MACA1_MSB_ADDR (0x93)
#define KSZ8863_MACA1_LSB_ADDR (0x8E)

/**
 * @brief Register 153~148 (0x99~0x94): Station Address 2 MACA2
 *
 */
#define KSZ8863_MACA2_MSB_ADDR (0x99)
#define KSZ8863_MACA2_LSB_ADDR (0x94)

/**
 * @brief Register 198 (0xC6): Forward Invalid VID Frame and Host Mode
 *
 */
typedef union {
    struct {
        uint32_t host_mode_if : 2;
        uint32_t p1_rmii_clk : 1;
        uint32_t p3_rmii_clk : 1;
        uint32_t fwd_inv_vid_frame : 3;
        uint32_t reserved_7 : 1;

    };
    uint32_t val;
} ksz8863_fwdfrmhostm_reg_t;
#define KSZ8863_FWDFRM_HOSTM_ADDR (0xC6)

/**
 * @brief Static MAC Address Table (8 entries)
 *
 */
typedef union {
    struct __attribute__((packed)) {
        uint8_t mac_addr[6];      /*!< MAC Address */
        uint16_t fwd_ports : 3;    /*!< Forwarding Ports */
        uint16_t entry_val : 1;    /*!< Valid */
        uint16_t override : 1;     /*!< Override port TX/RX Enable setting */
        uint16_t use_fid : 1;      /*!< Use FID */
        uint16_t fid : 4;          /*!< Filter VLAN ID */
    };
    uint8_t data[8];
} ksz8863_sta_mac_table_t;
#define KSZ8863_STA_MAC_TBL_MAX_ENTR (8)

/**
 * @brief Dynamic MAC Address Table (1K entries)
 *
 */
typedef union {
    struct __attribute__((packed)) {
        uint8_t mac_addr[6];            /*!< MAC Address */
        uint8_t fid : 4;                /*!< Filter VLAN ID */
        uint8_t src_port : 2;           /*!< Source Port */
        uint8_t ts : 2;                 /*!< Time Stamp */
        uint16_t val_entries : 10;      /*!< Number of Valid Entries */
        uint16_t mac_empty : 1;         /*!< MAC Empty (no valid entry in the table) */
        uint16_t reserved : 4;          /*!< Reserved */
        uint16_t data_not_ready : 1;    /*!< Data Not Ready */
    };
    uint8_t data[KSZ8863_INDIR_DATA_MAX_SIZE];
} ksz8863_dyn_mac_table_t;
#define KSZ8863_DYN_MAC_TBL_MAX_ENTR (0x3ff)

#ifdef __cplusplus
}
#endif
