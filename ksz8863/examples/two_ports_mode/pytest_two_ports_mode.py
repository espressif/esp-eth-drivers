# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
import os
import sys

from pytest_embedded import Dut

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

def s_test_ksz8863_two_ports_mode(dut: Dut) -> None:
    # Connect to virtual machines
    runner.connect(IP_ADDRESS_RUNNER, SSH_VM_USERNAME, SSH_VM_PASSWORD)
    endnode.connect(IP_ADDRESS_ENDNODE, SSH_VM_USERNAME, SSH_VM_PASSWORD)
    switch.connect(IP_ADDRESS_SWITCH, SSH_SW_USERNAME, SSH_SW_PASSWORD)
    # Wait for ESP32 to initialize
    dut.expect('main_task: Returned from app_main()')
    # Wait until we get an L2 test messages which the example periodically transmits
    runner_out = runner.execute('tcpdump -A -i enp3s0 -c 3')
    endnode_out = endnode.execute('tcpdump -A -i enp3s0 -c 3')
    assert 'This is ESP32 L2 TAP test msg from Port: 1' in runner_out
    assert 'This is ESP32 L2 TAP test msg from Port: 2' in endnode_out
    # Up / Down
    switch.bring_port(3, 'down')
    dut.expect(r'Ethernet Link Down Port (\d)')
    switch.bring_port(3, 'up')
    dut.expect(r'Ethernet Link Up Port (\d)')
    switch.bring_port(2, 'down')
    dut.expect(r'Ethernet Link Down Port (\d)')
    switch.bring_port(2, 'up')
    dut.expect(r'Ethernet Link Up Port (\d)')
