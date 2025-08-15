# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
import logging
import re
import threading
import time

import paramiko  # type: ignore


class RemoteMachineSSH:
    # Properties
    tag: str
    sshc: paramiko.SSHClient
    process_output: str
    process_running: bool
    # Methods
    def connect(self, ip, username, password, port=22):
        self.sshc.connect(ip, username=username, password=password, port=port)

    def close(self):
        self.sshc.close()

    def put(self, filepath, remotepath=None):
        if remotepath is None:
            remotepath = filepath
        sftpc = self.sshc.open_sftp()
        sftpc.put(filepath, remotepath)
        sftpc.close()

    def execute(self, command):
        logging.info(f"{self.tag}::running:\"{command}\"")
        stdin_, stdout_, stderr_ = self.sshc.exec_command(command)
        return stdout_.read().decode('ascii').strip('\n')

    def _priv_execute_thread(self, command):
        self.process_output = self.execute(command)
        print(f"{self.process_output}")
        self.process_running = False

    def execute_async(self, command):
        self.process_running = True
        self.process_output = ''
        thread = threading.Thread(target=self._priv_execute_thread, args=(command,))
        thread.start()

    def wait_until_process_finish(self):
        while self.process_running:
            time.sleep(0.1)
        tmp = self.process_output
        self.process_output = ''
        return tmp

    def __init__(self, tag='RemoteMachine'):
        self.tag = tag
        self.sshc = paramiko.client.SSHClient()
        self.sshc.set_missing_host_key_policy(paramiko.AutoAddPolicy())


class VirtualMachineSSH(RemoteMachineSSH):
    def get_interface_ip(self, interface):
        find_ip_regex = re.compile(r'inet ([0-9.]+)\/')

        output = self.execute(f"ip -f inet addr show {interface}")
        res = find_ip_regex.search(output).groups()
        if res is None:
            # Interface doesn't have an IP address
            logging.warning(f"{self.tag}: The interface {interface} doesn't have an IP address assigned to it")
            return None
        else:
            return res[0]

    def get_interface_mac_address(self, interface):
        find_ip_regex = re.compile(r'ether ([0-9a-f]{2}:[0-9a-f]{2}:[0-9a-f]{2}:[0-9a-f]{2}:[0-9a-f]{2}:[0-9a-f]{2})')
        output = self.execute(f"ip -f link addr show {interface}")
        return find_ip_regex.search(output).groups()[0]

    def get_all_interfaces_ips(self):
        # Get list of network interfaces
        sys_class_net = self.execute('ls /sys/class/net/')
        interfaces = sys_class_net.split(' ')
        interfaces.remove('lo') # Remove loopback interface because it doesn't matter

        interface_ip_dict = {}
        for interface in interfaces:
            interface_ip_dict[interface] = self.get_interface_ip(interface)
        return interface_ip_dict

    def __init__(self, tag='VM'):
        super().__init__(tag)


class SwitchSSH(RemoteMachineSSH):
    def bring_port(self, port, state):
        code = None
        if state == 'up':
            code = '0x1000'
        elif state == 'down':
            code = '0x800'
        else:
            logging.error(f"{self.tag}: Attempting to set incorrect state \"{state}\" for port #{port}. Nothing will be done for this command.")
            return
        self.execute(f"/usr/bin/tswconf debug phy set {str(port - 1)} 0 {code}")

    def __init__(self, tag='Switch'):
        super().__init__(tag)


class HelperFunctionsClass:
    # Methods
    def perform_transmission_test(self, runner, endnode):
        endnode_ip = endnode.get_interface_ip('enp3s0')
        runner_ip = runner.get_interface_ip('enp3s0')
        # Attempt runner to endnode transmission
        endnode.execute_async('timeout 5 socat - udp-listen:12345')
        runner.execute(f'for i in {{1..5}}; do echo "Transmission"; sleep 0.75; done | socat - udp:{endnode_ip}:12345,sp=12345')
        endnode_output = endnode.wait_until_process_finish()
        r2e_success = 'Transmission' in endnode_output

        runner.execute_async('timeout 5 socat - udp-listen:12345')
        endnode.execute(f'for i in {{1..5}}; do echo "Transmission"; sleep 0.75; done | socat - udp:{runner_ip}:12345,sp=12345')
        runner_output = runner.wait_until_process_finish()
        e2r_success = 'Transmission' in runner_output

        return (r2e_success, e2r_success)

    def perform_l2_broadcast_async(self, vm, interface, eth_type, payload):
        vm_mac = vm.get_interface_mac_address(interface).split(':')
        # Convert human-readable 11:22:33:44:55:66 to \x11\x22\x33\x44\x55\x66
        source_mac = f'\\x{vm_mac[0]}\\x{vm_mac[1]}\\x{vm_mac[2]}\\x{vm_mac[3]}\\x{vm_mac[4]}\\x{vm_mac[5]}'
        # Construct the packet we will be sending to ff:ff:ff:ff:ff:ff
        packet = '\\xff\\xff\\xff\\xff\\xff\\xff' + source_mac + f'\\x{hex(eth_type)[2:4]}\\x{hex(eth_type)[4:]}' + payload

        # Construct sockaddr for socat SOCKET-SENDTO
        # Get ifindex of the interface we are boradcasting from, convert it to big endian and hexadecimal representation
        interface_ifindex = int(vm.execute(f'cat /sys/class/net/{interface}/ifindex'))
        interface_ifindex_be = interface_ifindex.to_bytes(4, 'little').hex()
        # Build the string that will be supplied to socat
        sockaddr_string = f'x0300x{interface_ifindex_be}x0000x00x06xffffffffffff0000'
        vm.execute_async(f'for i in {{1..5}}; do echo -e -n \"{packet}\"; sleep 0.75; done | sudo socat - SOCKET-SENDTO:17:3:0:{sockaddr_string}')


    def __init__(self):
        pass

HelperFunctions = HelperFunctionsClass()
