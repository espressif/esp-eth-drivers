# SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0

norm = '\033[0m'
red = '\033[31m'
green = '\033[32m'
italics = '\033[3m'
yellow = '\033[33m'


class EthFailMsg(object):
    @classmethod
    def print_yellow(cls, msg: str) -> None:
        print(yellow + msg + norm)

    @classmethod
    def print_eth_init_fail_msg(cls) -> None:
        cls.print_yellow('=================================================================================')
        cls.print_yellow(f'Ethernet initialization failed!')
        cls.print_yellow(f'------------------------------')
        cls.print_yellow(f'Check the above DUT log and investigate possible root causes:')
        cls.print_yellow(f'1) If EMAC errors:')
        cls.print_yellow(f'   a) RMII REF CLK mode incorrectly configured if emac initialization timeouts.')
        cls.print_yellow(f'        Does ESP32 outputs the RMII CLK? Or is RMII CLK provided externally by PHY or oscillator?')
        cls.print_yellow(f'   b) If external RMII CLK is used, measure the clock signal at ESP32 REF RMII CLK input pin using oscilloscope with sufficient '
                         'bandwidth. There must be 50 MHz square wave signal.')
        # TODO make it target dependent
        cls.print_yellow(f'   c) Make sure programmer/monitor device correctly handles “nDTR”/”nRST” and associated transistors which are connected to GPIO0.')
        cls.print_yellow(f'2) If PHY errors:')
        cls.print_yellow(f'   a) Check if bootstrap (PHY address) is set correctly or set to "auto" in the code (menuconfig).')
        cls.print_yellow(f'   b) Cross-check with schematics if MDIO and MDC GPIOs are correctly configured.')
        cls.print_yellow('=================================================================================')

    @classmethod
    def print_link_up_fail_msg(cls) -> None:
        cls.print_yellow('=================================================================================')
        cls.print_yellow(f'Link failed to up!')
        cls.print_yellow(f'------------------')
        cls.print_yellow(f'Possible root causes:')
        cls.print_yellow(f'1) DUT not connected. Check if the Ethernet cable from your test PC is properly connected to DUT.')
        cls.print_yellow(f'2) PHY is not able to negotiate a link:')
        cls.print_yellow(f'   a) incorrect configuration of test PC NIC')
        cls.print_yellow(f'   b) HW problem in DUT at path between the PHY and RJ45')
        cls.print_yellow('=================================================================================\n')

    @classmethod
    def print_link_stability_fail_msg(cls) -> None:
        cls.print_yellow('=================================================================================')
        cls.print_yellow(f'Ethernet link is not stable!')
        cls.print_yellow(f'---------------------------')
        cls.print_yellow('=================================================================================')
        # TODO crate a test
        # - Something connected to GPIO0 (like a programmer via wires) can cause signal integrity issues. Link oscillate UP/Down.
        # - if ESP32 is configured as source of RMII REF CLK and Wi-Fi is used at the same time TBD!!!

    @classmethod
    def print_rmii_fail_msg(cls, is_rx: bool, is_tx: bool) -> None:
        cls.print_yellow('=================================================================================')
        cls.print_yellow(f'The RMII data pane non functional!')
        cls.print_yellow(f'----------------------------------')
        receive = ''
        rx_gpio = ''
        transmit = ''
        tx_gpio = ''
        if is_rx:
            receive = 'receive'
            rx_gpio = 'RXD[0,1]'

        if is_tx:
            transmit = 'transmit'
            tx_gpio = 'TXD[0,1]'

        _and = ''
        if is_tx and is_rx:
            _and = ' and '

        cls.print_yellow(f'The DUT is not able to {receive}{_and}{transmit} Ethernet frames. The issue needs to be probed on the board.')
        cls.print_yellow(f'* Identify location of {rx_gpio}{_and}{tx_gpio} pins on your board at ESP32 side and at PHY side.')
        cls.print_yellow(f'* Go over all identified pins. Connect the first oscilloscope probe at the ESP32 pin side and the other probe at PHY side (expect '
                         'signal with frequency of 25 MHz when in 100mbps mode).')
        if is_tx:
            cls.print_yellow(f'\n-- Transmit path steps: --')
            cls.print_yellow(f'  * Flash `phy_tester` tool to DUT and connect to it using `monitor`')
            cls.print_yellow(f'  * Run `dummy-tx` command in DUT console. Appropriately configure the interval and number of transmission so you have enough '
                             'time to perform measurement.')
            cls.print_yellow(f'  * The DUT now transmits frames and you should see the same signal at both ESP32 and PHY sides. If you do not see it at PHY '
                             'side, the trace is corrupted. It could be missing or having incorrect value of series termination resistor or incorrect '
                             'tracing.')
            cls.print_yellow(f'  * If signal at {tx_gpio} is OK, check TX_EN signal path.')

        if is_rx:
            cls.print_yellow(f'\n-- Receive path steps: --')
            cls.print_yellow(f'  * Run `python pytest_eth_phy.py --eth_nic=NIC_NAME dummy-tx` command in test PC console. Appropriately configure the interval '
                             'and number of transmission so you have enough time to perform measurement.')
            cls.print_yellow(f'  * The Test PC now transmits frames and you should see the same signal at both PHY and ESP32 sides. If you do not see it at '
                             'ESP32 side, the trace is corrupted. It could be missing or having incorrect value of series termination resistor or incorrect '
                             'tracing.')

        if is_rx and is_tx:
            cls.print_yellow(f'Since both receive and transmit is not possible, check also CRS_DV signal path if data signals look good.')

        cls.print_yellow('=================================================================================')

    @classmethod
    def print_rj45_fail_msg(cls, is_rx: bool, is_tx: bool) -> None:
        cls.print_yellow('=================================================================================')
        cls.print_yellow(f'Path between PHY and RJ45 non functional!')
        cls.print_yellow(f'----------------------------------------')
        cls.print_yellow(f'* Double check the design checklist if you used correct components and the design is correct.')
        cls.print_yellow(f'* Check the trace is not corrupted.')
        cls.print_yellow(f'* Check the components are correctly soldered and correct Part Numbers (PNs) are mounted.')
        cls.print_yellow('=================================================================================')
