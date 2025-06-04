# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
import os
import sys
import time

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

def test_ksz8863_switch_mode(dut: Dut) -> None:
    # Connect to virtual machines
    runner.connect(IP_ADDRESS_RUNNER, SSH_VM_USERNAME, SSH_VM_PASSWORD)
    endnode.connect(IP_ADDRESS_ENDNODE, SSH_VM_USERNAME, SSH_VM_PASSWORD)
    switch.connect(IP_ADDRESS_SWITCH, SSH_SW_USERNAME, SSH_SW_PASSWORD)
    # Upload test script to the VMs
    runner.put('../../vm_test_app.py', 'vm_test_app.py')
    endnode.put('../../vm_test_app.py', 'vm_test_app.py')
    # Wait for ESP32 to initialize
    dut.expect('main_task: Returned from app_main()')
    # Trigger endnode re-requesting IP address from the runner
    endnode.execute('ip link set dev enp3s0 down')
    endnode.execute('ip link set dev enp3s0 up')
    # Verify that disabling TX or RX produces expected results
    # | Case # | Port 1 | Port 2 | ENDNODE -> RUNNER | RUNNER -> ENDNODE |
    # |   1    |   --   |   --   |        Fails      |       Fails       |
    # |   2    |   TX   |   RX   |       Succeeds    |       Fails       |
    # |   3    |   RX   |   TX   |        Fails      |      Succeeds     |
    # |   4    |  TX RX |  TX RX |       Succeeds    |      Succeeds     |
    dut.write('switch -p 1 set tx 0\n')
    dut.write('switch -p 1 set rx 0\n')
    dut.write('switch -p 2 set tx 0\n')
    dut.write('switch -p 2 set rx 0\n')
    assert HelperFunctions.PerformTransmissions(endnode, runner) == (False, False)
    dut.write('switch -p 1 set tx 1\n')
    dut.write('switch -p 1 set rx 0\n')
    dut.write('switch -p 2 set tx 0\n')
    dut.write('switch -p 2 set rx 1\n')
    assert HelperFunctions.PerformTransmissions(endnode, runner) == (True, False)
    dut.write('switch -p 1 set tx 0\n')
    dut.write('switch -p 1 set rx 1\n')
    dut.write('switch -p 2 set tx 1\n')
    dut.write('switch -p 2 set rx 0\n')
    assert HelperFunctions.PerformTransmissions(endnode, runner) == (False, True)
    dut.write('switch -p 1 set tx 1\n')
    dut.write('switch -p 1 set rx 1\n')
    dut.write('switch -p 2 set tx 1\n')
    dut.write('switch -p 2 set rx 1\n')
    assert HelperFunctions.PerformTransmissions(endnode, runner) == (True, True)
    # Verify that forwarding frames from certain MAC addresses works
    dut.write(f"switch set macstatbl \"0 {runner.get_interface_mac_address('enp3s0')} 100 E-- 0\"")
    dut.write(f"switch set macstatbl \"1 {endnode.get_interface_mac_address('enp3s0')}  100 E-- 0\"")
    dut.write('switch show macstatbl')
    time.sleep(0.25)
    assert HelperFunctions.PerformTransmissions(endnode, runner) == (False, False)
    dut.expect(rf"Received data on HOST eth from {runner.get_interface_mac_address('enp3s0')}")
    dut.expect(rf"Received data on HOST eth from {endnode.get_interface_mac_address('enp3s0')}")
