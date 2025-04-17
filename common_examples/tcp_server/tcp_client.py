#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

"""
This script is a simple TCP client that connects to a server and sends data.
"""

import argparse
import logging
import signal
import socket
import sys
import time

# Define client port
SOCKET_PORT = 8080  # Fixed to match the server port


def parse_arguments() -> argparse.Namespace:
    """Parse command line arguments."""
    parser = argparse.ArgumentParser(
        description='Establish TCP connection to a server using Berkeley sockets and periodically send transmission expecting echo',
        epilog='Part of the tcp_server example for esp_eth_drivers'
    )
    parser.add_argument('ip', help='IP address to connect to')
    parser.add_argument('-i', '--interval', type=int, default=1000, help='Interval between each transmission (ms)')
    parser.add_argument('-c', '--count', type=int, help='How many transmissions to perform (default: no limit)')
    parser.add_argument('-s', '--silent', action='store_true', help='Do not log transmissions, just print out the result, automatically limits count to 10')
    return parser.parse_args()


def setup_logging(silent: bool) -> logging.Logger:
    """Configure logging for the application."""
    new_logger = logging.getLogger('tcp_client')
    logging.basicConfig(format='%(name)s :: %(levelname)-8s :: %(message)s', level=logging.DEBUG)
    if silent:
        new_logger.setLevel(logging.INFO)
    return new_logger


def run_client(ip: str, port: int, period_ms: int, count: int, logger: logging.Logger) -> None:
    """Initialize and run the TCP client."""
    logger.info('Transmitting to %s:%d every %d ms', ip, port, period_ms)

    # init socket connection
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(10)

    try:
        sock.connect((ip, port))
    except TimeoutError:
        logger.error('Couldn\'t establish connection due to timeout')
        sys.exit(1)
    except Exception as e:  # pylint: disable=broad-exception-caught
        logger.error('Connection error: %s', str(e))
        sys.exit(1)

    # do transmissions
    counter = 1
    receives = 0
    timeouts = 0

    try:
        while count is None or counter <= count:
            msg = f'Transmission {counter}: Hello from Python TCP Client!\n'
            logger.debug('Transmitting: %s', msg)
            sock.sendall(str.encode(msg))
            try:
                data = sock.recv(128)
                receives += 1
                logger.debug('Received: %s', data.decode())
            except TimeoutError:
                logger.error('Timeout, no echo received')
                timeouts += 1
            counter += 1
            time.sleep(period_ms * 0.001)
    except KeyboardInterrupt:
        logger.info('Client shutting down')
    except Exception as e:  # pylint: disable=broad-exception-caught
        logger.error('Client error: %s', str(e))
    finally:
        sock.close()

    if count is not None:
        success_rate = (receives / count) * 100.0 if count > 0 else 0
        logger.info('Performed %d transmissions, received %d replies with %d timeouts (%.2f%% success rate)',
                   count, receives, timeouts, success_rate)


if __name__ == '__main__':
    # Setup signal handler for graceful exit
    signal.signal(signal.SIGINT, lambda s, f: sys.exit(0))

    # Parse arguments and setup
    args = parse_arguments()

    # Set default count if silent mode is enabled
    if args.silent and args.count is None:
        args.count = 10

    local_logger = setup_logging(args.silent)

    # Run the client
    run_client(args.ip, SOCKET_PORT, args.interval, args.count, local_logger)
