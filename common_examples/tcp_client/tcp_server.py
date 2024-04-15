#!/usr/bin/env python3
import socket
import argparse
import logging
import signal

parser = argparse.ArgumentParser(description='Serve TCP connection using Berkley sockets and wait for connections', epilog='Part of the tcp_client example for esp_eth_drivers')
parser.add_argument('ip')
args = parser.parse_args()

SOCKET_PORT = 5000

# setup sigint handler
signal.signal(signal.SIGINT, lambda s, f : exit(0))

logger = logging.getLogger("tcp_server")
logging.basicConfig(format="%(name)s :: %(levelname)-8s :: %(message)s", level=logging.DEBUG)
logger.info("Listening on %s:%d", args.ip, SOCKET_PORT)

# init server
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.bind((args.ip, 5000))
# listen for incoming connections
sock.listen(1)

counter = 1
while True:
    conn, address = sock.accept()
    logger.debug("Accepted connection from %s:%d", address[0], address[1])
    while True:
        try:
            data = conn.recv(128).decode()
        except ConnectionAbortedError:
            logger.info("Connection closed by client")
            break
        logger.debug("Received: \"%s\"", data)
        msg = f"Transmission {counter}: Hello from Python"
        logger.debug("Transmitting: \"%s\"", msg)
        conn.sendall(str.encode(msg))
        counter += 1
