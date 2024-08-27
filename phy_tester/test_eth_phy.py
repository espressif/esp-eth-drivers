# SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: CC0-1.0

import argparse
import contextlib
import logging
import os
import re
import socket
import sys
import signal
from multiprocessing import Pipe, Process, connection
from time import sleep
from typing import Iterator

import pexpect
import pytest
from eth_error_msg import EthFailMsg
from pytest_embedded import Dut
from scapy.all import Ether, raw

ETH_TYPE = 0x3300
LINK_UP_TIMEOUT = 6

norm = '\033[0m'
red = '\033[31m'
green = '\033[32m'
italics = '\033[3m'
yellow = '\033[33m'
cyan = '\033[36m'


class EthTestIntf(object):
    def __init__(self, eth_type: int, my_if: str = ''):
        self.target_if = ''
        self.eth_type = eth_type
        self.find_target_if(my_if)

    def find_target_if(self, my_if: str = '') -> None:
        # try to determine which interface to use
        netifs = os.listdir('/sys/class/net/')
        logging.info('detected interfaces: %s', str(netifs))

        for netif in netifs:
            # if no interface defined, try to find it automatically
            if my_if == '':
                if netif.find('eth') == 0 or netif.find('enp') == 0 or netif.find('eno') == 0:
                    self.target_if = netif
                    break
            else:
                if netif.find(my_if) == 0:
                    self.target_if = my_if
                    break
        if self.target_if == '':
            raise RuntimeError('network interface not found')
        logging.info('Use %s for testing', self.target_if)

    @contextlib.contextmanager
    def configure_eth_if(self, eth_type:int=0) -> Iterator[socket.socket]:
        if eth_type == 0:
            eth_type = self.eth_type
        try:
            so = socket.socket(socket.AF_PACKET, socket.SOCK_RAW, socket.htons(eth_type))
        except PermissionError:
            EthFailMsg.print_yellow('\n\n=================================================================================')
            EthFailMsg.print_yellow('Insufficient permission to create raw socket')
            EthFailMsg.print_yellow('Either run as root or set appropriate capability to python by executing:')
            EthFailMsg.print_yellow("  sudo setcap 'CAP_NET_RAW+eip' $(readlink -f $(which python))")
            EthFailMsg.print_yellow('=================================================================================')
            raise RuntimeError('Insufficient permission to create raw socket')

        so.bind((self.target_if, 0))
        try:
            yield so
        finally:
            so.close()

    def get_mac_addr(self) -> str:
        with self.configure_eth_if() as so:
            mac_bytes = so.getsockname()[4]
            mac = ':'.join(format(x, '02x') for x in mac_bytes)
            return mac

    def send_eth_frame(self, mac: str, payload_len: int=1010) -> None:
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
        with self.configure_eth_if() as so:
            so.settimeout(5)
            payload = bytearray(payload_len)
            for i, _ in enumerate(payload):
                payload[i] = i & 0xff
            eth_frame = Ether(dst=mac, src=so.getsockname()[4], type=self.eth_type) / raw(payload)
            try:
                so.send(raw(eth_frame))
            except Exception as e:
                raise e

            try:
                rx_eth_frame = Ether(so.recv(1522))
            except Exception as e:
                logging.error('recv: %s', e)
                return False
            if rx_eth_frame.load != eth_frame.load:
                logging.error("doesn't match!")
                return False
        return True

    def traffic_gen(self, mac: str, count: int, payload_len: int, data_pattern_hex: str, interval: float, pipe_rcv:connection.Connection) -> None:
        with self.configure_eth_if() as so:
            payload = bytearray.fromhex(data_pattern_hex) * int((payload_len + 1) / len(bytearray.fromhex(data_pattern_hex)))
            i = 0
            cnt = 0
            try:
                while pipe_rcv.poll() is not True:
                    payload[0] = cnt
                    cnt += 1
                    if cnt > 255:
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
    ret = 'PASS'
    dut.write('farend-loop-en -e\n')
    try:
        dut.expect_exact('Link Up', timeout=LINK_UP_TIMEOUT)
    except pexpect.exceptions.TIMEOUT:
        # it could be caused by PHY does not support far-end loopback
        try:
            dut.expect_exact('far-end loopback is not supported by selected PHY', timeout=1)
        except:  # noqa
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
    ret = 'PASS'
    dut.write(f'loop-test -s 640 -c {count}\n')
    try:
        dut.expect_exact('Link Up')
        dut.expect_exact(f'looped frames: {count}, rx errors: 0', timeout=10)
    except:  # noqa
        logging.error('near-end loop back failed')
        ret = 'FAIL'
    return ret


def _test_dut_rx(dut: Dut, eth_if: EthTestIntf, mac: str) -> str:
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
    len = payload_len + 6 + 6 + 2  # + dest mac + src mac + ethtype
    try:
        dut.expect_exact(f'esp.emac: receive len= {len}', timeout=4)
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


def draw_result(near_tx_color: str, near_rx_color: str, far_tx_color: str, far_rx_color: str, phy_loop: bool, esp_loop: bool) -> None:
    rmii_rx = near_rx_color
    rmii_tx = near_tx_color
    if phy_loop is True:
        near_rx = f'{near_rx_color}-<-.{norm}'
        near_tx = f"{near_tx_color}->-'{norm}"
        near_loop = f'{near_tx_color}|{norm}'

        far_rx = f'{far_rx_color}.-<-{norm}'
        far_tx = f"{far_tx_color}'->-{norm}"
        far_loop = f'{far_tx_color}|{norm}'
        phy_rx_path = f'       '
        phy_tx_path = f'       '
    else:
        near_rx = f'{far_rx_color}----{norm}'
        near_tx = f'{near_tx_color}----{norm}'
        near_loop = f'{near_tx_color} {norm}'
        far_rx = f'{far_rx_color}<---{norm}'
        far_tx = f'{far_tx_color}>---{norm}'
        far_loop = f'{far_tx_color} {norm}'
        phy_rx_path = f'{far_rx_color}-------{norm}'
        phy_tx_path = f'{far_tx_color}-------{norm}'

    if esp_loop is True:
        esp_rx = f'{near_rx_color}.-<-{norm}'
        esp_tx = f"{near_tx_color}'->-{norm}"
        loop_esp = f'{near_tx_color}|{norm}'
    else:
        esp_rx = f'{near_rx_color}    {norm}'
        esp_tx = f'{near_tx_color}    {norm}'
        loop_esp = f'{near_tx_color} {norm}'

    if (far_tx_color == far_rx_color):
        pc_conn = far_rx_color
    else:
        pc_conn = norm

    print(f'. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .')
    print(f'. DUT                                                                                   .')
    print(f'.                          RMII                             Tr.                         .')
    print(f'.  +------------------+            +---------------+                  +-----------+     .     +---------------+')
    print(f'.  |              +---|            |               |        8|8       |           |     .     |               |')
    print(f'.  |          {esp_rx}| M |{rmii_rx}-----Rx-----{norm}|{near_rx}{phy_rx_path}{far_rx}|{far_rx_color}-------{norm} 8|8 {far_rx_color}------{norm}|           |     .     |               |')  # noqa
    print(f'.  |   ESP32  {loop_esp}   | A |            |   {near_loop}  PHY  {far_loop}   |                  |    RJ45   |{pc_conn}==========={norm}|    Test PC    |')  # noqa
    print(f'.  |          {esp_tx}| C |{rmii_tx}-----Tx-----{norm}|{near_tx}{phy_tx_path}{far_tx}|{far_tx_color}-------{norm} 8|8 {far_tx_color}------{norm}|           |     .     |               |')  # noqa
    print(f'.  |              +---|            |               |        8|8       |           |     .     |               |')
    print(f'.  +------------------+            +---------------+                  +-----------+     .     +---------------+')
    print(f'.                                                                                       .')
    if phy_loop is True:
        print(f'.                    {italics}(near-end loopback)         (far-end loopback){norm}                     .')
    print(f'. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .')


def ethernet_phy_test(dut: Dut, test_pc_nic: str) -> None:
    target_if = EthTestIntf(ETH_TYPE, test_pc_nic)
    try:
        dut.expect_exact('Ethernet init successful!')
    except pexpect.exceptions.TIMEOUT:
        EthFailMsg.print_eth_init_fail_msg()
        raise RuntimeError()

    dut.expect_exact('Steps to Test Ethernet PHY')

    res_dict = {'loopback server': _test_loopback_server(dut, target_if, 'ff:ff:ff:ff:ff:ff')}
    # Loopback server results
    if res_dict['loopback server'] == 'PASS':
        tx_path = green
        rx_path = green
    else:
        tx_path = red
        rx_path = red
    print('\n\n')
    print(f'loopback server: {tx_path}{res_dict["loopback server"]}{norm}')
    draw_result(tx_path, rx_path, tx_path, rx_path, False, True)

    if True is True: #res_dict['loopback server'] == 'FAIL':
        logging.error('loopback server test failed!')
        logging.info('Running additional tests to try to isolate the problem...')

        res_dict.update({'DUT tx': _test_dut_tx(dut, target_if, 1)})
        res_dict.update({'DUT rx': _test_dut_rx(dut, target_if, 'ff:ff:ff:ff:ff:ff')})

        res_dict.update({'far-end loopback': _test_farend_loopback(dut, target_if, 'ff:ff:ff:ff:ff:ff')})
        res_dict.update({'near-end loopback': _test_nearend_loopback(dut, 5)})

        # DUT Tx/Rx results
        if res_dict['DUT tx'] == 'PASS':
            tx_path = green
        else:
            tx_path = red

        if res_dict['DUT rx'] == 'PASS':
            rx_path = green
        else:
            rx_path = red

        print('\n\n')
        print(f'DUT tx: {tx_path}{res_dict["DUT tx"]}{norm}')
        print(f'DUT rx: {rx_path}{res_dict["DUT rx"]}{norm}')
        draw_result(tx_path, rx_path, tx_path, rx_path, False, False)

        # PHY loopback results
        if res_dict['near-end loopback'] == 'PASS':
            near_loop = green
        else:
            near_loop = red

        if res_dict['far-end loopback'] == 'PASS':
            far_loop = green
        elif res_dict['far-end loopback'] == 'NOT SUPPORTED':
            far_loop = yellow
        else:
            far_loop = red

        print('\n\n')
        print(f'Near-end loopback: {near_loop}{res_dict["near-end loopback"]}{norm}')
        print(f'Far-end loopback: {far_loop}{res_dict["far-end loopback"]}{norm}')
        draw_result(near_loop, near_loop, far_loop, far_loop, True, False)

        # Final isolation
        rx_near = green
        tx_near = green
        rmii_rx_fail = False
        rmii_tx_fail = False
        if near_loop != green:
            if near_loop == red:
                if rx_path == red:
                    rx_near = red
                    rmii_rx_fail = True
                if tx_path == red:
                    tx_near = red
                    rmii_tx_fail = True
            else:
                rx_near = near_loop
                tx_near = near_loop

        tx_far = green
        rx_far = green
        rj45_rx_fail = False
        rj45_tx_fail = False
        if far_loop != green:
            if far_loop == red:
                if rx_path == red:
                    rx_far = red
                    rj45_rx_fail = True
                if tx_path == red:
                    tx_far = red
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

        print(f'\n{cyan}The test finished! `Final problem isolation` should show you the most probable location of issue. However, go over the full log to see '
              f'additional details and to fully understand each tested scenario.{norm}')

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

        pipe_rcv, pipe_send = Pipe(False)
        tx_proc = Process(target=target_if.traffic_gen, args=(args.mac, args.count, args.size, args.pattern, args.interval, pipe_rcv, ))
        tx_proc.start()
        # pipe_send.send(0)  # just send some dummy data to stop the process
        tx_proc.join()
        if tx_proc.exitcode is None:
            tx_proc.terminate()

