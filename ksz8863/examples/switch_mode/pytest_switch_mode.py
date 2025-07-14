# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
import logging
import os
import sys

import pytest

from pytest_embedded import Dut

# Import test_common.py from parent directory
sys.path.append('../../')
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
    # Upload test script to the VMs
    runner.put('../../vm_test_app.py', 'vm_test_app.py')
    endnode.put('../../vm_test_app.py', 'vm_test_app.py')
    # Ensure that ports are brought up on the switch
    switch.bring_port(2, 'up')
    switch.bring_port(3, 'up')

# We can't be sure that the broadcast packet will be in any random sequence of packets
# 20 makes it being received very likely, but for the better measure the test is marked as flaky
@pytest.mark.flaky(reruns=3, reruns_delay=5)
def test_ksz8863_managed_switch_tailtag(dut: Dut) -> None:
    # Wait for ESP32 to initialize
    dut.expect('Ethernet Got IP Address')
    runner_out = runner.execute('tcpdump -A -i enp3s0 -c 20')
    endnode_out = endnode.execute('tcpdump -A -i enp3s0 -c 20')
    if 'This is ESP32 L2 TAP test msg for Port 1' not in runner_out:
        logging.error(runner_out)
        raise RuntimeError('Runner has not received a broadcast from device\'s port 1')
    if 'This is ESP32 L2 TAP test msg for Port 2' in runner_out:
        logging.error(runner_out)
        raise RuntimeError('Runner has received a broadcast from device\'s port 2 which was not meant for it')

    if 'This is ESP32 L2 TAP test msg for Port 2' not in endnode_out:
        logging.error(endnode_out)
        raise RuntimeError('Endnode has not received a broadcast from device\'s port 2')
    if 'This is ESP32 L2 TAP test msg for Port 1' in endnode_out:
        logging.error(runner_out)
        raise RuntimeError('Endnode has received a broadcast from device\'s port 1 which was not meant for it')
