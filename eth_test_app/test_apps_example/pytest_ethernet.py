# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""
Target test example using EthTestRunner from eth_test_app component.
"""

import pytest

from pytest_embedded import Dut

# -----------------------------------------------------------------------------
# CONFIGURE FOR YOUR CHIP (required when copying this file to a driver test app)
# -----------------------------------------------------------------------------
# 1. TEST_IF: host Ethernet interface for L2/heap tests (find with: ip -c a).
#    Leave empty to use default interface.
TEST_IF = ''

# 2. config / marks / target: set the sdkconfig preset and PHY marker for your driver runner
#    (e.g. pytest.mark.eth_dm9051, pytest.mark.eth_ip101).
@pytest.mark.parametrize(
    'config, target',
    [
        pytest.param('default_generic', 'esp32', marks=[pytest.mark.eth_ip101]),
    ],
    indirect=['target'],
)
def test_eth_example(dut: Dut, eth_test_runner) -> None:
    eth_test_runner.run_ethernet_test_apps(dut)
    dut.serial.hard_reset()
    eth_test_runner.run_ethernet_l2_test(dut, TEST_IF)
    dut.serial.hard_reset()
    eth_test_runner.run_ethernet_heap_alloc_test(dut, TEST_IF)
