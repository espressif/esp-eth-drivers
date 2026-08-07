/**
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 *  SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EXTEND_PAGE 0
#define LINK_DIAGNOS_PAGE 2
#define PTP1588_BASE_PAGE 4
#define PTP1588_CFG1_PAGE 5
#define PTP1588_CFG2_PAGE 6

#define DP83640_REG_ADDR(reg)   ((uint32_t)(&((dp83640_dev_reg_t *) 0)->reg) / 4)

// Adj for pin input delay and edge detection time (35ns = 8ns(refclk) * 4 + 3)
#define	PIN_INPUT_DELAY		35

// #define DP83640_ATTR    __attribute__((packed))
#define DP83640_ATTR

/******************************* * IEEE 802.3 Regs **************************** */
/**
 * @brief  Basic Mode Control Register
 * @addr   00h
 * @access RW
 */
typedef union DP83640_ATTR {
    struct {

    };
    uint32_t val;
} dp83640_bmcr_reg_t;

/**
 * @brief  Basic Mode Status Register
 * @addr   01h
 * @access RO
 */
typedef union DP83640_ATTR {
    struct {

    };
    uint32_t val;
} dp83640_bmsr_reg_t;

/**
 * @brief  PHY Identifier Register #1
 * @addr   02h
 * @access RO
 */
typedef union DP83640_ATTR {
    struct {

    };
    uint32_t val;
} dp83640_phyidr1_reg_t;

/**
 * @brief  PHY Identifier Register #2
 * @addr   03h
 * @access RO
 */
typedef union DP83640_ATTR {
    struct {

    };
    uint32_t val;
} dp83640_phyidr2_reg_t;

/**
 * @brief  Auto-Negotiation Advertisement Register
 * @addr   04h
 * @access RW
 */
typedef union DP83640_ATTR {
    struct {

    };
    uint32_t val;
} dp83640_anar_reg_t;

/**
 * @brief  Auto-Negotiation Link Partner Ability Register
 * @addr   05h
 * @access RW
 */
typedef union DP83640_ATTR {
    struct {
        uint32_t selector:5;
        uint32_t ten:1;
        uint32_t ten_fd:1;
        uint32_t tx:1;
        uint32_t tx_fd:1;
        uint32_t t4:1;
        uint32_t pause:1;
        uint32_t asm_dir:1;
        uint32_t reserved12:1;
        uint32_t rf:1;
        uint32_t ack:1;
        uint32_t np:1;
    };
    uint32_t val;
} dp83640_anlpar_reg_t;

/**
 * @brief  Auto-Negotiation Expansion Register
 * @addr   06h
 * @access RW
 */
typedef union DP83640_ATTR {
    struct {

    };
    uint32_t val;
} dp83640_aner_reg_t;

/**
 * @brief  Auto-Negotiation Next Page TX Register
 * @addr   07h
 * @access RW
 */
typedef union DP83640_ATTR {
    struct {

    };
    uint32_t val;
} dp83640_annptr_reg_t;

/**
 * @brief  PHY Status Register
 * @addr   10h
 * @access RO
 */
typedef union DP83640_ATTR {
    struct {
        uint32_t link_status : 1;               /* Link Status  */
        uint32_t speed_status : 1;              /* Speed Status  */
        uint32_t duplex_status : 1;             /* Duplex Status  */
        uint32_t loopback_status : 1;           /* MII Loopback  */
        uint32_t auto_nego_complete : 1;        /* Auto-Negotiation Complete  */
        uint32_t jabber_detect : 1;             /* Jabber Detect  */
        uint32_t remote_fault : 1;              /* Remote Fault  */
        uint32_t mii_interrupt : 1;             /* MII Interrupt Pending  */
        uint32_t page_received : 1;             /* Link Code Word Page Received  */
        uint32_t descrambler_lock : 1;          /* Descrambler Lock  */
        uint32_t signal_detect : 1;             /* Signal Detect  */
        uint32_t false_carrier_sense_latch : 1; /* False Carrier Sense Latch  */
        uint32_t polarity_status : 1;           /* Polarity Status  */
        uint32_t receive_error_latch : 1;       /* Receive Error Latch  */
        uint32_t mdix_mode : 1;                 /* MDI-X mode reported by auto-negotiation  */
        uint32_t reserved : 1;                  /* Reserved  */
    };
    uint32_t val;
} dp83640_physts_reg_t;

/**
 * @brief  MII Interrupt Control Register
 * @addr   11h
 * @access RW
 */
typedef union DP83640_ATTR {
    struct {

    };
    uint32_t val;
} dp83640_micr_reg_t;

/**
 * @brief  MII Interrupt Status and Event Control Register
 * @addr   12h
 * @access RW
 */
typedef union DP83640_ATTR {
    struct {

    };
    uint32_t val;
} dp83640_misr_reg_t;

/**
 * @brief  Page Select Register
 * @addr   13h
 * @access RW
 */
typedef union DP83640_ATTR {
    struct {

    };
    uint32_t val;
} dp83640_pagesel_reg_t;

/*************************** * Page 0: Extended Regs ************************** */
/**
 * @brief  False Carrier Sense Counter Register
 * @addr   14h
 * @access RO
 */
typedef union DP83640_ATTR {
    struct {

    };
    uint32_t val;
} dp83640_fcscr_reg_t;

/**
 * @brief  Receive Error Counter Register
 * @addr   15h
 * @access RO
 */
typedef union DP83640_ATTR {
    struct {

    };
    uint32_t val;
} dp83640_recr_reg_t;

/**
 * @brief  PCS Sub-Layer Configuration and Status Register
 * @addr   16h
 * @access RW
 */
typedef union DP83640_ATTR {
    struct {

    };
    uint32_t val;
} dp83640_pcsr_reg_t;

/**
 * @brief  RMII and Bypass Register
 * @addr   17h
 * @access RW
 */
typedef union DP83640_ATTR {
    struct {

    };
    uint32_t val;
} dp83640_rbr_reg_t;

/**
 * @brief  LED Direct Control Register
 * @addr   18h
 * @access RW
 */
typedef union DP83640_ATTR {
    struct {

    };
    uint32_t val;
} dp83640_ledcr_reg_t;

/**
 * @brief  PHY Control Register
 * @addr   19h
 * @access RW
 */
typedef union DP83640_ATTR {
    struct {
        uint32_t phy_addr : 5;               /* PHY Address  */
        uint32_t led_cfg : 2;                /* LED Configuration Modes  */
        uint32_t bypass_led_stretching : 1;  /* Bypass LED Stretching  */
        uint32_t bist_start : 1;             /* BIST Start  */
        uint32_t bist_status : 1;            /* BIST Test Status  */
        uint32_t psr_15 : 1;                 /* BIST Sequence select  */
        uint32_t bist_force_error : 1;       /* BIST Force Error  */
        uint32_t pause_trans_negotiate : 1;  /* Pause Transmit Negotiated Status  */
        uint32_t pause_receive_negotiat : 1; /* Pause Receive Negotiated Status  */
        uint32_t force_mdix : 1;             /* Force MDIX  */
        uint32_t en_auto_mdix : 1;           /* Auto-MDIX Enable  */
    };
    uint32_t val;
} dp83640_phycr_reg_t;

/**
 * @brief  10Base-T Status/Control Register
 * @addr   1Ah
 * @access RW
 */
typedef union DP83640_ATTR {
    struct {

    };
    uint32_t val;
} dp83640_10btscr_reg_t;

/**
 * @brief  CD Test Control Register and BIST Extensions Register
 * @addr   1Bh
 * @access RW
 */
typedef union DP83640_ATTR {
    struct {

    };
    uint32_t val;
} dp83640_cdctrl1_reg_t;

/**
 * @brief  PHY Control Register 2
 * @addr   1Ch
 * @access RW
 */
typedef union DP83640_ATTR {
    struct {
        uint32_t reserved0 : 1;
        uint32_t clk_out_dis : 1;
        uint32_t reserved2 : 7;
        uint32_t soft_reset : 1;
        uint32_t phyter_comp : 1;
        uint32_t bc_write : 1;
        uint32_t clkout_rx_clk : 1;
        uint32_t sync_enet_en : 1;       /* When Synchronous Ethernet mode is enabled, control of the PTP clock, digital counter, and PTP rate
                                          * adjust logic is switched from the local reference clock to the recovered receive clock,
                                          * Only can be enabled for slave role
                                          */
        uint32_t reserved14 : 2;
    };
    uint32_t val;
} dp83640_phycr2_reg_t;

/**
 * @brief  Energy Detect Control Register
 * @addr   1Dh
 * @access RW
 */
typedef union DP83640_ATTR {
    struct {

    };
    uint32_t val;
} dp83640_edcr_reg_t;

/**
 * @brief  PHY Control Frames Configuration Register
 * @addr   1Fh
 * @access RW
 */
typedef union DP83640_ATTR {
    struct {

    };
    uint32_t val;
} dp83640_pcfcr_reg_t;

/***************************** * Page 1: Test Regs **************************** */
// Skipped

/*********************** * Page 2: Link Diagnostics Regs ********************** */
/**
 * @brief  100 Mb Length Detect Register
 * @addr   14h
 * @access RO
 */
typedef union DP83640_ATTR {
    struct {

    };
    uint32_t val;
} dp83640_len100_det_reg_t;

/**
 * @brief  100 Mb Frequency Offset Indication Register
 * @addr   15h
 * @access RW
 */
typedef union DP83640_ATTR {
    struct {

    };
    uint32_t val;
} dp83640_freq100_reg_t;

/**
 * @brief  TDR Control Register
 * @addr   16h
 * @access RW
 */
typedef union DP83640_ATTR {
    struct {

    };
    uint32_t val;
} dp83640_tdr_ctrl_reg_t;

/**
 * @brief  TDR Window Register
 * @addr   17h
 * @access RW
 */
typedef union DP83640_ATTR {
    struct {

    };
    uint32_t val;
} dp83640_tdr_win_reg_t;

/**
 * @brief  TDR Peak Measurement Register
 * @addr   18h
 * @access RO
 */
typedef union DP83640_ATTR {
    struct {

    };
    uint32_t val;
} dp83640_tdr_peak_reg_t;

/**
 * @brief  TDR Threshold Measurement Register
 * @addr   19h
 * @access RO
 */
typedef union DP83640_ATTR {
    struct {

    };
    uint32_t val;
} dp83640_tdr_thr_reg_t;

/**
 * @brief  Variance Control Register
 * @addr   1Ah
 * @access RW
 */
typedef union DP83640_ATTR {
    struct {

    };
    uint32_t val;
} dp83640_var_ctrl_reg_t;

/**
 * @brief  Variance Data Register
 * @addr   1Bh
 * @access RO
 */
typedef union DP83640_ATTR {
    struct {

    };
    uint32_t val;
} dp83640_var_dat_reg_t;

/**
 * @brief  Link Quality Monitor Register
 * @addr   1Dh
 * @access RW
 */
typedef union DP83640_ATTR {
    struct {

    };
    uint32_t val;
} dp83640_lqmr_reg_t;

/**
 * @brief  Link Quality Data Register
 * @addr   1Eh
 * @access RW
 */
typedef union DP83640_ATTR {
    struct {

    };
    uint32_t val;
} dp83640_lqdr_reg_t;

/**
 * @brief  Link Quality Monitor Register 2
 * @addr   1Fh
 * @access RW
 */
typedef union DP83640_ATTR {
    struct {

    };
    uint32_t val;
} dp83640_lqmr2_reg_t;

/****************************** * Page 3: Reserved **************************** */
// Skipped

/************************* * Page 4: PTP 1588 Base Regs *********************** */
/**
 * @brief  PTP Control Register
 * @addr   14h
 * @access RW
 */
typedef union DP83640_ATTR {
    struct {
        uint32_t reset : 1;
        uint32_t disable : 1;
        uint32_t enable : 1;
        uint32_t step_clk: 1;
        uint32_t load_clk: 1;
        uint32_t rd_clk: 1;
        uint32_t trig_load: 1;
        uint32_t trig_read: 1;
        uint32_t trig_en: 1;
        uint32_t trig_dis: 1;
        uint32_t trig_sel: 3;
        uint32_t reserved: 3;
    };
    uint32_t val;
} dp83640_ptp_ctl_reg_t;

/**
 * @brief  PTP Time Data Register
 * @addr   15h
 * @access RW
 */
typedef union DP83640_ATTR {
    struct {
        uint32_t time_data : 16;
    };
    uint32_t val;
} dp83640_ptp_tdr_reg_t;

/**
 * @brief  PTP Status Register
 * @addr   16h
 * @access RW
 */
typedef union DP83640_ATTR {
    struct {
        uint32_t event_ie : 1;
        uint32_t trig_ie : 1;
        uint32_t rxts_ie : 1;
        uint32_t txts_ie: 1;
        uint32_t reserved04: 4;
        uint32_t event_rdy: 1;
        uint32_t trig_done: 1;
        uint32_t rxts_rdy: 1;
        uint32_t txts_rdy: 1;
        uint32_t reserved0c: 4;
    };
    uint32_t val;
} dp83640_ptp_sts_reg_t;

/**
 * @brief  PTP Trigger Status Register
 * @addr   17h
 * @access RW
 */
typedef union DP83640_ATTR {
    struct {
        uint32_t active : 1;
        uint32_t error : 1;
    } trig[8];
    uint32_t val;
} dp83640_ptp_tsts_reg_t;

/**
 * @brief  PTP Rate Low Register
 * @addr   18h
 * @access RW
 */
typedef union DP83640_ATTR {
    struct {
        uint32_t rate_low : 16;
    };
    uint32_t val;
} dp83640_ptp_ratel_reg_t;

/**
 * @brief  PTP Rate High Register
 * @addr   19h
 * @access RW
 */
typedef union DP83640_ATTR {
    struct {
        uint32_t rate_high : 10;
        uint32_t reserved : 4;
        uint32_t is_tmp_rate : 1;
        uint32_t rate_dir : 1;
    };
    uint32_t val;
} dp83640_ptp_rateh_reg_t;

/**
 * @brief  PTP Page 4 Read Checksum
 * @addr   1Ah
 * @access RO
 */
typedef union DP83640_ATTR {
    struct {

    };
    uint32_t val;
} dp83640_ptp_rdcksum_reg_t;

/**
 * @brief  PTP Page 4 Write Checksum
 * @addr   1Bh
 * @access RO
 */
typedef union DP83640_ATTR {
    struct {

    };
    uint32_t val;
} dp83640_ptp_wrcksum_reg_t;

/**
 * @brief  PTP Transmit TimeStamp Register
 * @addr   1Ch
 * @access RO
 */
typedef union DP83640_ATTR {
    struct {
        uint32_t ts_data : 16;
    };
    uint32_t val;
} dp83640_ptp_txts_reg_t;

/**
 * @brief  PTP Receive TimeStamp Register
 * @addr   1Dh
 * @access RO
 */
typedef union DP83640_ATTR {
    struct {
        uint32_t ts_data : 16;
    };
    uint32_t val;
} dp83640_ptp_rxts_reg_t;

/**
 * @brief  PTP Event Status Register
 * @addr   1Eh
 * @access RO
 */
typedef union DP83640_ATTR {
    struct {
        uint32_t event_det : 1;
        uint32_t mult_event : 1;
        uint32_t event_num : 3;
        uint32_t event_rf: 1;
        uint32_t event_ts_len: 2;
        uint32_t events_missed: 3;
        uint32_t reserved: 4;
    };
    uint32_t val;
} dp83640_ptp_ests_reg_t;

/**
 * @brief  PTP Event Data Register
 * @addr   1Fh
 * @access RO
 */
typedef union DP83640_ATTR {
    struct {
        uint32_t det : 1;
        uint32_t rise : 1;
    } evt[8];
    uint32_t val;
} dp83640_ptp_edata_reg_t;

/******************** * Page 5: PTP 1588 Configuration Regs ******************* */
/**
 * @brief  PTP Trigger Configuration Register
 * @addr   14h
 * @access RW
 */
typedef union DP83640_ATTR {
    struct {
        uint32_t trig_wr : 1;           /* Trigger Configuration Select. Setting this bit will generate a Configuration Write to the selected Trigger. This bit
                                           will always read back as 0 */
        uint32_t trig_csel : 3;         /* Trigger Configuration Select */
        uint32_t reserved4 : 3;         /* Reserved */
        uint32_t trig_toggle : 1;       /* Trigger Configuration Select */
        uint32_t trig_gpio : 4;         /* Trigger GPIO Connection, Setting this field to a non-zero value will connect the Trigger to the associated GPIO
                                           pin. Valid settings for this field are 1 thru 12. */
        uint32_t trig_notify : 1;       /* Trigger Notification Enable. Setting this bit will enable Trigger status to be reported on completion of a Trigger or
                                           on an error detection due to late trigger. If Trigger interrupts are enabled, the
                                           notification will also result in an interrupt being generated. */
        uint32_t trig_if_late : 1;      /* Trigger-if-late Control. Setting this bit will allow an immediate Trigger in the event the Trigger is
                                           programmed to a time value which is less than the current time. This provides a
                                           mechanism for generating an immediate trigger or to immediately begin generating a
                                           periodic signal. For a periodic signal, no notification be generated if this bit is set
                                           and a Late Trigger occurs. */
        uint32_t trig_per : 1;          /* Trigger Periodic. Setting this bit will cause the Trigger to generate a periodic signal. If this bit is 0, the
                                           Trigger will generate a single Pulse or Edge depending on the Trigger Control settings. */
        uint32_t trig_pulse : 1;        /* Trigger Pulse. Setting this bit will cause the Trigger to generate a Pulse rather than a single rising
                                           or falling edge. */
    };
    uint32_t val;
} dp83640_ptp_trig_reg_t;

/**
 * @brief  PTP Event Configuration Register
 * @addr   15h
 * @access RW
 */
typedef union DP83640_ATTR {
    struct {
        uint32_t evnt_wr : 1;           /* Event Configuration Write.
                                           Setting this bit will generate a Configuration Write to the selected Event Timestamp Unit. */
        uint32_t evnt_sel : 3;          /* Event Select. This field selects the Event Timestamp Unit for configuration read or write */
        uint32_t reserved4 : 3;         /* Reserved */
        uint32_t evnt_gpio : 4;         /* Event GPIO Connection. Setting this field to a non-zero value will connect the Event to the associated GPIO
                                           pin. Valid settings for this field are 1 thru 12. */
        uint32_t evnt_single : 1;       /* Single Event Capture. Setting this bit to a 1 will enable single event capture
                                           operation. The EVNT_RISE and EVNT_FALL enables will be cleared upon a valid
                                           event timestamp capture. */
        uint32_t evnt_fall : 1;         /* Event Fall Detect Enable. Enable Detection of Falling edge on Event input */
        uint32_t evnt_rise : 1;         /* Event Rise Detect Enable. Enable Detection of Rising edge on Event input */
        uint32_t reserved15 : 1;        /* Reserved */
    };
    uint32_t val;
} dp83640_ptp_evnt_reg_t;

/**
 * @brief  PTP Transmit Configuration Register 0
 * @addr   16h
 * @access RW
 */
typedef union DP83640_ATTR {
    struct {
        uint32_t tx_ts_en : 1;
        uint32_t tx_ptp_ver : 4;
        uint32_t tx_ipv4_en : 1;
        uint32_t tx_ipv6_en : 1;
        uint32_t tx_l2_en : 1;
        uint32_t ip1588_en : 1;
        uint32_t chk_1step : 1;
        uint32_t crc_1step : 1;
        uint32_t ignore_2step : 1;
        uint32_t ntp_ts_en : 1;
        uint32_t dr_insert : 1;
        uint32_t reserved : 1;
        uint32_t sync_1step : 1;
    };
    uint32_t val;
} dp83640_ptp_txcfg0_reg_t;

/**
 * @brief  PTP Transmit Configuration Register 1
 * @addr   17h
 * @access RW
 */
typedef union DP83640_ATTR {
    struct {
        uint32_t byte0_data : 8;
        uint32_t byte0_mask : 8;
    };
    uint32_t val;
} dp83640_ptp_txcfg1_reg_t;

/**
 * @brief  PHY Status Frames Configuration Register 0
 * @addr   18h
 * @access RW
 */
typedef union DP83640_ATTR {
    struct {
        uint32_t psf_evnt_en : 1;
        uint32_t psf_trig_en : 1;
        uint32_t psf_rxts_en : 1;
        uint32_t psf_txts_en : 1;
        uint32_t psf_err_en : 1;
        uint32_t psf_pcf_rd : 1;
        uint32_t psf_ipv4 : 1;
        uint32_t psf_endian : 1;
        uint32_t min_pre : 3;
        uint32_t mac_src_add : 2;
        uint32_t reserved13 : 3;
    };
    uint32_t val;
} dp83640_psf_cfg0_reg_t;

/**
 * @brief  PTP Receive Configuration Register 0
 * @addr   19h
 * @access RW
 */
typedef union DP83640_ATTR {
    struct {
        uint32_t rx_ts_en : 1;
        uint32_t rx_ptp_ver : 4;
        uint32_t rx_ipv4_en : 1;
        uint32_t rx_ipv6_en : 1;
        uint32_t rx_l2_en : 1;
        uint32_t ip1588_en : 1;
        uint32_t rx_slave : 1;
        uint32_t user_ip_en : 1;
        uint32_t user_ip_sel : 1;
        uint32_t alt_mast_dis : 1;
        uint32_t domain_en : 1;
    };
    uint32_t val;
} dp83640_ptp_rxcfg0_reg_t;

/**
 * @brief  PTP Receive Configuration Register 1
 * @addr   1Ah
 * @access RW
 */
typedef union DP83640_ATTR {
    struct {
        uint32_t byte0_data : 8;
        uint32_t byte0_mask : 8;
    };
    uint32_t val;
} dp83640_ptp_rxcfg1_reg_t;

/**
 * @brief  PTP Receive Configuration Register 2
 * @addr   1Bh
 * @access RW
 */
typedef union DP83640_ATTR {
    struct {
        uint32_t ip_addr_data : 16;
    };
    uint32_t val;
} dp83640_ptp_rxcfg2_reg_t;

/**
 * @brief  PTP Receive Configuration Register 3
 * @addr   1Ch
 * @access RW
 */
typedef union DP83640_ATTR {
    struct {
        uint32_t ptp_domain : 8;
        uint32_t ts_insert : 1;
        uint32_t ts_append : 1;
        uint32_t acc_crc : 1;
        uint32_t acc_udp : 1;
        uint32_t ts_min_cfg : 4;
    };
    uint32_t val;
} dp83640_ptp_rxcfg3_reg_t;

/**
 * @brief  PTP Receive Configuration Register 4
 * @addr   1Dh
 * @access RW
 */
typedef union DP83640_ATTR {
    struct {
        uint32_t rxts_sec_offset : 6;
        uint32_t rxts_nsec_offset : 6;
        uint32_t ts_sec_len : 2;
        uint32_t ts_sec_en : 1;
        uint32_t ipv4_udp_mod : 1;
    };
    uint32_t val;
} dp83640_ptp_rxcfg4_reg_t;

/**
 * @brief  PTP Temporary Rate Duration Low Register
 * @addr   1Eh
 * @access RW
 */
typedef union DP83640_ATTR {
    struct {
        uint32_t ptp_tr_durl : 16;
    };
    uint32_t val;
} dp83640_ptp_trdl_reg_t;

/**
 * @brief  PTP Temporary Rate Duration High Register
 * @addr   1Fh
 * @access RW
 */
typedef union DP83640_ATTR {
    struct {
        uint32_t ptp_tr_durh : 10;
        uint32_t reserved10 : 6;
    };
    uint32_t val;
} dp83640_ptp_trdh_reg_t;

/******************** * Page 6: PTP 1588 Configuration Regs ******************* */
/**
 * @brief  PTP Clock Output Control Register
 * @addr   14h
 * @access RW
 */
typedef union DP83640_ATTR {
    struct {
        uint32_t ptp_clk_div : 8;
        uint32_t reserved8 : 5;
        uint32_t ptp_clk_out_speed_sel : 1;
        uint32_t ptp_clk_out_sel : 1;
        uint32_t ptp_clk_out_en : 1;
    };
    uint32_t val;
} dp83640_ptp_coc_reg_t;

/**
 * @brief  PHY Status Frames Configuration Register 1
 * @addr   15h
 * @access RW
 */
typedef union DP83640_ATTR {
    struct {
        uint32_t msg_type : 4;
        uint32_t trans_specific : 4;
        uint32_t ptp_version : 4;
        uint32_t ptp_reserved : 4;
    };
    uint32_t val;
} dp83640_psf_cfg1_reg_t;

/**
 * @brief  PHY Status Frames Configuration Register 2
 * @addr   16h
 * @access RW
 */
typedef union DP83640_ATTR {
    struct {
        uint32_t ip_sa_byte0 : 8;
        uint32_t ip_sa_byte1 : 8;
    };
    uint32_t val;
} dp83640_psf_cfg2_reg_t;

/**
 * @brief  PHY Status Frames Configuration Register 3
 * @addr   17h
 * @access RW
 */
typedef union DP83640_ATTR {
    struct {
        uint32_t ip_sa_byte2 : 8;
        uint32_t ip_sa_byte3 : 8;
    };
    uint32_t val;
} dp83640_psf_cfg3_reg_t;

/**
 * @brief  PHY Status Frames Configuration Register 4
 * @addr   18h
 * @access RW
 */
typedef union DP83640_ATTR {
    struct {
        uint32_t ip_checksum : 16;
    };
    uint32_t val;
} dp83640_psf_cfg4_reg_t;

/**
 * @brief  PTP SFD Configuration Register
 * @addr   19h
 * @access RW
 */
typedef union DP83640_ATTR {
    struct {
        uint32_t rx_sfd_gpio : 4;
        uint32_t tx_sfd_gpio : 4;
        uint32_t reserved : 8;
    };
    uint32_t val;
} dp83640_ptp_sfdcfg_reg_t;

/**
 * @brief  PTP Interrupt Control Register
 * @addr   1Ah
 * @access RW
 */
typedef union DP83640_ATTR {
    struct {
        uint32_t ptp_int_gpio : 4;
        uint32_t reserved4 : 12;
    };
    uint32_t val;
} dp83640_ptp_intctl_reg_t;

/**
 * @brief  PTP Clock Source Register
 * @addr   1Bh
 * @access RW
 */
typedef union DP83640_ATTR {
    struct {
        uint32_t clk_src_period : 7;
        uint32_t reserved7 : 7;
        uint32_t clk_src : 2;
    };
    uint32_t val;
} dp83640_ptp_clksrc_reg_t;

/**
 * @brief  PTP Ethernet Type Register
 * @addr   1Ch
 * @access RW
 */
typedef union DP83640_ATTR {
    struct {
        uint32_t ptp_eth_type : 16; // default 0xF788
    };
    uint32_t val;
} dp83640_ptp_etr_reg_t;

/**
 * @brief  PTP Offset Register
 * @addr   1Dh
 * @access RW
 */
typedef union DP83640_ATTR {
    struct {
        uint32_t ptp_offset : 8;
        uint32_t reserved9 : 8;
    };
    uint32_t val;
} dp83640_ptp_off_reg_t;

/**
 * @brief  PTP GPIO Monitor Register
 * @addr   1Eh
 * @access RO
 */
typedef union DP83640_ATTR {
    struct {

    };
    uint32_t val;
} dp83640_ptp_gpiomon_reg_t;

/**
 * @brief  PTP Receive Hash Register
 * @addr   1Fh
 * @access RW
 */
typedef union DP83640_ATTR {
    struct {

    };
    uint32_t val;
} dp83640_ptp_rxhash_reg_t;


typedef struct DP83640_ATTR {
    // IEEE 802.3 Regs
    dp83640_bmcr_reg_t	                bmcr;
    dp83640_bmsr_reg_t	                bmsr;
    dp83640_phyidr1_reg_t	            phyidr1;
    dp83640_phyidr2_reg_t	            phyidr2;
    dp83640_anar_reg_t	                anar;
    dp83640_anlpar_reg_t	            anlpar;
    dp83640_aner_reg_t	                aner;
    dp83640_annptr_reg_t	            annptr;
    uint32_t                            reserved08[8];
    // Vendor Specific Regs
    dp83640_physts_reg_t	            physts;
    dp83640_micr_reg_t	                micr;
    dp83640_misr_reg_t	                misr;
    dp83640_pagesel_reg_t	            pagesel;
    union {
        // Page 0: Extended Regs
        struct {
            dp83640_fcscr_reg_t	        fcscr;
            dp83640_recr_reg_t	        recr;
            dp83640_pcsr_reg_t	        pcsr;
            dp83640_rbr_reg_t	        rbr;
            dp83640_ledcr_reg_t	        ledcr;
            dp83640_phycr_reg_t	        phycr;
            dp83640_10btscr_reg_t	    btscr10;
            dp83640_cdctrl1_reg_t	    cdctrl1;
            dp83640_phycr2_reg_t	    phycr2;
            dp83640_edcr_reg_t	        edcr;
            uint32_t                    reserved01e;
            dp83640_pcfcr_reg_t	        pcfcr;
        } page0;
        // Page 1: Test Regs (skipped)
        // Page 2: Link Diagnostics Regs
        struct {
            dp83640_len100_det_reg_t	len100_det;
            dp83640_freq100_reg_t	    freq100;
            dp83640_tdr_ctrl_reg_t	    tdr_ctrl;
            dp83640_tdr_win_reg_t	    tdr_win;
            dp83640_tdr_peak_reg_t	    tdr_peak;
            dp83640_tdr_thr_reg_t	    tdr_thr;
            dp83640_var_ctrl_reg_t	    var_ctrl;
            dp83640_var_dat_reg_t	    var_dat;
            uint32_t                    reserved21c;
            dp83640_lqmr_reg_t	        lqmr;
            dp83640_lqdr_reg_t	        lqdr;
            dp83640_lqmr2_reg_t	        lqmr2;
        } page2;
        // Page 3: Reserved (skipped)
        // Page 4: PTP 1588 Base Regs
        struct {
            dp83640_ptp_ctl_reg_t	    ptp_ctl;
            dp83640_ptp_tdr_reg_t	    ptp_tdr;
            dp83640_ptp_sts_reg_t	    ptp_sts;
            dp83640_ptp_tsts_reg_t	    ptp_tsts;
            dp83640_ptp_ratel_reg_t	    ptp_ratel;
            dp83640_ptp_rateh_reg_t	    ptp_rateh;
            dp83640_ptp_rdcksum_reg_t	ptp_rdcksum;
            dp83640_ptp_wrcksum_reg_t	ptp_wrcksum;
            dp83640_ptp_txts_reg_t	    ptp_txts;
            dp83640_ptp_rxts_reg_t	    ptp_rxts;
            dp83640_ptp_ests_reg_t	    ptp_ests;
            dp83640_ptp_edata_reg_t	    ptp_edata;
        } page4;
        // Page 5: PTP 1588 Configuration Regs
        struct {
            dp83640_ptp_trig_reg_t	    ptp_trig;
            dp83640_ptp_evnt_reg_t	    ptp_evnt;
            dp83640_ptp_txcfg0_reg_t	ptp_txcfg0;
            dp83640_ptp_txcfg1_reg_t	ptp_txcfg1;
            dp83640_psf_cfg0_reg_t	    psf_cfg0;
            dp83640_ptp_rxcfg0_reg_t	ptp_rxcfg0;
            dp83640_ptp_rxcfg1_reg_t	ptp_rxcfg1;
            dp83640_ptp_rxcfg2_reg_t	ptp_rxcfg2;
            dp83640_ptp_rxcfg3_reg_t	ptp_rxcfg3;
            dp83640_ptp_rxcfg4_reg_t	ptp_rxcfg4;
            dp83640_ptp_trdl_reg_t	    ptp_trdl;
            dp83640_ptp_trdh_reg_t	    ptp_trdh;
        } page5;
        // Page 6: PTP 1588 Configuration Regs
        struct {
            dp83640_ptp_coc_reg_t	    ptp_coc;
            dp83640_psf_cfg1_reg_t	    psf_cfg1;
            dp83640_psf_cfg2_reg_t	    psf_cfg2;
            dp83640_psf_cfg3_reg_t	    psf_cfg3;
            dp83640_psf_cfg4_reg_t	    psf_cfg4;
            dp83640_ptp_sfdcfg_reg_t	ptp_sfdcfg;
            dp83640_ptp_intctl_reg_t	ptp_intctl;
            dp83640_ptp_clksrc_reg_t	ptp_clksrc;
            dp83640_ptp_etr_reg_t	    ptp_etr;
            dp83640_ptp_off_reg_t	    ptp_off;
            dp83640_ptp_gpiomon_reg_t	ptp_gpiomon;
            dp83640_ptp_rxhash_reg_t	ptp_rxhash;
        } page6;
    };
} dp83640_dev_reg_t;

#ifdef __cplusplus
}
#endif
