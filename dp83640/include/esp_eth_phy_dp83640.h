/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_eth_phy.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t ptp_version;                   /*!< PTP protocol version, 1 or 2 to choose PTPv1 or PTPv2 */
    struct {
        uint32_t    timestamp : 1;          /*!< Enable Timestamp capture for Transmit */
        uint32_t    ipv4_ts : 1;            /*!< Enables detection of UDP/IPv4 encapsulated PTP event messages */
        uint32_t    ipv6_ts : 1;            /*!< Enables detection of UDP/IPv6 encapsulated PTP event messages */
        uint32_t    l2_ts : 1;              /*!< Enables detection of IEEE 802.3/Ethernet encapsulated PTP event messages */
        uint32_t    ip1588_filter : 1;      /*!< Enable filtering of UDP/IP Event messages using the IANA assigned IP Destination
                                             *   addresses. If this bit is set to 1, packets with IP Destination addresses which do not
                                             *   match the IANA assigned addresses will not be timestamped. This field affects
                                             *   operation for both IPv4 and IPv6. If this field is set to 0, IP destination addresses will
                                             *   be ignored.
                                             */
        uint32_t    ignore_2step : 1;       /*!< If this bit is set to a 0, the device will not insert a timestamp if the Two_Step bit is set
                                             *   in the flags field of the PTP header. If this bit is set to 1, the device will insert a
                                             *   timestamp independent of the setting of the Two_Step flag.
                                             */
        uint32_t    ntp_ts : 1;             /*!< If this bit is set to 0, the device will check the UDP protocol field for a PTP Event
                                             *   message (value 319). If this bit is set to 1, the device will check the UDP protocol
                                             *   field for an NTP message (value 123). This setting applies to the transmit and
                                             *   receive packet parsing engines.
                                             */
        uint32_t    dr_insert : 1;          /*!< If this bit is set to a 1, the device insert the timestamp for transmitted Delay_Req
                                             *   messages into inbound Delay_Resp messages. The most recent timestamp will be
                                             *   used for any inbound Delay_Resp message. The receive timestamp insertion logic
                                             *   must be enabled through the PTP Receive Configuration Registers
                                             */
        uint32_t    chk_1step : 1;          /*!< Enables correction of the UDP checksum for messages which include insertion of
                                             *   the timestamp. The checksum is corrected by modifying the last two bytes of the
                                             *   UDP data. The last two bytes must be transmitted by the MAC as 0’s. This control
                                             *   must be set for proper IPv6/UDP One-Step operation. This control will have no
                                             *   effect for Layer2 Ethernet messages
                                             */
        uint32_t    crc_1step : 1;          /*!< If this bit is set to a 0, the device will force a CRC error for One-Step operation if the
                                             *   incoming frame has a CRC error. If this bit is set to a 1, the device will send the
                                             *   One- Step frame with a valid CRC, even if the incoming CRC is invalid.
                                             */
        uint32_t    sync_1step : 1;         /*!< Enable automatic insertion of timestamp into transmit Sync Messages. Device will
                                             *   automatically parse message and insert the timestamp in the correct location. UPD
                                             *   checksum and CRC fields will be regenerated
                                             */
    } flags;
} dp83640_tx_config_t;

typedef struct {
    uint32_t ptp_version;                   /*!< PTP protocol version, 1 or 2 to choose PTPv1 or PTPv2 */
    enum {
        IP_FILT_224_0_1_129 = 0x01,         /*!< Dest IP address = 224.0.1.129 */
        IP_FILT_224_0_1_130_132 = 0x02,     /*!< Dest IP address = 224.0.1.130-132 */
        IP_FILT_224_0_0_107 = 0x04,         /*!< Dest IP address = 224.0.0.107 */
    } ptp_ip_filter_mask;                   /*!< Enable detection of UDP/IP Event messages using the IANA assigned IP
                                             *   Destination addresses. This field affects operation for both IPv4 and IPv6. A
                                             *   Timestamp is captured for the PTP message if the IP destination address matches
                                             *   the masked IPs
                                             */
    uint32_t ptp_domain;                    /*!< Value of the PTP Message domainNumber field. If PTP_RXCFG0:DOMAIN_EN is
                                             *   set to 1, the Receive Timestamp unit will only capture a Timestamp if the
                                             *   domainNumber in the receive PTP message matches the value in this field. If the
                                             *   DOMAIN_EN bit is set to 0, the domainNumber field will be ignored.
                                             */
    struct {
        uint32_t    timestamp : 1;          /*!< Enable Timestamp capture for Receive */
        uint32_t    ipv4_ts : 1;            /*!< Enables detection of UDP/IPv4 encapsulated PTP event messages */
        uint32_t    ipv6_ts : 1;            /*!< Enables detection of UDP/IPv6 encapsulated PTP event messages */
        uint32_t    l2_ts : 1;              /*!< Enables detection of IEEE 802.3/Ethernet encapsulated PTP event messages */
        uint32_t    slave : 1;              /*!< By default, the Receive Timestamp Unit will provide Timestamps for event
                                             *   messages meeting other requirements. Setting this bit to a 1 will prevent Delay_Req
                                             *   messages from being Timestamped by requiring that the Control Field (offset 32 in
                                             *   the PTP message) be set to a value other than 1
                                             */
        uint32_t    no_alt_mst : 1;         /*!< Disables timestamp generation if the Alternate_Master flag is set:
                                             *   1 = Do not generate timestamp if Alternate_Master = 1 0 = Ignore Alternate_Master flag
                                             */
        uint32_t    domain : 1;             /*!< If set to 1, the Receive Timestamp unit will require the Domain field to match the
                                             *   value programmed in the PTP_DOMAIN field of the PTP_RXCFG3 register. If set to
                                             *   0, the Receive Timestamp will ignore the PTP_DOMAIN field
                                            */
    } flags;

} dp83640_ptp_rx_config_t;

typedef struct {
    uint32_t ts_nsec_offset;                /*!< This field provides an offset to the Nanoseconds field when inserting a Timestamp
                                             *   into a received PTP message. If TS_APPEND is set to 1, the offset indicates an
                                             *   offset from the end of the PTP message. If TS_APPEND is set to 0, the offset
                                             *   indicates the byte offset from the beginning of the PTP message. This field will be
                                             *   ignored if TS_INSERT is 0. */
    uint32_t ts_sec_offset;                 /*!< This field provides an offset to the Seconds field when inserting a Timestamp into a
                                             *   received PTP message. If TS_APPEND is set to 1, the offset indicates an offset
                                             *   from the end of the inserted Nanoseconds field. If TS_APPEND is set to 0, the offset
                                             *   indicates the byte offset from the beginning of the PTP message. This field will be
                                             *   ignored if TS_INSERT is 0
                                             */
    uint32_t ts_min_ifg;                    /*!< Minimum Inter-frame Gap:
                                             *   2 When a Timestamp is appended to a PTP Message, the length of the packet may
                                             *   get extended. This could reduce the Inter-frame Gap (IFG) between packets by as
                                             *   much as 8 byte times (640 ns at 100 Mb). This field sets a minimum on the IFG
                                             *   between packets in number of byte times. If the IFG is set larger than the actual
                                             *   IFG, preamble bytes of the subsequent packet will get dropped. This value should
                                             *   be set to the lowest possible value that the attached MAC can support.
                                             */
    enum {
        INSERT_SEC_LEN_ONE_LSB_BYTE,        /*!< Least Significant Byte only of Seconds field */
        INSERT_SEC_LEN_TWO_LSB_BYTE,        /*!< Two Least Significant Bytes of Seconds field */
        INSERT_SEC_LEN_THREE_LSB_BYTE,      /*!< Three Least Significant Bytes of Seconds field */
        INSERT_SEC_LEN_ALL_BYTE,            /*!< All four Bytes of Seconds field */
    } sec_len;                              /*!< This field indicates the length of the Seconds field to be inserted in the PTP
                                             *   message. This field will be ignored if TS_INSERT is 0 or if TS_SEC_EN is 0
                                             */
    struct {
        uint32_t insert_sec_en:1;           /*!< Setting this bit to a 1 enables inserting a seconds field when Timestamp Insertion is
                                             *   enabled. If set to 0, only the nanoseconds portion of the Timestamp will be inserted
                                             *   in the packet. This bit will be ignored if TS_INSERT is 0
                                             */
        uint32_t append_l2_ts:1;            /*!< For Layer 2 encapsulated PTP messages, if this bit is set, always append the
                                             *   Timestamp to end of the PTP message rather than inserted in unused message
                                             *   fields. This bit will be ignored if TS_INSERT is 0
                                             */
        uint32_t rec_crc_err_ts:1;          /*!< By default, Timestamps will be discarded for packets with CRC errors. If this bit is
                                             *   set, then the Timestamp will be made available in the normal manner.
                                             */
        uint32_t rec_udp_err_checksum_ts:1; /*!< By default, Timestamps will be discarded for packets with UDP Checksum errors. If
                                             *   this bit is set, then the Timestamp will be made available in the normal manner.
                                             */
        uint32_t udp_checksum_update:1;     /*!< When timestamp insertion is enabled, this bit controls how UDP checksums are
                                             *   handled for IPV4 PTP event messages.
                                             *   If set to a 0, the device will clear the UDP checksum. If a UDP checksum error is
                                             *   detected the device will force a CRC error.
                                             *   If set to a 1, the device will not clear the UDP checksum. Instead it will generate a 2-
                                             *   byte value to correct the UDP checksum and append this immediately following the
                                             *   PTP message. If an incoming UDP checksum error is detected, the device will
                                             *   cause a UDP checksum error in the modified field. This function should only be used
                                             *   if the incoming packets contain two extra bytes of UDP data following the PTP
                                             *   message. This should not be enabled for systems using version 1 of the IEEE 1588
                                             *   specification.
                                             */
    } flags;
} dp83640_rxts_insert_config_t;

typedef struct {
    uint32_t trig_id;                       /*!< Selects the Trigger for loading control information or for enabling the Trigger */
    uint32_t trig_phy_gpio;                 /*!< Setting this field to a non-zero value will connect the Trigger to the associated GPIO
                                             *   pin of dp83640. Valid settings for this field are 1 thru 12
                                             */
    struct {
        uint32_t    gen_pulse : 1;          /*!< Setting this bit will cause the Trigger to generate a Pulse rather than a single rising
                                             *   or falling edge
                                             */
        uint32_t    periodic : 1;           /*!< Setting this bit will cause the Trigger to generate a periodic signal. If this bit is 0, the
                                             *   Trigger will generate a single Pulse or Edge depending on the Trigger Control
                                             *   settings
                                             */
        uint32_t    if_late : 1;            /*!< Setting this bit will allow an immediate Trigger in the event the Trigger is
                                             *   programmed to a time value which is less than the current time. This provides a
                                             *   mechanism for generating an immediate trigger or to immediately begin generating a
                                             *   periodic signal. For a periodic signal, no notification be generated if this bit is set
                                             *   and a Late Trigger occurs
                                             */
        uint32_t    notify : 1;             /*!< Setting this bit will enable Trigger status to be reported on completion of a Trigger or
                                             *   on an error detection due to late trigger. If Trigger interrupts are enabled, the
                                             *   notification will also result in an interrupt being generated
                                             */
        uint32_t    toggle : 1;             /*!< Setting this bit will put the trigger into toggle mode. In toggle mode, the initial value
                                             *   will be ignored and the trigger output will be toggled at the trigger time
                                             */
    } flags;
} dp83640_trig_behavior_config_t;

typedef struct {
    uint32_t evt_id;                        /*!< Selects the Event Timestamp Unit for configuration read or write */
    uint32_t evt_phy_gpio;                  /*!< Setting this field to a non-zero value will connect the Event to the associated GPIO
                                             *   pin. Valid settings for this field are 1 thru 12.
                                             */
    struct {
        uint32_t rise_evt : 1;              /*!< Enable Detection of Rising edge on Event input */
        uint32_t fall_evt : 1;              /*!< Enable Detection of Falling edge on Event input */
        uint32_t single_ent : 1;            /*!< Setting this bit to a 1 will enable single event capture
                                             *   operation. The EVNT_RISE and EVNT_FALL enables will be cleared upon a valid
                                             *   event timestamp capture
                                             */
    } flags;
} dp83640_evt_config_t;

typedef struct {
    uint32_t ptp_eth_type;                  /*!< This field contains the Ethernet Type field used to detect PTP messages transported
                                             *   over Ethernet layer 2
                                             */
    uint32_t ptp_offset;                    /*!< This field contains the offset in bytes to the PTP Message from the preceding
                                             *   header. For Layer2, this is the offset from the Ethernet Type Field. For UDP/IP, it is
                                             *   the offset from the end of the UDP Header.
                                             */
    uint32_t tx_sfd_gpio;                   /*!< This field controls the GPIO output to which the TX SFD signal is assigned. Valid
                                             *   values are 0 (disabled) or 1-12.
                                             */
    uint32_t rx_sfd_gpio;                   /*!< This field controls the GPIO output to which the RX SFD signal is assigned. Valid
                                             *   values are 0 (disabled) or 1-12.
                                             */
} dp83640_misc_config_t;

typedef enum {
    CLK_SRC_PGM_125M,                       /* 125 MHz from internal PGM */
    CLK_SRC_PGM_DIVN,                       /* Divide-by-N from 125-MHz internal PGM */
    CLK_SRC_EXT,                            /* External reference clock */
} dp83640_clk_src_t;

typedef struct {
    enum {
        OUT_CLK_SRC_FCO,                    /*!< Select the Frequency-Controlled Oscillator (FCO) as the root clock for
                                             *   generating the divide-by-N output.
                                             */
        OUT_CLK_SRC_PGM,                    /*!< Select the Phase Generation Module (PGM) as the root clock for generating the
                                             *   divide-by-N output.
                                             */
    } out_clk_src;
    uint32_t clk_div;                       /*!< This field sets the divide-by value for the output clock. The output clock is divided
                                             *   from an internal 250 MHz clock. Valid values range from 2 to 255 (0x02 to 0xFF),
                                             *   giving a nominal output frequency range of 125 MHz down to 980.4 kHz. Divide-by
                                             *   values of 0 and 1 are not valid and will stop the output clock.
                                             */
    bool     faster_edge_en;                /*!<  Enable faster rise/fall time for the divide-by-N clock output pin */
} dp83640_out_clk_config_t;

typedef struct {
    uint32_t min_preamble;                  /*!< Determines the minimum preamble bytes required for sending packets on the MII
                                             *   interface. It is recommended that this be set to the smallest value the MAC will tolerate
                                             */
    enum {
        MAC_08_00_17_0B_6B_0F,              /*!< Use Mac Address [08 00 17 0B 6B 0F] */
        MAC_08_00_17_00_60_00,              /*!< Use Mac Address [08 00 17 00 00 00] */
        MAC_MULTICAST,                      /*!< Use Mac Multicast Dest Address */
        MAC_00_00_00_00_00_00,              /*!< Use Mac Address [00 00 00 00 00 00] */
    } ptp_mac_addr;                         /*!< PHY Status Frame Mac Source Address */
    struct {
        uint32_t event : 1;                 /*!< Enable PHY Status Frame delivery of Event Timestamps */
        uint32_t trigger : 1;               /*!< Enable PHY Status Frame delivery of Trigger Status */
        uint32_t rx_ts : 1;                 /*!< Enable PHY Status Frame delivery of Receive Timestamps */
        uint32_t tx_ts : 1;                 /*!< Enable PHY Status Frame delivery of Transmit Timestamps */
        uint32_t err_en : 1;                /*!< Enable PHY Status Frame delivery of PHY Status Frame Errors. This bit will not
                                             *   independently enable PHY Status Frame operation. One of the other enable bits
                                             *   must be set for PHY Status Frames to be generated
                                             */
        uint32_t ipv4_en : 1;               /*!< This bit controls the type of packet used for PHY Status Frames.
                                             *   0 = Layer2 Ethernet packets
                                             *   1 = IPv4 packets
                                             */
        uint32_t psf_endian : 1;            /*!< For each 16-bit field in a Status Message, the data will normally be presented in
                                             *   network byte order (Most significant byte first). If this bit is set to a 1, the byte data
                                             *   fields will be reversed so that the least significant byte is first
                                             */
    } flags;
} dp83640_psf_config_t;

typedef struct {
    uint32_t msg_type;                      /*!< This field contains the MESSAGETYPE field to be sent in status packets from the
                                             *   PHY to the local MAC using the MII receive data interface
                                             */
    uint32_t transport_specific;            /*!< This field contains the MESSAGETYPE field to be sent in status packets from the
                                             *   PHY to the local MAC using the MII receive data interface
                                             */
    uint32_t ptp_version;                   /*!< This field contains the versionPTP field to be sent in status packets from the PHY to
                                             *   the local MAC using the MII receive data interface
                                             */
    uint32_t ptp_reserved;                  /*!< This field contains the reserved 4-bit field (at offset 1) to be sent in status packets
                                             *   from the PHY to the local MAC using the MII receive data interface
                                             */
} dp83640_ptp_frame_header_t;

typedef struct {
    uint32_t    trig_id;                    /*!< Selects the Trigger for loading control information or for enabling the Trigger */
    uint32_t    expire_time_sec;            /*!< The seconds of expire time of this trigger */
    uint32_t    expire_time_nsec;           /*!< The nano seconds of  expire time of this trigger */
    uint32_t    pulse_width;                /*!< Pulse width */
    uint32_t    pulse_width2;               /*!< Pulse width 2 */
    bool        is_init;
    bool        wait_rollover;
} dp83640_trigger_config_t;

typedef enum {
    PTP_EVENT_TIMESTAMP_READY   = 0x01,     /*!< A PTP Event Timestamp is available. This bit will be cleared upon read of the PTP
                                             *   Event Status Register if no other event timestamps are ready
                                             */
    PTP_TRIGGER_DONE            = 0x02,     /*!< A PTP Trigger has occured. This bit will be cleared upon read. This bit will only be
                                             *   set if Trigger Notification is turned on for the Trigger through the Trigger
                                             *   Configuration Registers
                                             */
    PTP_RX_TIMESTAMP_READY      = 0x04,     /*!< A Receive Timestamp is available for an inbound PTP Message. This bit will be
                                             *   cleared upon read of the Receive Timestamp if no other timestamps are ready
                                             */
    PTP_TX_TIMESTAMP_READY      = 0x08,     /*!< A Transmit Timestamp is available for an outbound PTP Message. This bit will be
                                             *   cleared upon read of the Transmit Timestamp if no other timestamps are ready
                                             */
    PTP_EVENT_MAX               = 0x0F,
} dp83640_event_status_t;

typedef struct phy_dp83640_s    *dp83640_handle_t;

/**
 * @brief Create a PHY instance of DP83640
 *
 * @param[in]  config  Configuration of PHY
 * @return
 *      - instance: create PHY instance successfully
 *      - NULL: create PHY instance failed because some error occurred
 */
esp_eth_phy_t *esp_eth_phy_new_dp83640(const eth_phy_config_t *config);

/**
 * @brief Enable the PTP feature of dp83640
 *
 * @param[in]  dp83640  The dp83640 handle that created by 'esp_eth_phy_new_dp83640'
 * @param[in]  enable   Set true to enable PTP feature
 * @return
 *      - ESP_OK: Enable PTP feature successfully
 *      - ESP_FAIL: Write PHY register failed because some error occurred
 */
esp_err_t dp83640_ptp_enable(dp83640_handle_t dp83640, bool enable);

/**
 * @brief Reset the PTP module in dp83640
 *
 * @param[in]  dp83640  The dp83640 handle that created by 'esp_eth_phy_new_dp83640'
 * @return
 *      - ESP_OK: Reset the PTP module successfully
 *      - ESP_FAIL: Write PHY register failed because some error occurred
 */
esp_err_t dp83640_ptp_reset(dp83640_handle_t dp83640);

/**
 * @brief Set PTP time directly
 *
 * @param[in]  dp83640  The dp83640 handle that created by 'esp_eth_phy_new_dp83640'
 * @param[in]  sec      Seconds
 * @param[in]  nano_sec Nano Seconds
 * @return
 *      - ESP_OK: Set PTP time successfully
 *      - ESP_FAIL: Write PHY register failed because some error occurred
 */
esp_err_t dp83640_ptp_set_time(dp83640_handle_t dp83640, uint32_t sec, uint32_t nano_sec);

/**
 * @brief Get PTP time
 *
 * @param[in]  dp83640  The dp83640 handle that created by 'esp_eth_phy_new_dp83640'
 * @param[out] sec      Seconds, can be NULL if not needed
 * @param[out] nano_sec Nano Seconds, can be NULL if not needed
 * @return
 *      - ESP_OK: Get PTP time successfully
 *      - ESP_FAIL: Write PHY register failed because some error occurred
 */
esp_err_t dp83640_ptp_get_time(dp83640_handle_t dp83640, uint32_t *sec, uint32_t *nano_sec);

/**
 * @brief Adjust PTP time
 *
 * @param[in]  dp83640  The dp83640 handle that created by 'esp_eth_phy_new_dp83640'
 * @param[in]  sec      Seconds
 * @param[in]  nano_sec Nano Seconds
 * @return
 *      - ESP_OK: Adjust PTP time successfully
 *      - ESP_FAIL: Write PHY register failed because some error occurred
 */
esp_err_t dp83640_ptp_adjust_time(dp83640_handle_t dp83640, int32_t sec, int32_t nano_sec);

/**
 * @brief Set the normal time counting rate of the PTP timestamp
 *
 * @param[in]  dp83640  The dp83640 handle that created by 'esp_eth_phy_new_dp83640'
 * @param[in]  rate     Counting rate
 * @param[in]  dir      Counting direction
 * @return
 *      - ESP_OK: Set the normal time counting rate successfully
 *      - ESP_FAIL: Write PHY register failed because some error occurred
 */
esp_err_t dp83640_ptp_set_normal_rate(dp83640_handle_t dp83640, uint32_t rate, bool dir);

/**
 * @brief Set the temporary time counting rate of the PTP timestamp
 *
 * @param[in]  dp83640  The dp83640 handle that created by 'esp_eth_phy_new_dp83640'
 * @param[in]  rate     Counting rate
 * @param[in]  duration The duration of the temporary counting rate,
 *                      will restore to the normal rate after the temporary rate expired
 * @param[in]  dir      Counting direction
 * @return
 *      - ESP_OK: successfully
 *      - ESP_FAIL: Write PHY register failed because some error occurred
 */
esp_err_t dp83640_ptp_set_tmp_rate(dp83640_handle_t dp83640, uint32_t rate, uint32_t duration, bool dir);

/**
 * @brief Get the TX timestamp
 * @note  The device can buffer four timestamps
 *
 * @param[in]  dp83640  The dp83640 handle that created by 'esp_eth_phy_new_dp83640'
 * @param[out] sec      Seconds of the tx timestamp, can be NULL if not needed
 * @param[out] nano_sec Nano Seconds of the tx timestamp, can be NULL if not needed
 * @param[out] ovf_cnt  The count that tx timestamp were dropped due to an overflow of the queue (<=3), can be NULL if not needed
 * @return
 *      - ESP_OK: Get the TX timestamp successfully
 *      - ESP_FAIL: Write PHY register failed because some error occurred
 */
esp_err_t dp83640_ptp_get_tx_timestamp(dp83640_handle_t dp83640, uint32_t *sec, uint32_t *nano_sec, uint32_t *ovf_cnt);

/**
 * @brief Get the RX timestamp
 * @note  The device can buffer four timestamps
 *
 * @param[in]  dp83640  The dp83640 handle that created by 'esp_eth_phy_new_dp83640'
 * @param[out] sec      Seconds of the rx timestamp, can be NULL if not needed
 * @param[out] nano_sec Nano Seconds of the rx timestamp, can be NULL if not needed
 * @param[out] ovf_cnt  The count that rx timestamp were dropped due to an overflow of the queue (<=3), can be NULL if not needed
 * @param[out] sequence_id  The Sequence ID of the received PTP event packet
 * @param[out] msg_type The message type of the PTP event packet
 * @param[out] src_hash 12-bit hash value for octets 20-29 of the PTP event message
 * @return
 *      - ESP_OK: successfully
 *      - ESP_FAIL: Write PHY register failed because some error occurred
 */
esp_err_t dp83640_ptp_get_rx_timestamp(dp83640_handle_t dp83640, uint32_t *sec, uint32_t *nano_sec, uint32_t *ovf_cnt,
                                       uint32_t *sequence_id, uint8_t *msg_type, uint32_t *src_hash);

/**
 * @brief Set the TX configuration
 *
 * @param[in]  dp83640  The dp83640 handle that created by 'esp_eth_phy_new_dp83640'
 * @param[in]  tx_cfg   Tx configuration
 * @return
 *      - ESP_OK: Set the TX configuration successfully
 *      - ESP_FAIL: Write PHY register failed because some error occurred
 */
esp_err_t dp83640_ptp_set_tx_config(dp83640_handle_t dp83640, const dp83640_tx_config_t *tx_cfg);

/**
 * @brief Set the data and mask fields to filter the first byte in a transmitted PTP Message
 * @note  This function will be disabled if all the mask bits are set to 0
 *
 * @param[in]  dp83640  The dp83640 handle that created by 'esp_eth_phy_new_dp83640'
 * @param[in]  mask     Data to be used for matching Byte0 of the PTP Message
 * @param[in]  data     Bit mask to be used for matching Byte0 of the PTP Message. A one in any bit
 *                      enables matching for the associated data bit. If no matching is required, all bits of
 *                      the mask should be set to 0.
 * @return
 *      - ESP_OK: Set the data and mask fields successfully
 *      - ESP_FAIL: Write PHY register failed because some error occurred
 */
esp_err_t dp83640_ptp_set_tx_first_byte_filter(dp83640_handle_t dp83640, uint8_t mask, uint8_t data);

/**
 * @brief Set the RX configuration
 *
 * @param[in]  dp83640  The dp83640 handle that created by 'esp_eth_phy_new_dp83640'
 * @param[in]  rx_cfg   RX configuration
 * @return
 *      - ESP_OK: Set the RX configuration successfully
 *      - ESP_FAIL: Write PHY register failed because some error occurred
 */
esp_err_t dp83640_ptp_set_rx_config(dp83640_handle_t dp83640, const dp83640_ptp_rx_config_t *rx_cfg);

/**
 * @brief Enable and set the detection of UDP/IP Event messages using a programmable IP addresses
 *
 * @param[in]  dp83640  The dp83640 handle that created by 'esp_eth_phy_new_dp83640'
 * @param[in]  usr_ip   The user IP that should be detected by dp83640
 * @return
 *      - ESP_OK: Enable and set the detection of UDP/IP Event messages successfully
 *      - ESP_FAIL: Write PHY register failed because some error occurred
 */
esp_err_t dp83640_ptp_set_rx_usr_ip_filter(dp83640_handle_t dp83640, uint32_t usr_ip);

/**
 * @brief Set the data and mask fields to filter the first byte in a received PTP Message
 *
 * @param[in]  dp83640  The dp83640 handle that created by 'esp_eth_phy_new_dp83640'
 * @param[in]  mask     Data to be used for matching Byte0 of the PTP Message
 * @param[in]  data     Bit mask to be used for matching Byte0 of the PTP Message. A one in any bit
 *                      enables matching for the associated data bit. If no matching is required, all bits of
 *                      the mask should be set to 0.
 * @return
 *      - ESP_OK: Set the data and mask fields successfully
 *      - ESP_FAIL: Write PHY register failed because some error occurred
 */
esp_err_t dp83640_ptp_set_rx_first_byte_filter(dp83640_handle_t dp83640, uint8_t mask, uint8_t data);

/**
 * @brief Enables Timestamp insertion into a received packet that containing a PTP Event Message
 * @note  If this bit is set, the RX Timestamp will not be available through dp83640_ptp_get_rx_timestamp
 *
 * @param[in]  dp83640  The dp83640 handle that created by 'esp_eth_phy_new_dp83640'
 * @param[in]  insert_cfg   Insertion configuration
 * @return
 *      - ESP_OK: Enables Timestamp insertion successfully
 *      - ESP_FAIL: Write PHY register failed because some error occurred
 */
esp_err_t dp83640_ptp_enable_rx_timestamp_insertion(dp83640_handle_t dp83640, const dp83640_rxts_insert_config_t *insert_cfg);

/**
 * @brief Disable Timestamp insertion into a received packet that containing a PTP Event Message
 *
 * @param[in]  dp83640  The dp83640 handle that created by 'esp_eth_phy_new_dp83640'
 * @return
 *      - ESP_OK: Disable Timestamp insertion successfully
 *      - ESP_FAIL: Write PHY register failed because some error occurred
 */
esp_err_t dp83640_ptp_disable_rx_timestamp_insertion(dp83640_handle_t dp83640);

/**
 * @brief Set IEEE 1588 Triggers behavior
 *
 * @param[in]  dp83640  The dp83640 handle that created by 'esp_eth_phy_new_dp83640'
 * @param[in]  trig_bhv_cfg IEEE 1588 trigger configuration
 * @return
 *      - ESP_OK: Set IEEE 1588 Triggers behavior successfully
 *      - ESP_FAIL: Write PHY register failed because some error occurred
 */
esp_err_t dp83640_ptp_set_trigger_behavior(dp83640_handle_t dp83640, const dp83640_trig_behavior_config_t *trig_bhv_cfg);

/**
 * @brief Register an IEEE 1588 Trigger
 *
 * @param[in]  dp83640  The dp83640 handle that created by 'esp_eth_phy_new_dp83640'
 * @param trig_cfg
 * @return
 *      - ESP_OK: Register the IEEE 1588 Trigger successfully
 *      - ESP_FAIL: Write PHY register failed because some error occurred
 */
esp_err_t dp83640_ptp_register_trigger(dp83640_handle_t dp83640, const dp83640_trigger_config_t *trig_cfg);

/**
 * @brief Has the trigger expired or not
 *
 * @param[in]  dp83640  The dp83640 handle that created by 'esp_eth_phy_new_dp83640'
 * @param[in]  trig_id  The trigger id
 * @param[out] has_expired If the trigger has expired. True: expired, False: not expired
 * @return
 *      - ESP_OK: Get the trigger expired status successfully
 *      - ESP_FAIL: Write PHY register failed because some error occurred
 */
esp_err_t dp83640_ptp_has_trigger_expired(dp83640_handle_t dp83640, uint32_t trig_id, bool *has_expired);

/**
 * @brief Unregister the IEEE 1588 Trigger
 *
 * @param[in]  dp83640  The dp83640 handle that created by 'esp_eth_phy_new_dp83640'
 * @param[in]  trig_id  The trigger id
 * @return
 *      - ESP_OK: Unregister the IEEE 1588 Trigger successfully
 *      - ESP_FAIL: Write PHY register failed because some error occurred
 */
esp_err_t dp83640_ptp_unregister_trigger(dp83640_handle_t dp83640, uint32_t trig_id);

/**
 * @brief Configure the IEEE 1588 Events
 *
 * @param[in]  dp83640  The dp83640 handle that created by 'esp_eth_phy_new_dp83640'
 * @param[in]  evt_cfg  Event configuration
 * @return
 *      - ESP_OK: Configure the IEEE 1588 Events successfully
 *      - ESP_FAIL: Write PHY register failed because some error occurred
 */
esp_err_t dp83640_ptp_config_event(dp83640_handle_t dp83640, const dp83640_evt_config_t * evt_cfg);

/**
 * @brief Get the IEEE 1588 Event status
 *
 * @param[in]  dp83640  The dp83640 handle that created by 'esp_eth_phy_new_dp83640'
 * @param[out] evt_status The event status
 * @return
 *      - ESP_OK: Get the IEEE 1588 Event status successfully
 *      - ESP_FAIL: Write PHY register failed because some error occurred
 */
esp_err_t dp83640_ptp_get_event_status(dp83640_handle_t dp83640, dp83640_event_status_t *evt_status);

/**
 * @brief Get the IEEE 1588 Event
 *
 * @param[in]  dp83640  The dp83640 handle that created by 'esp_eth_phy_new_dp83640'
 * @param[out] evt_bits The event mask that has been detected
 * @param[out] rise_flags       The flag to indicate the events that masked by 'evt_bits' rise or fall, 1 for rise and 0 for fall
 * @param[out] evt_time_sec     The second of the event time
 * @param[out] evt_time_nsec    The nano second of the event time
 * @param[out] evt_missed       Whether the event is missed
 * @return
 *      - ESP_OK: Get the IEEE 1588 Event successfully
 *      - ESP_FAIL: Write PHY register failed because some error occurred
 */
esp_err_t dp83640_ptp_get_event(dp83640_handle_t dp83640, uint32_t *event_bits, uint32_t *rise_flags,
                                uint32_t *evt_time_sec, uint32_t *evt_time_nsec, uint32_t *evt_missed);


/**
 * @brief Configure the miscellaneous PTP configurations
 *
 * @param[in]  dp83640  The dp83640 handle that created by 'esp_eth_phy_new_dp83640'
 * @param[in]  misc_cfg Miscellaneous configurations
 * @return
 *      - ESP_OK: Configure the miscellaneous configurations successfully
 *      - ESP_FAIL: Write PHY register failed because some error occurred
 */
esp_err_t dp83640_ptp_config_misc(dp83640_handle_t dp83640, const dp83640_misc_config_t *misc_cfg);

/**
 * @brief Set the PTP clock source
 *
 * @param[in]  dp83640  The dp83640 handle that created by 'esp_eth_phy_new_dp83640'
 * @param[in]  clk_src  PTP clock source:
 *                      00 : 125 MHz from internal Phase Generation Module (PGM) (default)
 *                      01 : Divide-by-N from 125-MHz internal PGM
 *                      1x : External reference clock
 * @param[in]  period   This field configures the PTP clock source period in nanoseconds. Values less than
 *                      8 are invalid and cannot be written; attempting to write a value less than 8 will cause
 *                      CLK_SRC_PER to be 8. When the clock source selection is the Divide-by-N from
 *                      the internal PGM, bits 6:3 are used as the N value; bits 2:0 are ignored in this mode.
 * @return
 *      - ESP_OK: successfully
 *      - ESP_FAIL: Write PHY register failed because some error occurred
 */
esp_err_t dp83640_ptp_set_clk_src(dp83640_handle_t dp83640, dp83640_clk_src_t clk_src, uint32_t period);

/**
 * @brief Enable PTP clock output
 *
 * @param[in]  dp83640  The dp83640 handle that created by 'esp_eth_phy_new_dp83640'
 * @param[in]  out_clk_cfg The configuration of the output clock
 * @return
 *      - ESP_OK: Enable PTP clock output successfully
 *      - ESP_FAIL: Write PHY register failed because some error occurred
 */
esp_err_t dp83640_ptp_enable_clock_output(dp83640_handle_t dp83640, const dp83640_out_clk_config_t * out_clk_cfg);

/**
 * @brief Disable PTP clock output
 *
 * @param[in]  dp83640  The dp83640 handle that created by 'esp_eth_phy_new_dp83640'
 * @return
 *      - ESP_OK: Disable PTP clock output successfully
 *      - ESP_FAIL: Write PHY register failed because some error occurred
 */
esp_err_t dp83640_ptp_disable_clock_output(dp83640_handle_t dp83640);

/**
 * @brief Configure the PTP interrupt to use any of the GPIO pins
 *
 * @param[in]  dp83640  The dp83640 handle that created by 'esp_eth_phy_new_dp83640'
 * @param[in]  int_gpio Set a non-zero GPIO to enable interrupts on this GPIO pin
 * @return
 *      - ESP_OK: Configure the PTP interrupt GPIO successfully
 *      - ESP_FAIL: Write PHY register failed because some error occurred
 */
esp_err_t dp83640_ptp_config_intr_gpio(dp83640_handle_t dp83640, uint16_t int_gpio);

/**
 * @brief Configure the PHY Status Frame
 *
 * @param[in]  dp83640  The dp83640 handle that created by 'esp_eth_phy_new_dp83640'
 * @param[in]  psf_cfg  PSF configuration
 * @return
 *      - ESP_OK: Configure the PHY Status Frame successfully
 *      - ESP_FAIL: Write PHY register failed because some error occurred
 */
esp_err_t dp83640_ptp_config_psf(dp83640_handle_t dp83640, const dp83640_psf_config_t * psf_cfg);

/**
 * @brief Specify the PHY Status Frame IP
 *
 * @param[in]  dp83640  The dp83640 handle that created by 'esp_eth_phy_new_dp83640'
 * @param[in]  ip_addr  IP address
 * @return
 *      - ESP_OK: Specify the PHY Status Frame IP successfully
 *      - ESP_FAIL: Write PHY register failed because some error occurred
 */
esp_err_t dp83640_ptp_specify_psf_ip(dp83640_handle_t dp83640, uint32_t ip_addr);

/**
 * @brief Set PTP Header data for the PHY Status Frame.
 *
 * @param[in]  dp83640  The dp83640 handle that created by 'esp_eth_phy_new_dp83640'
 * @param[in]  header   PTP header data
 * @return
 *      - ESP_OK: Set PTP Header data for the PHY Status Frame successfully
 *      - ESP_FAIL: Write PHY register failed because some error occurred
 */
esp_err_t dp83640_ptp_set_ptp_frame_header(dp83640_handle_t dp83640, const dp83640_ptp_frame_header_t *header);

#ifdef __cplusplus
}
#endif
