# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
import argparse
import socket
import sys
import time

TIMEOUT_SECONDS = 5

parser = argparse.ArgumentParser(prog='VMtestApp', usage='%(prog)s [options]')
parser.add_argument('-n', help='Number of packets to send', default=5)
parser.add_argument('direction', choices=['tx', 'rx', 'bcast'], help='Direction of the data flow')
parser.add_argument('target', help='Destination IP', nargs='?')
args = parser.parse_args()

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)

if args.direction == 'tx':
    packets_to_send = int(args.n)
    for i in range(packets_to_send):
        sock.sendto(b'Transmission test', (args.target, 54321))
        print(f'Sent {i+1}/{packets_to_send} packets')
        time.sleep(0.5)
elif args.direction == 'rx':
    sock.bind((args.target, 54321))
    sock.settimeout(TIMEOUT_SECONDS)
    print(f'Listening on {args.target}')
    try:
        data, addr = sock.recvfrom(1024)
        print(f"<-- \"{data.decode('ascii')}\"")
    except TimeoutError:
        print(f'No new packets in {TIMEOUT_SECONDS} second(s). Stopping.')
        sys.exit(-1)
elif args.direction == 'bcast':
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    packets_to_send = int(args.n)
    for i in range(packets_to_send):
        sock.sendto(b'Broadcast test', ('120.140.1.255',54321))
        print(f'Sent {i+1}/{packets_to_send} packets')
        time.sleep(0.5)

sock.close()
