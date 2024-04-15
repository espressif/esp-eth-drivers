#!/usr/bin/env python3
import socket
import argparse
import logging
import time
import signal

parser = argparse.ArgumentParser(description='Establish TCP connection to a server using Berkley sockets and periodically send transmission expecting echo', epilog='Part of the tcp_server example for esp_eth_drivers')
parser.add_argument('ip')
parser.add_argument('-t', '--time', type=int, default=500, help='Period of transmission (ms)')
parser.add_argument('-c', '--count', type=int, help='How many transmissions to perform (default: no limit)')
parser.add_argument('-s', '--silent', action='store_true', help="Do not log transmissions, just print out the result, automatically limits count to 10")
args = parser.parse_args()

SOCKET_PORT = 5000

# setup sigint handler
signal.signal(signal.SIGINT, lambda s, f : exit(0))

logger = logging.getLogger("tcp_client")
logging.basicConfig(format="%(name)s :: %(levelname)-8s :: %(message)s", level=logging.DEBUG)
if args.silent:
    logger.setLevel(logging.INFO)
    args.count = 10
logger.info("Transmitting to %s:%d every %d ms", args.ip, SOCKET_PORT, args.time)

# init socket connection
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM) 
sock.settimeout(10)
try:
    sock.connect((args.ip, SOCKET_PORT))
except TimeoutError:
    logger.error("Couldn't establish connection due to timeout")
    exit(1)
# do transmissions
counter = 1
receives = 0
timeouts = 0
while args.count == None or counter <= args.count:
    msg = f"Transmission {counter}: Hello from Python"
    logger.debug("Transmitting: \"%s\"", msg)
    sock.sendall(str.encode(msg))
    try:
        data = sock.recv(128)
        receives+=1
    except TimeoutError:
        logger.error("Timeout, no echo received")
        timeouts+=1
    logger.debug("Received: \"%s\"", data.decode())
    counter+=1
    time.sleep(args.time * 0.001)
sock.close()
logger.info("Performed %d transmissions, received %d replies with %d timeouts (%.2f%% success rate)", args.count, receives, timeouts, (1 - timeouts/receives)*100.0)