#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

"""
This script is a simple TCP server that listens for incoming connections on a specified port.
It handles client connections, receives data, and sends a response back to the client.
"""

import argparse
import logging
import signal
import socket
import sys

# Define server port
SOCKET_PORT = 8080  # Fixed to match the bind port


def parse_arguments() -> argparse.Namespace:
    """Parse command line arguments."""
    parser = argparse.ArgumentParser(
        description='Serve TCP connection using Berkeley sockets and wait for connections',
        epilog='Part of the tcp_client example for esp_eth_drivers'
    )
    parser.add_argument('ip', help='IP address to listen on')
    return parser.parse_args()


def setup_logging() -> logging.Logger:
    """Configure logging for the application."""
    new_logger = logging.getLogger('tcp_server')
    logging.basicConfig(format='%(name)s :: %(levelname)-8s :: %(message)s', level=logging.DEBUG)
    return new_logger


def handle_client_connection(conn: socket.socket, address: tuple[str, int], logger: logging.Logger, counter: list[int]) -> None:
    """Handle communication with a connected client."""

    while True:
        try:
            data = conn.recv(128).decode()
            if not data:
                logger.info('Connection closed by client (no data)')
                break
            logger.info('Received %i bytes from %s:%d', len(data), address[0], address[1])
            logger.info('Received: %s', data)
            msg = f'Transmission {counter[0]}: Hello from Python TCP Server\n'
            logger.info('Transmitting: %s', msg)
            conn.sendall(str.encode(msg))
            counter[0] += 1

        except ConnectionAbortedError:
            logger.info('Connection closed by client')
            break
        except Exception as e:  # pylint: disable=broad-exception-caught
            logger.error('Error handling client: %s', str(e))
            break


def run_server(ip: str, port: int, logger: logging.Logger) -> None:
    """Initialize and run the TCP server."""
    # Setup server socket
    server_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

    try:
        server_sock.bind((ip, port))
        server_sock.listen(1) #  only one connection at a time
        logger.info('Listening on %s:%d', ip, port)

        # Use a list for counter to make it mutable across function calls
        counter = [1]

        while True:
            conn, address = server_sock.accept()
            try:
                handle_client_connection(conn, address, logger, counter)
            finally:
                conn.close()

    except KeyboardInterrupt:
        logger.info('Server shutting down')
    except Exception as e:  # pylint: disable=broad-exception-caught
        logger.error('Server error: %s', str(e))
    finally:
        server_sock.close()


if __name__ == '__main__':
    # Setup signal handler for graceful exit
    signal.signal(signal.SIGINT, lambda s, f: sys.exit(0))

    # Parse arguments and setup
    args = parse_arguments()
    local_logger = setup_logging()

    # Run the server
    run_server(args.ip, SOCKET_PORT, local_logger)
