import socket
import argparse
import pickle
import time
import sys

TIMEOUT_SECONDS = 5

parser = argparse.ArgumentParser(prog="VMtestApp", usage="%(prog)s [options]")
parser.add_argument("-n", help='Number of packets to send', default=5)
parser.add_argument("direction", choices=['tx', 'rx'], help='Direction of the data flow')
parser.add_argument("target", help='Destination IP')
args = parser.parse_args()

print(f"About to {'send' if (args.direction == 'tx') else 'receive'} {args.n} packets {'to' if (args.direction == 'tx') else 'from'} {args.target}")

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

if args.direction == "tx":
    packets_to_send = int(args.n)
    for i in range(packets_to_send):
        packet = pickle.dumps({"current": i+1, "total": packets_to_send})
        sock.sendto(packet, (args.target, 12345))
        print(f"Sent {i+1}/{packets_to_send} packets")
        time.sleep(0.25)
elif args.direction == "rx":
    sock.bind((args.target, 12345))
    sock.settimeout(TIMEOUT_SECONDS)
    print(f"Listening on {args.target}")
    time_last_packet = time.time()
    packet_cnt = 0
    while(True):
        try:
            data, addr = sock.recvfrom(1024)
            packet_recon = pickle.loads(data)
            print(f"Reconstructed packet {packet_recon['current']}/{packet_recon['total']} from {addr[0]}")
            packet_cnt += 1
            if packet_recon['current'] == packet_recon['total']:
                sys.exit(packet_cnt)
            time_last_packet = time.time()
        except TimeoutError:
            print(f"No new packets in {TIMEOUT_SECONDS} seconds. Stopping.")
            sys.exit(packet_cnt)
