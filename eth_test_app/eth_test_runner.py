# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""
Pytest runner for eth_test_app Unity tests.

This module is intended to be used from driver-specific test apps (e.g. dm9051/test_apps,
w5500/test_apps). The component may be loaded from:
  - managed_components/espressif__eth_test_app/ (when using the component from the registry)
  - override_path (e.g. ../../eth_test_app in a monorepo)

Usage in a driver test (e.g. dm9051/test_apps/pytest_dm9051.py):

  1. Ensure the component directory is on sys.path. In your test app's conftest.py:
       import sys
       from pathlib import Path
       project_root = Path(__file__).resolve().parent
       for name in ('espressif__eth_test_app', 'eth_test_app'):
           comp = project_root / 'managed_components' / name
           if comp.exists():
               sys.path.insert(0, str(comp))
               break
       else:
           # override_path: component at repo level
           comp = project_root.parent.parent / 'eth_test_app'
           if comp.exists():
               sys.path.insert(0, str(comp))

  2. In your test file:
       from eth_test_runner import EthTestRunner

       @pytest.mark.parametrize('config', ['dm9051'], indirect=True)
       @idf_parametrize('target', ['esp32'], indirect=['target'])
       def test_eth_dm9051(dut):
           runner = EthTestRunner()
           runner.run_ethernet_test(dut)
           dut.serial.hard_reset()
           runner.run_ethernet_l2_test(dut)
           dut.serial.hard_reset()
           runner.run_ethernet_heap_alloc_test(dut)
"""
from __future__ import annotations

import contextlib
import logging
import os
import socket

from collections.abc import Iterator
from multiprocessing import Pipe
from multiprocessing import Process
from multiprocessing import connection
from pathlib import Path

from scapy.all import Ether
from scapy.all import raw

# Ethernet type used for test data frames; control frames use ETH_TYPE + 1
ETH_TYPE = 0x3300

POKE_REQ = 0xFA
POKE_RESP = 0xFB
DUMMY_TRAFFIC = 0xFF


def get_component_dir() -> Path:
    """Return the directory where this module lives (component root).
    Useful for tests that need to resolve paths relative to the component.
    """
    return Path(__file__).resolve().parent


class EthTestIntf:
    """Host-side Ethernet interface helper for sending/receiving test frames."""

    def __init__(self, eth_type: int = ETH_TYPE, my_if: str = '') -> None:
        self.target_if = ''
        self.eth_type = eth_type
        self.find_target_if(my_if)

    def find_target_if(self, my_if: str = '') -> None:
        netifs = os.listdir('/sys/class/net/')
        netifs.sort(reverse=True)
        logging.info('detected interfaces: %s', str(netifs))

        if my_if == '':
            if 'dut_p1' in netifs:
                self.target_if = 'dut_p1'
            else:
                for netif in netifs:
                    if netif.find('eth') == 0 or netif.find('enp') == 0 or netif.find('eno') == 0:
                        self.target_if = netif
                        break
        elif my_if in netifs:
            self.target_if = my_if

        if self.target_if == '':
            raise RuntimeError('network interface not found')
        logging.info('Use %s for testing', self.target_if)

    @contextlib.contextmanager
    def configure_eth_if(self, eth_type: int = 0) -> Iterator[socket.socket]:
        if eth_type == 0:
            eth_type = self.eth_type
        so = socket.socket(socket.AF_PACKET, socket.SOCK_RAW, socket.htons(eth_type))
        so.bind((self.target_if, 0))
        try:
            yield so
        finally:
            so.close()

    def send_eth_packet(self, mac: str) -> None:
        with self.configure_eth_if() as so:
            so.settimeout(10)
            payload = bytearray(1010)
            for i, _ in enumerate(payload):
                payload[i] = i & 0xFF
            eth_frame = Ether(dst=mac, src=so.getsockname()[4], type=self.eth_type) / raw(payload)
            so.send(raw(eth_frame))

    def recv_resp_poke(self, mac: str, i: int = 0) -> None:
        eth_type_ctrl = self.eth_type + 1
        with self.configure_eth_if(eth_type_ctrl) as so:
            so.settimeout(30)
            for _ in range(10):
                eth_frame = Ether(so.recv(60))
                if mac == eth_frame.src and eth_frame.load[0] == POKE_REQ:
                    if eth_frame.load[1] != i:
                        raise RuntimeError('Missed Poke Packet')
                    logging.info('Poke Packet received...')
                    eth_frame.dst = eth_frame.src
                    eth_frame.src = so.getsockname()[4]
                    eth_frame.load = bytes([POKE_RESP])
                    so.send(raw(eth_frame))
                    return
                logging.warning('Unexpected Control packet')
                logging.warning(f'Expected Ctrl command: 0x{POKE_REQ:02x}, actual: 0x{eth_frame.load[0]:02x}')
                logging.warning('Source MAC %s', eth_frame.src)
            raise RuntimeError('No Poke Packet!')

    def traffic_gen(self, mac: str, pipe_rcv: connection.Connection) -> None:
        with self.configure_eth_if() as so:
            payload = bytes.fromhex('ff') + bytes(1485)
            eth_frame = Ether(dst=mac, src=so.getsockname()[4], type=self.eth_type) / raw(payload)
            while pipe_rcv.poll() is not True:
                so.send(raw(eth_frame))

    def eth_loopback(self, mac: str, pipe_rcv: connection.Connection) -> None:
        with self.configure_eth_if(self.eth_type) as so:
            so.settimeout(30)
            while pipe_rcv.poll() is not True:
                eth_frame = Ether(so.recv(1522))
                if mac == eth_frame.src:
                    eth_frame.dst = eth_frame.src
                    eth_frame.src = so.getsockname()[4]
                    so.send(raw(eth_frame))
                else:
                    logging.warning('Received frame from unexpected source %s', eth_frame.src)


class EthTestRunner:
    """
    Runner for eth_test_app Unity test groups.

    Use this class in driver-specific pytest files to run the common Ethernet
    test suite (ethernet, ethernet_l2, heap utilization) against a DUT.
    """

    def __init__(self, eth_type: int = ETH_TYPE) -> None:
        self.eth_type = eth_type

    def run_ethernet_test_apps(self, dut, timeout: int = 980) -> None:
        """Run all single-board test cases in the 'ethernet' group (init, IO, loopback, events, DHCP, etc.)."""
        dut.run_all_single_board_cases(group='ethernet', timeout=timeout)

    def run_ethernet_l2_test(self, dut, test_if: str = '') -> None:
        """Run L2 tests: broadcast transmit, recv_pkt with filters, start/stop stress under traffic."""
        target_if = EthTestIntf(self.eth_type, test_if)

        dut.expect_exact('Press ENTER to see the list of tests')
        dut.write('\n')
        dut.expect_exact('Enter test for running.')

        with target_if.configure_eth_if() as so:
            so.settimeout(30)
            dut.write('"ethernet broadcast transmit"')
            res = dut.expect(
                r'DUT MAC: ([0-9A-Fa-f]{2}:[0-9A-Fa-f]{2}:[0-9A-Fa-f]{2}:[0-9A-Fa-f]{2}:[0-9A-Fa-f]{2}:[0-9A-Fa-f]{2})'
            )
            dut_mac = res.group(1).decode('utf-8')
            target_if.recv_resp_poke(mac=dut_mac)

            for _ in range(10):
                eth_frame = Ether(so.recv(1024))
                if dut_mac == eth_frame.src:
                    break
            else:
                raise RuntimeError('No broadcast received from expected DUT MAC addr')

            for i in range(0, 1010):
                if eth_frame.load[i] != i & 0xFF:
                    raise RuntimeError('Packet content mismatch')
        dut.expect_unity_test_output()

        dut.expect_exact("Enter next test, or 'enter' to see menu")
        dut.write('"ethernet recv_pkt"')
        res = dut.expect(
            r'DUT MAC: ([0-9A-Fa-f]{2}:[0-9A-Fa-f]{2}:[0-9A-Fa-f]{2}:[0-9A-Fa-f]{2}:[0-9A-Fa-f]{2}:[0-9A-Fa-f]{2})'
        )
        dut_mac = res.group(1).decode('utf-8')
        target_if.recv_resp_poke(mac=dut_mac)
        for _ in range(5):
            dut.expect_exact('Filter configured')
            target_if.send_eth_packet('ff:ff:ff:ff:ff:ff')
            target_if.send_eth_packet('01:00:5e:00:00:00')
            target_if.send_eth_packet('33:33:00:00:00:00')
            target_if.send_eth_packet(mac=dut_mac)

        dut.expect_unity_test_output(extra_before=res.group(1))

        dut.expect_exact("Enter next test, or 'enter' to see menu")
        dut.write('"ethernet start/stop stress test under heavy traffic"')
        res = dut.expect(
            r'DUT MAC: ([0-9A-Fa-f]{2}:[0-9A-Fa-f]{2}:[0-9A-Fa-f]{2}:[0-9A-Fa-f]{2}:[0-9A-Fa-f]{2}:[0-9A-Fa-f]{2})'
        )
        dut_mac = res.group(1).decode('utf-8')
        for tx_i in range(10):
            target_if.recv_resp_poke(dut_mac, tx_i)
            dut.expect_exact('Ethernet Stopped')

        for rx_i in range(10):
            target_if.recv_resp_poke(dut_mac, rx_i)
            pipe_rcv, pipe_send = Pipe(False)
            tx_proc = Process(
                target=target_if.traffic_gen,
                args=(dut_mac, pipe_rcv),
            )
            tx_proc.start()
            try:
                dut.expect_exact('Ethernet Stopped')
            finally:
                pipe_send.send(0)
            tx_proc.join(5)
            if tx_proc.exitcode is None:
                tx_proc.terminate()

        dut.expect_unity_test_output(extra_before=res.group(1))

    def run_ethernet_heap_alloc_test(self, dut, test_if: str = '') -> None:
        """Run heap utilization test (may require host loopback if PHY loopback is disabled)."""
        target_if = EthTestIntf(self.eth_type, test_if)
        dut.expect_exact('Press ENTER to see the list of tests')
        dut.write('\n')
        dut.expect_exact('Enter test for running.')
        dut.write('"heap utilization"')
        res = dut.expect(r'PHY loopback is (enabled|disabled)')
        phy_loopback = res.group(1).decode('utf-8')
        if phy_loopback == 'disabled':
            logging.info('Starting loopback server...')
            res = dut.expect(
                r'DUT MAC: ([0-9A-Fa-f]{2}:[0-9A-Fa-f]{2}:[0-9A-Fa-f]{2}:[0-9A-Fa-f]{2}:[0-9A-Fa-f]{2}:[0-9A-Fa-f]{2})'
            )
            dut_mac = res.group(1).decode('utf-8')
            pipe_rcv, pipe_send = Pipe(False)
            loopback_proc = Process(
                target=target_if.eth_loopback,
                args=(dut_mac, pipe_rcv),
            )
            loopback_proc.start()
            target_if.recv_resp_poke(mac=dut_mac)
            dut.expect_exact('Ethernet Stopped')
            pipe_send.send(0)
            loopback_proc.join(5)
            if loopback_proc.exitcode is None:
                loopback_proc.terminate()

        dut.expect_unity_test_output()
