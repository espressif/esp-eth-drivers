# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
import logging
import os
import sys
import time

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

def perform_broadcast_test(dut, endnode, runner):
    # Broadcast from the Endnode
    dut.write('hosteth -u rx')
    runner.execute_async('python3 -u vm_test_app.py rx ""')
    endnode.execute('python3 -u vm_test_app.py bcast')
    runner_output = runner.wait_until_process_finish()
    e2r_bcast_success = 'Broadcast' in runner_output
    e2h_bcast_success = True
    # Broadcast from the Runner
    dut.write('hosteth -u rx')
    endnode.execute_async('python3 -u vm_test_app.py rx ""')
    runner.execute('python3 -u vm_test_app.py bcast')
    endnode_output = endnode.wait_until_process_finish()
    r2e_bcast_success = 'Broadcast' in endnode_output
    r2h_bcast_success = True
    # Broadcast from Host Port on ESP32
    h2e_bcast_success = True
    h2r_bcast_success = True

    return ([e2r_bcast_success, e2h_bcast_success], [r2e_bcast_success, r2h_bcast_success], [h2e_bcast_success, h2r_bcast_success])

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
    dut.write('switch -p 1 set tailtag 0\n')
    dut.write('switch -p 2 set tailtag 0\n')
    # Trigger endnode re-requesting IP address from the runner
    endnode.execute('sudo -n ip link set dev enp3s0 down')
    endnode.execute('sudo -n ip link set dev enp3s0 up')
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
    assert HelperFunctions.PerformTransmissionTest(endnode, runner) == (False, False)
    dut.write('switch -p 1 set tx 1\n')
    dut.write('switch -p 1 set rx 0\n')
    dut.write('switch -p 2 set tx 0\n')
    dut.write('switch -p 2 set rx 1\n')
    assert HelperFunctions.PerformTransmissionTest(endnode, runner) == (True, False)
    dut.write('switch -p 1 set tx 0\n')
    dut.write('switch -p 1 set rx 1\n')
    dut.write('switch -p 2 set tx 1\n')
    dut.write('switch -p 2 set rx 0\n')
    assert HelperFunctions.PerformTransmissionTest(endnode, runner) == (False, True)
    dut.write('switch -p 1 set tx 1\n')
    dut.write('switch -p 1 set rx 1\n')
    dut.write('switch -p 2 set tx 1\n')
    dut.write('switch -p 2 set rx 1\n')
    assert HelperFunctions.PerformTransmissionTest(endnode, runner) == (True, True)
    # Verify that forwarding frames from certain MAC addresses works
    # First we check that each device can broadcast and receives all other's broadcasts
    #assert perform_broadcast_test(dut, endnode, runner) == (True, True, True)
    # Verify that modifying Static MAC Table reroutes packets
    # The syntax of Static MAC Table entry
    #          "2 00:11:22:33:44:55 101 EOF 0"
    # Entry # --|         |         ||| ||| |-- Filter ID
    # MAC ----------------|         ||| |||
    # Forward to Host (0xx/1xx) ----||| |||---- Set the flag to use filter ID
    # Forward to Port 1 (x0x/x1x) ---|| ||----- Set the flag to override port's TX disable/RX disable settings
    # Forward to Port 2 (xx0/xx1) ----| |------ Set the flag to indicate the entry is valid
    #
    # | Case # | Ruleset                 | Endnode Broadcast | Runner Broadcast | ESP Broadcast |
    # |   1    | Default configuration   |      Succeeds     |     Succeeds     |    Succeeds   |
    # |   2    | Runner's MAC 100 E-- 0  |      Succeeds     |     Succeeds     |    Succeeds   |
    # |   3    | Runner's MAC 100 E-- 0  |      Succeeds     |     Succeeds     |    Succeeds   |
    # |        | Endnode's MAC 100 E-- 0 |
    # |   4    | Endnode's MAC 100 E-- 0 |      Succeeds     |     Succeeds     |    Succeeds   |

    #dut.write(f"switch set macstatbl \"0 {runner.get_interface_mac_address('enp3s0')} 000 E-- 0\"")
    #dut.write('switch show macstatbl')
    #time.sleep(0.5)
    #logging.warning(perform_broadcast_test(dut, endnode, runner))

    #dut.write(f"switch set macstatbl \"1 {endnode.get_interface_mac_address('enp3s0')} 000 E-- 0\"")
    dut.write("switch set macstatbl \"0 ff:ff:ff:ff:ff:ff 000 E-- 0\"")
    time.sleep(0.5)
    dut.write('switch show macstatbl')
    logging.warning(perform_broadcast_test(dut, endnode, runner))
    dut.write("switch set macstatbl \"0 ff:ff:ff:ff:ff:ff 100 E-- 0\"")
    time.sleep(0.5)
    dut.write('switch show macstatbl')
    logging.warning(perform_broadcast_test(dut, endnode, runner))
    dut.write("switch set macstatbl \"0 ff:ff:ff:ff:ff:ff 010 E-- 0\"")
    time.sleep(0.5)
    dut.write('switch show macstatbl')
    logging.warning(perform_broadcast_test(dut, endnode, runner))
    dut.write("switch set macstatbl \"0 ff:ff:ff:ff:ff:ff 001 E-- 0\"")
    time.sleep(0.5)
    dut.write('switch show macstatbl')
    logging.warning(perform_broadcast_test(dut, endnode, runner))
    dut.write("switch set macstatbl \"0 ff:ff:ff:ff:ff:ff 111 E-- 0\"")
    time.sleep(0.5)
    dut.write('switch show macstatbl')
    logging.warning(perform_broadcast_test(dut, endnode, runner))

    #logging.warning(perform_broadcast_test(dut, endnode, runner))

    #dut.write(f"switch set macstatbl \"0 {runner.get_interface_mac_address('enp3s0')} 100 --- 0\"")
    #dut.write('switch show macstatbl')
    #dut.write(f"switch set macstatbl \"1 {endnode.get_interface_mac_address('enp3s0')}  100 E-- 0\"")
    #dut.write('switch show macstatbl')
    #time.sleep(0.25)
    #assert HelperFunctions.PerformTransmissions(endnode, runner) == (False, False)
    #dut.expect(rf"Received data on HOST eth from {runner.get_interface_mac_address('enp3s0')}")
    #dut.expect(rf"Received data on HOST eth from {endnode.get_interface_mac_address('enp3s0')}")
