import pytest
from pytest_embedded import Dut
from pytest_embedded_idf.utils import idf_parametrize
import paramiko  # type: ignore
import logging
import os
import time
import re

IP_ADDRESS_RUNNER = "10.10.1.2"
IP_ADDRESS_ENDNODE = "10.10.1.3"
SSH_PORT = 22
SSH_USERNAME = "rob"
SSH_PASSWORD = "123"

runner = paramiko.client.SSHClient()
endnode = paramiko.client.SSHClient()
runner.set_missing_host_key_policy(paramiko.AutoAddPolicy())
endnode.set_missing_host_key_policy(paramiko.AutoAddPolicy())

def ksz8863_bring_ports(dut, start):
    dut.write(f"bring --port 1 {"up" if start else "down"}")
    dut.expect("ksz8863>")
    dut.write(f"bring --port 2 {"up" if start else "down"}")
    dut.expect("ksz8863>")

def ksz8863_set_port_direction(dut, rx1, tx1, rx2, tx2):
    dut.write(f"set --port 1 tx {"1" if rx1 else "0"}")
    dut.expect("ksz8863>")
    dut.write(f"set --port 1 rx {"1" if tx1 else "0"}")
    dut.expect("ksz8863>")
    dut.write(f"set --port 2 tx {"1" if rx2 else "0"}")
    dut.expect("ksz8863>")
    dut.write(f"set --port 2 rx {"1" if tx2 else "0"}")
    dut.expect("ksz8863>")

def parse_endnode_ip():
    find_ip_regex = re.compile(r"inet ([0-9.]+)\/")
    stdin_, stdout_, stderr_ = endnode.exec_command(f"ip -f inet addr show enp3s0")
    output = stdout_.read().decode('ascii').strip("\n")
    res = find_ip_regex.search(output).groups()[0]
    logging.info(f"Found an IP address of ENDNODE VM: {res}")
    return res

def attempt_transmissions(runner, endnode, endnode_ip):
    # Attempt runner to endnode transmission
    stdin_r, stdout_r, stderr_r = runner.exec_command(f"python3 -u vm_test_app.py rx 120.140.1.1")
    stdin_e, stdout_e, stderr_e = endnode.exec_command(f"python3 -u vm_test_app.py tx 120.140.1.1")
    #e2r_success = runner.recv_exit_status() > 0
    e2r_success = "Reconstructed packet" in stdout_r.read().decode('ascii').strip("\n")
    # Attempt endnode to runner transmisstion
    stdin_e, stdout_e, stderr_e = endnode.exec_command(f"python3 -u vm_test_app.py rx {endnode_ip}")
    stdin_r, stdout_r, stderr_r = runner.exec_command(f"python3 -u vm_test_app.py tx {endnode_ip}")
    r2e_success = "Reconstructed packet" in stdout_e.read().decode('ascii').strip("\n")
    #r2e_success = endnode.recv_exit_status() > 0
    return (e2r_success, r2e_success)

def test_esp_eth_bridge(dut: Dut) -> None:
    # Wait until KSZ8863 inits
    dut.expect("ksz8863>")
    logging.info("Initialization complete")
    # Connect to virtual machines
    runner.connect(IP_ADDRESS_RUNNER, username=SSH_USERNAME, password=SSH_PASSWORD)
    endnode.connect(IP_ADDRESS_ENDNODE, username=SSH_USERNAME, password=SSH_PASSWORD)
    runner_sftp = runner.open_sftp()
    endnode_sftp = endnode.open_sftp()
    # Put vm_test_app.py to both VMs
    runner_sftp.put("vm_test_app.py", "vm_test_app.py")
    endnode_sftp.put("vm_test_app.py", "vm_test_app.py")
    # Get endnode IP
    endnode_ip = parse_endnode_ip()
    # Run test cases

    ksz8863_set_port_direction(dut, False, False, False, False)
    ksz8863_bring_ports(dut, True)
    e2r, r2e = attempt_transmissions(runner, endnode, endnode_ip)
    logging.error(f"Test case 4: {e2r} {r2e}")
    assert e2r == False and r2e == False

    ksz8863_set_port_direction(dut, True, False, False, True)
    ksz8863_bring_ports(dut, True)
    e2r, r2e = attempt_transmissions(runner, endnode, endnode_ip)
    logging.error(f"Test case 4: {e2r} {r2e}")
    assert e2r == True and r2e == False

    ksz8863_set_port_direction(dut, False, True, True, False)
    ksz8863_bring_ports(dut, True)
    e2r, r2e = attempt_transmissions(runner, endnode, endnode_ip)
    logging.error(f"Test case 4: {e2r} {r2e}")
    assert e2r == False and r2e == True

    ksz8863_set_port_direction(dut, True, True, True, True)
    ksz8863_bring_ports(dut, True)
    e2r, r2e = attempt_transmissions(runner, endnode, endnode_ip)
    logging.error(f"Test case 4: {e2r} {r2e}")
    assert e2r == True and r2e == True