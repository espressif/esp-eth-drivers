# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""
Test for simple switch example
Includes following tests:
 i. Ping test
 ii. Dynamic MAC table test
 iii. Link test
 iv. Port RX/TX test
 v. Static MAC table test
"""
import os
import time

import pytest

from pytest_embedded import Dut
from test_common import HelperFunctions
from test_common import SwitchSSH
from test_common import VirtualMachineSSH

print(HelperFunctions)
IP_ADDRESS_RUNNER = os.environ.get('IP_ADDRESS_RUNNER')
IP_ADDRESS_ENDNODE = os.environ.get('IP_ADDRESS_ENDNODE')
IP_ADDRESS_SWITCH = os.environ.get('IP_ADDRESS_SWITCH')
SSH_VM_USERNAME = os.environ.get('SSH_VM_USERNAME')
SSH_VM_PASSWORD = os.environ.get('SSH_VM_PASSWORD')
SSH_SW_USERNAME = os.environ.get('SSH_SW_USERNAME')
SSH_SW_PASSWORD = os.environ.get('SSH_SW_PASSWORD')
SSH_PORT = 22

SWITCH_PORT_RUNNER = 2
SWITCH_PORT_ENDNODE = 3

runner = VirtualMachineSSH('Runner')
endnode = VirtualMachineSSH('Endnode')
switch = SwitchSSH('Switch')


@pytest.fixture(scope='session', autouse=True)
def prepare_vms_and_ksz8863():
    """
    Connect to virtual machines, pull switch ports up to guarantee that no lingering effects from previous tests are present
    """
    # Connect to virtual machines
    runner.connect(IP_ADDRESS_RUNNER, SSH_VM_USERNAME, SSH_VM_PASSWORD)
    endnode.connect(IP_ADDRESS_ENDNODE, SSH_VM_USERNAME, SSH_VM_PASSWORD)
    switch.connect(IP_ADDRESS_SWITCH, SSH_SW_USERNAME, SSH_SW_PASSWORD)
    # Ensure that ports are brought up on the switch
    switch.bring_port(SWITCH_PORT_RUNNER, 'up')
    switch.bring_port(SWITCH_PORT_ENDNODE, 'up')

def test_ksz8863_simple_switch_ping(dut: Dut) -> None:
    """
    Check whether the endnode and the host are pingable
    """
    # Wait for ESP32 to initialize and an IP to be assigned
    dut_ip = dut.expect(r'esp_netif_handlers: .+ ip: (\d+\.\d+\.\d+\.\d+),').group(1).decode()
    # ping it once
    if '1 packets transmitted, 1 received' not in runner.execute(f'ping -c 1 {dut_ip}'):
        raise RuntimeError('Unable to ping host device')
    # Ping endnode as well
    endnode_ip = endnode.get_interface_ip('enp3s0')
    if '1 packets transmitted, 1 received' not in runner.execute(f'ping -c 1 {endnode_ip}'):
        raise RuntimeError('Unable to ping endnode')


def test_ksz8863_simple_switch_macdyntbl(dut: Dut) -> None:
    """
    Check whether Dynamic MAC table is being populated and read as expected
    """
    # Wait for ESP32 to initialize
    dut.expect('Ethernet Got IP Address')
    # Check that MAC addresses of both VMs are in the Dynamic MAC table
    runner_mac = runner.get_interface_mac_address('enp3s0')
    endnode_mac = endnode.get_interface_mac_address('enp3s0')

    # We may need several tries while we wait for the dynamic mac table to populate
    # The MAC addresses are only added when some data comes through the switch, so we may
    # need to wait a little for all devices to be added.
    # We will also try to force at least some transmission to happen so that the Dynamic MAC table is populated quicker
    HelperFunctions.perform_l2_broadcast_async(runner, 'enp3s0', 0x7000, 'Broadcast')
    HelperFunctions.perform_l2_broadcast_async(endnode, 'enp3s0', 0x7000, 'Broadcast')
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
    """
    Check whether host device is able to detect link being down
    """
    # Wait for ESP32 to initialize
    dut.expect('Ethernet Got IP Address')

    # Bring Endnode and Runner interfaces down and back up
    switch.bring_port(SWITCH_PORT_ENDNODE, 'down')
    dut.expect(r'Ethernet Link Down Port 2')
    switch.bring_port(SWITCH_PORT_RUNNER, 'down')
    dut.expect(r'Ethernet Link Down Port 1')

    switch.bring_port(SWITCH_PORT_ENDNODE, 'up')
    dut.expect(r'Ethernet Link Up Port 2')
    switch.bring_port(SWITCH_PORT_RUNNER, 'up')
    dut.expect(r'Ethernet Link Up Port 1')

    # Check that when we pull switch ports up or down the transmissions get received as expected
    link_test_conditions: list = [
        {'switch_ports_link': {'port_runner': 'down', 'port_endnode': 'down'},  'runner_should_receive': False, 'endnode_should_receive': False},
        {'switch_ports_link': {'port_runner': 'up', 'port_endnode': 'down'},    'runner_should_receive': True, 'endnode_should_receive': False},
        {'switch_ports_link': {'port_runner': 'down', 'port_endnode': 'up'},    'runner_should_receive': False, 'endnode_should_receive': True},
        {'switch_ports_link': {'port_runner': 'up', 'port_endnode': 'up'},      'runner_should_receive': True, 'endnode_should_receive': True},
    ]
    for condition in link_test_conditions:
        switch.bring_port(SWITCH_PORT_RUNNER, condition['switch_ports_link']['port_runner'])
        switch.bring_port(SWITCH_PORT_ENDNODE, condition['switch_ports_link']['port_endnode'])

        runner_out = runner.execute('timeout 5 tcpdump -A -i enp3s0')
        if condition['runner_should_receive'] and 'This is ESP32 L2 TAP test msg' not in runner_out:
            switch.bring_port(SWITCH_PORT_RUNNER, 'up')
            switch.bring_port(SWITCH_PORT_ENDNODE, 'up')
            raise RuntimeError('Runner should have received a test message, but did not.' +
                               f'Switch runner port is {condition["switch_ports_link"]["port_runner"]},' +
                               f'endnode port is {condition["switch_ports_link"]["port_endnode"]}')
        if not condition['runner_should_receive'] and 'This is ESP32 L2 TAP test msg' in runner_out:
            switch.bring_port(SWITCH_PORT_RUNNER, 'up')
            switch.bring_port(SWITCH_PORT_ENDNODE, 'up')
            raise RuntimeError('Runner should not have received a test message, but did.' +
                               f'Switch runner port is {condition["switch_ports_link"]["port_runner"]},' +
                               f'endnode port is {condition["switch_ports_link"]["port_endnode"]}')

        endnode_out = runner.execute('timeout 5 tcpdump -A -i enp3s0')
        if condition['runner_should_receive'] and 'This is ESP32 L2 TAP test msg' not in endnode_out:
            switch.bring_port(SWITCH_PORT_RUNNER, 'up')
            switch.bring_port(SWITCH_PORT_ENDNODE, 'up')
            raise RuntimeError('Endnode should have received a test message, but did not.' +
                               f'Switch runner port is {condition["switch_ports_link"]["port_runner"]},' +
                               f'endnode port is {condition["switch_ports_link"]["port_endnode"]}')
        if not condition['runner_should_receive'] and 'This is ESP32 L2 TAP test msg' in endnode_out:
            switch.bring_port(SWITCH_PORT_RUNNER, 'up')
            switch.bring_port(SWITCH_PORT_ENDNODE, 'up')
            raise RuntimeError('Endnode should not have received a test message, but did.' +
                               f'Switch runner port is {condition["switch_ports_link"]["port_runner"]},' +
                               f'endnode port is {condition["switch_ports_link"]["port_endnode"]}')


def test_ksz8863_simple_switch_txrx(dut: Dut) -> None:
    """
    Check whether configuring RX/TX capabilities on ports works and is producing expected results
    """
    # Wait for ESP32 to initialize
    dut.expect('Ethernet Got IP Address')

    # Attempt to transmit data with different TX/RX configurations
    # Important note:
    # TX here refers to the transmission from KSZ8863. This means that when TX is disabled on Port 1 KSZ8863 will not write anything to Port 1
    # and it will result in the computer that is connected to Port 1 to not receive any data.
    # Same for RX - when RX is disabled KSZ8863 will not read any data from the port, and the connected device will not be able to transmit.
    tx_rx_test_conditions: list = [
        {'port1': {'tx_en': True,  'rx_en': False}, 'port2': {'tx_en': False, 'rx_en': True},  'expected_result': (False, True)},
        {'port1': {'tx_en': True,  'rx_en': True},  'port2': {'tx_en': True,  'rx_en': True},  'expected_result': (True, True)},
        {'port1': {'tx_en': False, 'rx_en': True},  'port2': {'tx_en': True,  'rx_en': False}, 'expected_result': (True, False)},
        {'port1': {'tx_en': False, 'rx_en': False}, 'port2': {'tx_en': False, 'rx_en': False}, 'expected_result': (False, False)},
    ]
    for condition in tx_rx_test_conditions:
        dut.write('\n')
        dut.expect('ksz8863>')
        dut.write(f'switch -p 1 set rx {"1" if condition["port1"]["rx_en"] else "0"}\n')
        dut.write(f'switch -p 1 set tx {"1" if condition["port1"]["tx_en"] else "0"}\n')
        dut.write(f'switch -p 2 set tx {"1" if condition["port2"]["tx_en"] else "0"}\n')
        dut.write(f'switch -p 2 set rx {"1" if condition["port2"]["rx_en"] else "0"}\n')

        result = HelperFunctions.perform_transmission_test(runner, endnode)
        if result != condition['expected_result']:
            raise RuntimeError(f'The resulting Success/Failure pattern {result} did not match the expected {condition["expected_result"]}')
    # Enable RX and TX back on
    dut.write('switch -p 1 set tx 1\n')
    dut.write('switch -p 1 set rx 1\n')
    dut.write('switch -p 2 set tx 1\n')
    dut.write('switch -p 2 set rx 1\n')


def test_ksz8863_simple_switch_macstatbl(dut: Dut) -> None:
    """
    Check whether static MAC table is readable, writable, and changes to it are resulting in the expected outcome
    """
    # Wait for ESP32 to initialize
    dut.expect('Ethernet Got IP Address')

    # Check that MAC addresses of both VMs are in the Dynamic MAC table
    runner_mac = runner.get_interface_mac_address('enp3s0')
    endnode_mac = endnode.get_interface_mac_address('enp3s0')
    # Prepare commands for l2 broadcasts
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
        mac_sta_tbl_entry = f'0 ff:ff:ff:ff:ff:ff {condition["forwarding"]} E-- 0'
        dut.write('\n')
        dut.expect('ksz8863>')
        dut.write(f'switch set macstatbl \"{mac_sta_tbl_entry}\"\n')
        time.sleep(0.5)

        # Runner broadcasting
        endnode.execute_async('timeout 5 tcpdump -A -i enp3s0')
        #runner.execute_async('for i in {1..5}; do echo "Broadcast"; sleep 0.75; done | socat - udp-datagram:120.140.1.255:12345,broadcast,sp=12345')
        HelperFunctions.perform_l2_broadcast_async(runner, 'enp3s0', 0x7000, 'Broadcast')
        runner.wait_until_process_finish()
        endnode_received_from_runner = 'Broadcast' in endnode.wait_until_process_finish()
        # By Python standard 'and' and 'or' are evaluated lazily
        # dut.expect will only be evaluated if condition['runner_bcast_results'][1] is True
        # dut.expect will also terminate the test if it fails
        esp_received_from_runner = condition['runner_bcast_results'][1] and (dut.expect(rf'Host has received \d+ bytes from {runner_mac}') is not None)
        rbcast_result = (endnode_received_from_runner, esp_received_from_runner)
        if rbcast_result != condition['runner_bcast_results']:
            raise RuntimeError(f'Results of runner broadcast did not match expected. Expected {condition["runner_bcast_results"]}, got {rbcast_result}')

        # Endnode broadcasting
        runner.execute_async('timeout 5 tcpdump -A -i enp3s0')
        #endnode.execute_async('for i in {1..5}; do echo "Broadcast"; sleep 0.75; done | socat - udp-datagram:120.140.1.255:12345,broadcast,sp=12345')
        HelperFunctions.perform_l2_broadcast_async(endnode, 'enp3s0', 0x7000, 'Broadcast')
        endnode.wait_until_process_finish()
        runner_received_from_endnode = 'Broadcast' in runner.wait_until_process_finish()
        esp_received_from_endnode = condition['endnode_bcast_results'][1] and (dut.expect(rf'Host has received \d+ bytes from {endnode_mac}') is not None)
        ebcast_result = (runner_received_from_endnode, esp_received_from_endnode)
        if ebcast_result != condition['endnode_bcast_results']:
            raise RuntimeError(f'Results of endnode broadcast did not match expected. Was expecting {condition["endnode_bcast_results"]}, got {ebcast_result}')

        # ESP Broadcasting
        runner.execute_async('timeout 5 tcpdump -A -i enp3s0')
        endnode.execute_async('timeout 5 tcpdump -A -i enp3s0')
        endnode_received_from_esp = 'L2.TAP.test.msg' in endnode.wait_until_process_finish()
        runner_received_from_esp = 'L2.TAP.test.msg' in runner.wait_until_process_finish()
        hbcast_result = (runner_received_from_esp, endnode_received_from_esp)
        if hbcast_result != condition['esp_bcast_results']:
            raise RuntimeError(f'Results of ESP broadcast did not match expected. Was expecting {condition["esp_bcast_results"]}, got {hbcast_result}')
