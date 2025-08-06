# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
import os
import sys
import time

import pytest

from pytest_embedded import Dut

# Import test_common.py from parent directory
sys.path.append('../../')
from test_common import HelperFunctions
from test_common import SwitchSSH
from test_common import VirtualMachineSSH

IP_ADDRESS_RUNNER = os.environ.get('IP_ADDRESS_RUNNER')
IP_ADDRESS_ENDNODE = os.environ.get('IP_ADDRESS_ENDNODE')
IP_ADDRESS_SWITCH = os.environ.get('IP_ADDRESS_SWITCH')
SSH_VM_USERNAME = os.environ.get('SSH_VM_USERNAME')
SSH_VM_PASSWORD = os.environ.get('SSH_VM_PASSWORD')
SSH_SW_USERNAME = os.environ.get('SSH_SW_USERNAME')
SSH_SW_PASSWORD = os.environ.get('SSH_SW_PASSWORD')
SSH_PORT = 22

runner = VirtualMachineSSH('Runner')
endnode = VirtualMachineSSH('Endnode')
switch = SwitchSSH('Switch')
@pytest.fixture(scope='session', autouse=True)
def prepare_vms_and_ksz8863():
    # Connect to virtual machines
    runner.connect(IP_ADDRESS_RUNNER, SSH_VM_USERNAME, SSH_VM_PASSWORD)
    endnode.connect(IP_ADDRESS_ENDNODE, SSH_VM_USERNAME, SSH_VM_PASSWORD)
    switch.connect(IP_ADDRESS_SWITCH, SSH_SW_USERNAME, SSH_SW_PASSWORD)
    # Ensure that ports are brought up on the switch
    switch.bring_port(2, 'up')
    switch.bring_port(3, 'up')

def test_ksz8863_simple_switch_macdyntbl(dut: Dut) -> None:
    # Wait for ESP32 to initialize
    dut.expect('Ethernet Got IP Address')

    # Check that MAC addresses of both VMs are in the Dynamic MAC table
    runner_mac = runner.get_interface_mac_address('enp3s0')
    endnode_mac = endnode.get_interface_mac_address('enp3s0')

    # We may need several tries while we wait for the dynamic mac table to populate
    # The MAC addresses are only added when some data comes through thr switch, so we may
    # need to wait a little for all devices to be added.
    attempts = 30
    while attempts > 0:
        dut.write('\n')
        dut.expect('ksz8863>')
        dut.write('switch show macdyntbl 6\n')

        entries_count = int(dut.expect(r'valid entries ([0-9]+)').group(1))
        dynamic_mac_table = []
        for _i in range(entries_count):
            mac = dut.expect(r'([0-9a-f]{2} [0-9a-f]{2} [0-9a-f]{2} [0-9a-f]{2} [0-9a-f]{2} [0-9a-f]{2})').group(0).decode('ascii')
            dynamic_mac_table.append(mac.replace(' ', ':'))

        if runner_mac in dynamic_mac_table and endnode_mac in dynamic_mac_table:
            # Break will not trigger else clause
            break

        attempts -= 1
        time.sleep(1)
    else:
        raise RuntimeError('Invalid Dynamic MAC Table - after 30s of attempts Dynamic MAC Table is still missing expected MAC addresses')


def test_ksz8863_simple_switch_link(dut: Dut) -> None:
    # Wait for ESP32 to initialize
    dut.expect('Ethernet Got IP Address')

    # Bring Endnode and Runner interfaces down and back up
    switch.bring_port(3, 'down')
    dut.expect(r'Ethernet Link Down Port (\\d)')
    switch.bring_port(2, 'down')
    dut.expect(r'Ethernet Link Down Port (\\d)')

    ports_down_transmission_test = HelperFunctions.PerformTransmissionTest(runner, endnode)

    switch.bring_port(3, 'up')
    dut.expect(r'Ethernet Link Up Port (\\d)')
    switch.bring_port(2, 'up')
    dut.expect(r'Ethernet Link Up Port (\\d)')

    if ports_down_transmission_test != (False, False):
        raise RuntimeError('Some data came through even though both ports were supposed to be down')

@pytest.mark.flaky(reruns=3, reruns_delay=5)
def test_ksz8863_simple_switch_txrx(dut: Dut) -> None:
    # Wait for ESP32 to initialize
    dut.expect('Ethernet Got IP Address')

    # Attempt to transmit data with different TX/RX configurations
    # Important note:
    # TX enable means that the PORT ITSELF will transmit the data, so disabling TX on Port 1 will prevent device from RECEIVING data and
    # enabling RX on Port 1 will allow device connected to it to transmit data to the network
    tx_rx_test_conditions: list = [
        {'port1': {'tx_en': True,  'rx_en': False}, 'port2': {'tx_en': False, 'rx_en': True},  'expected_result': (False, True)},
        {'port1': {'tx_en': True,  'rx_en': True},  'port2': {'tx_en': True,  'rx_en': True},  'expected_result': (True, True)},
        {'port1': {'tx_en': False, 'rx_en': True},  'port2': {'tx_en': True,  'rx_en': False}, 'expected_result': (True, False)},
        {'port1': {'tx_en': False, 'rx_en': False}, 'port2': {'tx_en': False, 'rx_en': False}, 'expected_result': (False, False)},
    ]
    for condition in tx_rx_test_conditions:
        dut.write('\n')
        dut.expect('ksz8863>')
        dut.write(f'switch -p 1 set rx {'1' if condition['port1']['rx_en'] else '0'}\n')
        dut.write(f'switch -p 1 set tx {'1' if condition['port1']['tx_en'] else '0'}\n')
        dut.write(f'switch -p 2 set tx {'1' if condition['port2']['tx_en'] else '0'}\n')
        dut.write(f'switch -p 2 set rx {'1' if condition['port2']['rx_en'] else '0'}\n')

        result = HelperFunctions.PerformTransmissionTest(runner, endnode)
        if result != condition['expected_result']:
            raise RuntimeError(f'The resulting Success/Failure pattern {result} did not match the expected {condition['expected_result']}')
    # Enable RX and TX back on
    dut.write('switch -p 1 set tx 1\n')
    dut.write('switch -p 1 set rx 1\n')
    dut.write('switch -p 2 set tx 1\n')
    dut.write('switch -p 2 set rx 1\n')

def test_ksz8863_simple_switch_macstatbl(dut: Dut) -> None: #noqa: C901
    # Wait for ESP32 to initialize
    dut.expect('Ethernet Got IP Address')

    # Check that MAC addresses of both VMs are in the Dynamic MAC table
    runner_mac = runner.get_interface_mac_address('enp3s0')
    endnode_mac = endnode.get_interface_mac_address('enp3s0')
    # Check that broadcasts go through the way we expect them to
    macstatbl_test_conditions: list[dict[str, str | tuple[bool, bool]]] = [
        # Forwarding rule (Port 3, Port 2, Port 1)     Endnode,ESP has received                 Runner,ESP has received              Runner,Endnode has received
        {'forwarding': '000', 'runner_bcast_results': (False, False), 'endnode_bcast_results': (False, False), 'esp_bcast_results': (False, False)},
        {'forwarding': '001', 'runner_bcast_results': (False, False), 'endnode_bcast_results': (True, False),  'esp_bcast_results': (True, False)},
        {'forwarding': '010', 'runner_bcast_results': (True, False),  'endnode_bcast_results': (False, False), 'esp_bcast_results': (False, True)},
        {'forwarding': '011', 'runner_bcast_results': (True, False),  'endnode_bcast_results': (True, False),  'esp_bcast_results': (True, True)},
        {'forwarding': '100', 'runner_bcast_results': (False, True),  'endnode_bcast_results': (False, True),  'esp_bcast_results': (False, False)},
        {'forwarding': '101', 'runner_bcast_results': (False, True),  'endnode_bcast_results': (True, True),   'esp_bcast_results': (True, False)},
        {'forwarding': '110', 'runner_bcast_results': (True, True),   'endnode_bcast_results': (False, True),  'esp_bcast_results': (False, True)},
        {'forwarding': '111', 'runner_bcast_results': (True, True),   'endnode_bcast_results': (True, True),   'esp_bcast_results': (True, True)},
    ]

    for condition in macstatbl_test_conditions:
        mac_sta_tbl_entry = f"0 ff:ff:ff:ff:ff:ff {condition['forwarding']} E-- 0"
        dut.write('\n')
        dut.expect('ksz8863>')
        dut.write(f'switch set macstatbl \"{mac_sta_tbl_entry}\"\n')
        time.sleep(0.5)

        # Runner broadcasting
        endnode.execute_async('timeout 5 socat - udp-recvfrom:12345,broadcast,fork')
        HelperFunctions.PerformBroadcastAsync(runner)
        runner.wait_until_process_finish()
        endnode_received_from_runner = 'Broadcast' in endnode.wait_until_process_finish()
        # By Python standard 'and' and 'or' are evaluated lazily
        # dut.expect will only be evaluated if condition['runner_bcast_results'][1] is True
        # dut.expect will also terminate the test if it fails
        esp_received_from_runner = condition['runner_bcast_results'][1] and (dut.expect(rf'Host has received \d+ bytes from {runner_mac}') is not None)
        rbcast_result = (endnode_received_from_runner, esp_received_from_runner)
        if rbcast_result != condition['runner_bcast_results']:
            raise RuntimeError(f'Results of runner broadcast did not match expected. Expected {condition['runner_bcast_results']}, got {rbcast_result}')

        # Endnode broadcasting
        runner.execute_async('timeout 5 socat - udp-recvfrom:12345,broadcast,fork')
        HelperFunctions.PerformBroadcastAsync(endnode)
        endnode.wait_until_process_finish()
        runner_received_from_endnode = 'Broadcast' in runner.wait_until_process_finish()
        esp_received_from_endnode = condition['endnode_bcast_results'][1] and (dut.expect(rf'Host has received \d+ bytes from {endnode_mac}') is not None)
        ebcast_result = (runner_received_from_endnode, esp_received_from_endnode)
        if ebcast_result != condition['endnode_bcast_results']:
            raise RuntimeError(f'Results of endnode broadcast did not match expected. Was expecting {condition['endnode_bcast_results']}, got {ebcast_result}')

        # ESP Broadcasting
        runner.execute_async('timeout 5 tcpdump -A -i enp3s0')
        endnode.execute_async('timeout 5 tcpdump -A -i enp3s0')
        runner_output = runner.wait_until_process_finish()
        endnode_output = endnode.wait_until_process_finish()
        endnode_received_from_esp = 'L2.TAP.test.msg' in endnode_output
        runner_received_from_esp = 'L2.TAP.test.msg' in runner_output
        hbcast_result = (runner_received_from_esp, endnode_received_from_esp)
        if hbcast_result != condition['esp_bcast_results']:
            raise RuntimeError(f'Results of ESP broadcast did not match expected. Was expecting {condition['esp_bcast_results']}, got {hbcast_result}')
