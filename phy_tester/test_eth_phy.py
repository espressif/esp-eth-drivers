# SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

import argparse
import contextlib
import logging
import os
import re
import signal
import socket
import sys

from collections.abc import Iterator
from multiprocessing import Pipe
from multiprocessing import Process
from multiprocessing import connection
from time import sleep

import pexpect
import pytest

from eth_error_msg import EthFailMsg
from pytest_embedded import Dut
from scapy.all import Ether
from scapy.all import raw

ETH_TYPE = 0x3300
LINK_UP_TIMEOUT = 6

UINT8_MAX = 255

NORM = '\033[0m'
RED = '\033[31m'
GREEN = '\033[32m'
ITALICS = '\033[3m'
YELLOW = '\033[33m'
CYAN = '\033[36m'


class EthTestIntf:
    """
    Ethernet test interface.
    """
    def __init__(self, eth_type: int, my_if: str = ''):
        """
        Initialize the Ethernet test interface.

        Args:
            eth_type: Ethertype in hex
            my_if: Name of the test PC Ethernet NIC connected to DUT (automatically detected if empty)
        """
        self.target_if = ''
        self.eth_type = eth_type
        self.find_target_if(my_if)

    def find_target_if(self, my_if: str = '') -> None:
        """
        Find the target Ethernet interface.

        Args:
            my_if: Name of the test PC Ethernet NIC connected to DUT (automatically detected if empty)
        """
        # try to determine which interface to use
        netifs = os.listdir('/sys/class/net/')
        logging.info('detected interfaces: %s', str(netifs))

        for netif in netifs:
            # if no interface defined, try to find it automatically
            if my_if == '':
                if netif.find('eth') == 0 or netif.find('enp') == 0 or netif.find('eno') == 0:
                    self.target_if = netif
                    break
            elif netif.find(my_if) == 0:
                self.target_if = my_if
                break
        if self.target_if == '':
            raise RuntimeError('network interface not found')
        logging.info('Use %s for testing', self.target_if)

    @contextlib.contextmanager
    def configure_eth_if(self, eth_type:int=0) -> Iterator[socket.socket]:
        """
        Configure Ethertype and bind raw socket to the target Ethernet interface.

        Args:
            eth_type: Ethertype in hex (use Ethertype defined in constructor if 0)
        """
        if eth_type == 0:
            eth_type = self.eth_type
        try:
            so = socket.socket(socket.AF_PACKET, socket.SOCK_RAW, socket.htons(eth_type))
        except PermissionError as e:
            EthFailMsg.print_yellow('\n\n=================================================================================')
            EthFailMsg.print_yellow('Insufficient permission to create raw socket')
            EthFailMsg.print_yellow('Either run as root or set appropriate capability to python by executing:')
            EthFailMsg.print_yellow("  sudo setcap 'CAP_NET_RAW+eip' $(readlink -f $(which python))")
            EthFailMsg.print_yellow('=================================================================================')
            raise RuntimeError('Insufficient permission to create raw socket') from e

        so.bind((self.target_if, 0))
        try:
            yield so
        finally:
            so.close()

    def get_mac_addr(self) -> str:
        """
        Get the MAC address of the target Ethernet interface.
        """
        with self.configure_eth_if() as so:
            mac_bytes = so.getsockname()[4]
            mac = ':'.join(format(x, '02x') for x in mac_bytes)
            return mac

    def send_eth_frame(self, mac: str, payload_len: int=1010) -> None:
        """
        Send an Ethernet frame.

        Args:
            mac: MAC address of the destination
            payload_len: Length of the payload
        """
        with self.configure_eth_if() as so:
            so.settimeout(10)
            payload = bytearray(payload_len)
            for i, _ in enumerate(payload):
                payload[i] = i & 0xff
            eth_frame = Ether(dst=mac, src=so.getsockname()[4], type=self.eth_type) / raw(payload)
            try:
                so.send(raw(eth_frame))
            except Exception as e:
                raise e

    def send_recv_cmp(self, mac: str, payload_len: int=1010) -> bool:
        """
        Send and receive an Ethernet frame and compare the payload.

        Args:
            mac: MAC address of the destination
            payload_len: Length of the payload

        Note:
            Typically used in DUT loopback tests.
        """
        with self.configure_eth_if() as so:
            so.settimeout(5)
            payload = bytearray(payload_len)
            for i, _ in enumerate(payload):
                payload[i] = i & 0xff
            eth_frame = Ether(dst=mac, src=so.getsockname()[4], type=self.eth_type) / raw(payload)
            try:
                so.send(raw(eth_frame))
            except Exception as e:  # pylint: disable=broad-except  # noqa
                raise e

            try:
                rx_eth_frame = Ether(so.recv(1522))
            except Exception as e:  # pylint: disable=broad-except  # noqa
                logging.error('recv: %s', e)
                return False
            if rx_eth_frame.load != eth_frame.load:
                logging.error("doesn't match!")
                return False
        return True

    def traffic_gen(
        self,
        pipe_rcv: connection.Connection,
        *,  # force keyword arguments
        mac: str = 'ff:ff:ff:ff:ff:ff',
        count: int = -1,
        payload_len: int = 1010,
        data_pattern_hex: str = 'ff00',
        interval: float = 0.5
    ) -> None:
        """
        Generate traffic on the target Ethernet interface.

        Args:
            pipe_rcv: Pipe connection to receive the stop signal
            mac: MAC address of the destination (default: ff:ff:ff:ff:ff:ff)
            count: Number of frames to send (default: -1 for infinite)
            payload_len: Length of the payload (default: 1010)
            data_pattern_hex: Data pattern in hex (default: ff00)
            interval: Interval between frames in seconds (default: 0.5)
        """
        with self.configure_eth_if() as so:
            payload = bytearray.fromhex(data_pattern_hex) * int((payload_len + 1) / len(bytearray.fromhex(data_pattern_hex)))
            i = 0
            cnt = 0
            try:
                while pipe_rcv.poll() is not True:
                    payload[0] = cnt
                    cnt += 1
                    if cnt > UINT8_MAX:
                        cnt = 0
                    eth_frame = Ether(dst=mac, src=so.getsockname()[4], type=self.eth_type) / raw(payload[0:payload_len])
                    so.send(raw(eth_frame))
                    sleep(interval)
                    if count != -1:
                        i += 1
                        if i >= count:
                            break
            except Exception as e:
                raise e


def _test_loopback_server(dut: Dut, eth_if: EthTestIntf, mac: str) -> str:
    """
    Test DUT loopback server. It tests the whole path from the test PC to DUT and back (PC-PHY-DUT_MAC-PHY-PC).

    Args:
        dut: DUT object
        eth_if: Ethernet test interface
        mac: MAC address of the destination

    Returns:
        'PASS' if the whole path is working, 'FAIL' otherwise
    """
    ret = 'PASS'
    eth_type_hex = hex(eth_if.eth_type)
    dut.write(f'loop-server -f {eth_type_hex} -t 2000\n')
    try:
        dut.expect_exact('Link Up', timeout=LINK_UP_TIMEOUT)
    except pexpect.exceptions.TIMEOUT:
        EthFailMsg.print_link_up_fail_msg()
        return 'FAIL'

    if eth_if.send_recv_cmp(mac) is False:
        ret = 'FAIL'

    dut.expect_exact('Link Down')
    return ret


def _test_farend_loopback(dut: Dut, eth_if: EthTestIntf, mac: str) -> str:
    """
    Test DUT PHY far-end loopback. It tests the path from the test PC to DUT and back (PC-PHY-PC).

    Args:
        dut: DUT object
        eth_if: Ethernet test interface
        mac: MAC address of the destination

    Returns:
        'PASS' if the PHY far-end loopback is working, 'FAIL' when it is not working, 'NOT SUPPORTED' when the PHY does not support far-end loopback
    """
    ret = 'PASS'
    dut.write('farend-loop-en -e\n')
    try:
        dut.expect_exact('Link Up', timeout=LINK_UP_TIMEOUT)
    except pexpect.exceptions.TIMEOUT:
        # it could be caused by PHY does not support far-end loopback
        try:
            dut.expect_exact('far-end loopback is not supported by selected PHY', timeout=1)
        except:  # pylint: disable=bare-except  # noqa
            pass  # far-end loopback is supported, do nothing
        else:
            logging.warning('DUT PHY does not support far-end loopback!')
            return 'NOT SUPPORTED'
        EthFailMsg.print_link_up_fail_msg()
        dut.write('farend-loop-en -d\n')
        return 'FAIL'

    if eth_if.send_recv_cmp(mac) is False:
        ret = 'FAIL'

    dut.write('farend-loop-en -d\n')
    dut.expect_exact('Link Down')
    return ret


def _test_nearend_loopback(dut: Dut, count: int) -> str:
    """
    Starts and evaluates DUT PHY near-end loopback (DUT_MAC-PHY-DUT_MAC).

    Args:
        dut: DUT object
        count: Number of frames to send

    Returns:
        'PASS' if the PHY near-end loopback is working, 'FAIL' when it is not working
    """
    ret = 'PASS'
    dut.write(f'loop-test -s 640 -c {count}\n')
    try:
        dut.expect_exact('Link Up')
        dut.expect_exact(f'looped frames: {count}, rx errors: 0', timeout=10)
    except:  # pylint: disable=bare-except  # noqa
        logging.error('near-end loop back failed')
        ret = 'FAIL'
    return ret


def _test_dut_rx(dut: Dut, eth_if: EthTestIntf, mac: str) -> str:
    """
    Test DUT receive path. It tests the path from the test PC to DUT (PC-PHY-DUT_MAC).

    Args:
        dut: DUT object
        eth_if: Ethernet test interface
        mac: MAC address of the destination

    Returns:
        'PASS' if the receive path is working, 'FAIL' otherwise
    """
    ret = 'PASS'
    eth_type_hex = hex(eth_if.eth_type)
    dut.write(f'loop-server -v -f {eth_type_hex} -t 2000\n')
    try:
        dut.expect_exact('Link Up', timeout=LINK_UP_TIMEOUT)
    except pexpect.exceptions.TIMEOUT:
        EthFailMsg.print_link_up_fail_msg()
        return 'FAIL'

    payload_len = 50
    eth_if.send_eth_frame(mac, payload_len)
    length = payload_len + 6 + 6 + 2  # + dest mac + src mac + ethtype
    try:
        dut.expect_exact(f'esp.emac: receive length= {length}', timeout=4)
        rx_frame = dut.expect(r'([0-9A-Fa-f]{2} [0-9A-Fa-f]{2} [0-9A-Fa-f]{2} [0-9A-Fa-f]{2} [0-9A-Fa-f]{2} [0-9A-Fa-f]{2}) ' +
                              r'([0-9A-Fa-f]{2} [0-9A-Fa-f]{2}  [0-9A-Fa-f]{2} [0-9A-Fa-f]{2} [0-9A-Fa-f]{2} [0-9A-Fa-f]{2}) ' +
                              r'([0-9A-Fa-f]{2} [0-9A-Fa-f]{2})', timeout=4)
    except pexpect.exceptions.TIMEOUT:
        logging.error('expected frame was not received by DUT')
        return 'FAIL'

    rx_dst_mac = rx_frame.group(1).decode('utf-8').replace(' ', ':')
    rx_src_mac = rx_frame.group(2).decode('utf-8').replace('  ', ' ').replace(' ', ':')
    rx_eth_type = rx_frame.group(3).decode('utf-8').replace(' ','')

    mac.lower()
    host_mac = eth_if.get_mac_addr()
    if rx_dst_mac != mac or rx_src_mac != host_mac or rx_eth_type not in eth_type_hex:
        ret = 'FAIL'

    dut.expect_exact('Link Down')
    return ret


def _test_dut_tx(dut: Dut, eth_if: EthTestIntf, count: int) -> str:
    """
    Test DUT transmit path. It tests the path from DUT to PC(DUT_MAC-PHY-PC).

    Args:
        dut: DUT object
        eth_if: Ethernet test interface
        count: Number of frames to send by DUT

    Returns:
        'PASS' if the transmit path is working, 'FAIL' otherwise
    """
    ret = 'PASS'
    with eth_if.configure_eth_if() as so:
        so.settimeout(5)
        dut.write(f'dummy-tx -s 256 -c {count}\n')
        try:
            dut.expect_exact('Link Up', timeout=LINK_UP_TIMEOUT)
        except pexpect.exceptions.TIMEOUT:
            EthFailMsg.print_link_up_fail_msg()
            return 'FAIL'

        try:
            rx_eth_frame = Ether(so.recv(1522))
            decoded_rx_msg = rx_eth_frame.load[1:].decode('utf-8')  # the first byte is seq. number
            if 'ESP32 HELLO' not in decoded_rx_msg:
                logging.error('recv. frame does not contain expected string')
                ret = 'FAIL'
        except Exception as e:
            logging.error('recv: %s', e)
            ret = 'FAIL'

    dut.expect_exact('Link Down')
    return ret


def draw_result(near_tx_color: str,
                near_rx_color: str,
                far_tx_color: str,
                far_rx_color: str,
                phy_loop: bool,
                esp_loop: bool) -> None:
    """
    Draw the result of the test.

    Args:
        near_tx_color: Color of the near-end transmit path
        near_rx_color: Color of the near-end receive path
        far_tx_color: Color of the far-end transmit path
        far_rx_color: Color of the far-end receive path
        phy_loop: True if to draw PHY loopbacks (near-end and far-end are enabled), False when loopbacks are disabled
        esp_loop: True if DUT loopback server is enabled, False otherwise
    """
    rmii_rx = near_rx_color
    rmii_tx = near_tx_color
    if phy_loop is True:
        near_rx = f'{near_rx_color}-<-.{NORM}'
        near_tx = f"{near_tx_color}->-'{NORM}"
        near_loop = f'{near_tx_color}|{NORM}'

        far_rx = f'{far_rx_color}.-<-{NORM}'
        far_tx = f"{far_tx_color}'->-{NORM}"
        far_loop = f'{far_tx_color}|{NORM}'
        phy_rx_path = '       '
        phy_tx_path = '       '
    else:
        near_rx = f'{far_rx_color}----{NORM}'
        near_tx = f'{near_tx_color}----{NORM}'
        near_loop = f'{near_tx_color} {NORM}'
        far_rx = f'{far_rx_color}<---{NORM}'
        far_tx = f'{far_tx_color}>---{NORM}'
        far_loop = f'{far_tx_color} {NORM}'
        phy_rx_path = f'{far_rx_color}-------{NORM}'
        phy_tx_path = f'{far_tx_color}-------{NORM}'

    if esp_loop is True:
        esp_rx = f'{near_rx_color}.-<-{NORM}'
        esp_tx = f"{near_tx_color}'->-{NORM}"
        loop_esp = f'{near_tx_color}|{NORM}'
    else:
        esp_rx = f'{near_rx_color}    {NORM}'
        esp_tx = f'{near_tx_color}    {NORM}'
        loop_esp = f'{near_tx_color} {NORM}'

    if far_tx_color == far_rx_color:
        pc_conn = far_rx_color
    else:
        pc_conn = NORM

    print('. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .')
    print('. DUT                                                                                   .')
    print('.                          RMII                             Tr.                         .')
    print('.  +------------------+            +---------------+                  +-----------+     .     +---------------+')
    print('.  |              +---|            |               |        8|8       |           |     .     |               |')
    print(f'.  |          {esp_rx}| M |{rmii_rx}-----Rx-----{NORM}|{near_rx}{phy_rx_path}{far_rx}|{far_rx_color}-------{NORM} 8|8 {far_rx_color}------{NORM}|           |     .     |               |')  # pylint: disable=line-too-long  # noqa
    print(f'.  |   ESP32  {loop_esp}   | A |            |   {near_loop}  PHY  {far_loop}   |                  |    RJ45   |{pc_conn}==========={NORM}|    Test PC    |')  # pylint: disable=line-too-long  # noqa
    print(f'.  |          {esp_tx}| C |{rmii_tx}-----Tx-----{NORM}|{near_tx}{phy_tx_path}{far_tx}|{far_tx_color}-------{NORM} 8|8 {far_tx_color}------{NORM}|           |     .     |               |')  # pylint: disable=line-too-long  # noqa
    print('.  |              +---|            |               |        8|8       |           |     .     |               |')
    print('.  +------------------+            +---------------+                  +-----------+     .     +---------------+')
    print('.                                                                                       .')
    if phy_loop is True:
        print(f'.                    {ITALICS}(near-end loopback)         (far-end loopback){NORM}                     .')
    print('. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .')


def ethernet_phy_test(dut: Dut, test_pc_nic: str) -> None:
    """
    Runs the Ethernet PHY tests.

    Args:
        dut: DUT object
        test_pc_nic: Name of the test PC Ethernet NIC connected to DUT
    """
    target_if = EthTestIntf(ETH_TYPE, test_pc_nic)
    try:
        dut.expect_exact('Ethernet init successful!')
    except pexpect.exceptions.TIMEOUT as e:
        EthFailMsg.print_eth_init_fail_msg()
        raise RuntimeError() from e

    dut.expect_exact('Steps to Test Ethernet PHY')

    res_dict = {'loopback server': _test_loopback_server(dut, target_if, 'ff:ff:ff:ff:ff:ff')}
    # Loopback server results
    if res_dict['loopback server'] == 'PASS':
        tx_path = GREEN
        rx_path = GREEN
    else:
        tx_path = RED
        rx_path = RED
    print('\n\n')
    print(f'loopback server: {tx_path}{res_dict["loopback server"]}{NORM}')
    draw_result(tx_path, rx_path, tx_path, rx_path, False, True)

    if res_dict['loopback server'] == 'FAIL':
        logging.error('loopback server test failed!')
        logging.info('Running additional tests to try to isolate the problem...')

        res_dict.update({'DUT tx': _test_dut_tx(dut, target_if, 1)})
        res_dict.update({'DUT rx': _test_dut_rx(dut, target_if, 'ff:ff:ff:ff:ff:ff')})

        res_dict.update({'far-end loopback': _test_farend_loopback(dut, target_if, 'ff:ff:ff:ff:ff:ff')})
        res_dict.update({'near-end loopback': _test_nearend_loopback(dut, 5)})

        # DUT Tx/Rx results
        if res_dict['DUT tx'] == 'PASS':
            tx_path = GREEN
        else:
            tx_path = RED

        if res_dict['DUT rx'] == 'PASS':
            rx_path = GREEN
        else:
            rx_path = RED

        print('\n\n')
        print(f'DUT tx: {tx_path}{res_dict["DUT tx"]}{NORM}')
        print(f'DUT rx: {rx_path}{res_dict["DUT rx"]}{NORM}')
        draw_result(tx_path, rx_path, tx_path, rx_path, False, False)

        # PHY loopback results
        if res_dict['near-end loopback'] == 'PASS':
            near_loop = GREEN
        else:
            near_loop = RED

        if res_dict['far-end loopback'] == 'PASS':
            far_loop = GREEN
        elif res_dict['far-end loopback'] == 'NOT SUPPORTED':
            far_loop = YELLOW
        else:
            far_loop = RED

        print('\n\n')
        print(f'Near-end loopback: {near_loop}{res_dict["near-end loopback"]}{NORM}')
        print(f'Far-end loopback: {far_loop}{res_dict["far-end loopback"]}{NORM}')
        draw_result(near_loop, near_loop, far_loop, far_loop, True, False)

        # Final isolation
        rx_near = GREEN
        tx_near = GREEN
        rmii_rx_fail = False
        rmii_tx_fail = False
        if near_loop != GREEN:
            if near_loop == RED:
                if rx_path == RED:
                    rx_near = RED
                    rmii_rx_fail = True
                if tx_path == RED:
                    tx_near = RED
                    rmii_tx_fail = True
            else:
                rx_near = near_loop
                tx_near = near_loop

        tx_far = GREEN
        rx_far = GREEN
        rj45_rx_fail = False
        rj45_tx_fail = False
        if far_loop != GREEN:
            if far_loop == RED:
                if rx_path == RED:
                    rx_far = RED
                    rj45_rx_fail = True
                if tx_path == RED:
                    tx_far = RED
                    rj45_tx_fail = True
            else:
                tx_far = far_loop
                rx_far = far_loop

        print('\n\n')
        print('==============================')
        print('Final problem isolation')
        print('==============================')
        draw_result(tx_near, rx_near, tx_far, rx_far, True, False)
        if rmii_rx_fail or rmii_tx_fail:
            EthFailMsg.print_rmii_fail_msg(rmii_rx_fail, rmii_tx_fail)
        if rj45_rx_fail or rj45_tx_fail:
            EthFailMsg.print_rj45_fail_msg(rj45_rx_fail, rj45_tx_fail)

        print(f'\n{CYAN}The test finished! `Final problem isolation` should show you the most probable location of issue. However, go over the full log to see '
              f'additional details and to fully understand each tested scenario.{NORM}')

    print('\nScript run:')


@pytest.mark.parametrize('target', [
    'esp32',
    'esp32p4',
], indirect=True)
def test_esp_ethernet(dut: Dut,
                      eth_nic: str) -> None:
    print(eth_nic)
    ethernet_phy_test(dut, eth_nic)


if __name__ == '__main__':
    signal.signal(signal.SIGINT, lambda s, f : sys.exit())

    logging.basicConfig(format='%(asctime)s %(message)s', level=logging.INFO)

    # Add arguments
    parser = argparse.ArgumentParser(description='Ethernet PHY tester helper script')
    parser.add_argument('--eth_nic', default='',
                        help='Name of the test PC Ethernet NIC connected to DUT. If the option '
                             'is omitted, script automatically uses the first identified Ethernet interface.')

    subparsers = parser.add_subparsers(help='Commands', dest='command')

    tx_parser = subparsers.add_parser('dummy-tx',
                                      help='Generates dummy Ethernet transmissions')

    tx_parser.add_argument('-i', type=float, default=0.5, dest='interval',
                           help='seconds between sending each frame')
    tx_parser.add_argument('-c', type=int, default=-1, dest='count',
                           help='number of Ethernet frames to send')
    tx_parser.add_argument('-t', type=str, default=hex(ETH_TYPE), dest='eth_type',
                           help='Ethertype in hex')
    tx_parser.add_argument('-s', type=int, default=512, dest='size',
                           help='payload size in B')
    tx_parser.add_argument('-a', type=str, default='ff:ff:ff:ff:ff:ff', dest='mac',
                           help='destination MAC address')
    tx_parser.add_argument('-p', type=str, default='ff 00', dest='pattern',
                           help='payload pattern. For example if you define "ff 00", the pattern will be repeated to the end of frame size.')

    args = parser.parse_args()

    target_if = EthTestIntf(ETH_TYPE, args.eth_nic)
    if args.command == 'dummy-tx':
        if args.mac:
            if re.fullmatch('(?:[0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}', args.mac) is None:
                logging.error('MAC is not in expected format')
                sys.exit()

        pipe_recv, pipe_send = Pipe(False)
        tx_proc = Process(target=target_if.traffic_gen, args=(pipe_recv, ))
        tx_proc.start()
        # pipe_send.send(0)  # just send some dummy data to stop the process
        tx_proc.join()
        if tx_proc.exitcode is None:
            tx_proc.terminate()
