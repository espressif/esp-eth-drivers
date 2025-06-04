# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
import os
import sys

from pytest_embedded import Dut
from test_common import HelperFunctions

# Import test_common.py from parent directory
sys.path.insert(1, '../../')
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

def test_ksz8863_simple_switch(dut: Dut) -> None:
    # Connect to virtual machines
    runner.connect(IP_ADDRESS_RUNNER, SSH_VM_USERNAME, SSH_VM_PASSWORD)
    endnode.connect(IP_ADDRESS_ENDNODE, SSH_VM_USERNAME, SSH_VM_PASSWORD)
    switch.connect(IP_ADDRESS_SWITCH, SSH_SW_USERNAME, SSH_SW_PASSWORD)
    # Upload test script to the VMs
    runner.put('../../vm_test_app.py', '~/vm_test_app.py')
    endnode.put('../../vm_test_app.py', '~/vm_test_app.py')
    # Wait for ESP32 to initialize
    dut.expect('Ethernet Got IP Address')
    # Check that MAC addresses of both VMs are in the Dynamic MAC table
    dut.expect('valid entries 3')
    #entries_count = int(dut.expect(r"valid entries ([0-9]+)").group(1))
    dynamic_mac_table = []
    entries_count = 3
    for _i in range(entries_count):
        mac = dut.expect(r'([0-9a-f]{2} [0-9a-f]{2} [0-9a-f]{2} [0-9a-f]{2} [0-9a-f]{2} [0-9a-f]{2})').group(1).decode('ascii')
        dynamic_mac_table.append(mac.replace(' ', ':'))
    assert runner.get_interface_mac_address('enp3s0') in dynamic_mac_table
    assert endnode.get_interface_mac_address('enp3s0') in dynamic_mac_table
    # Attempt to get endnode IP proving that there's connection between the VMs
    endnode_ip = endnode.get_interface_ip('enp3s0')
    assert endnode_ip is not None
    # Attempt to transmit data
    assert HelperFunctions.PerformTransmissions(runner, endnode) == (True, True)
    # Bring Endnode interface down and back up
    switch.bring_port(3, 'down')
    dut.expect(r'Ethernet Link Down Port (\d)')
    switch.bring_port(3, 'up')
    dut.expect(r'Ethernet Link Up Port (\d)')
    # Bring runner interface down and back up
    switch.bring_port(2, 'down')
    dut.expect(r'Ethernet Link Down Port (\d)')
    switch.bring_port(2, 'up')
    dut.expect(r'Ethernet Link Up Port (\d)')
