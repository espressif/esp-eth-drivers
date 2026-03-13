# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""
Target test using EthTestRunner from eth_test_app component.
"""

import pytest

from pytest_embedded import Dut

TEST_IF = ''


@pytest.mark.parametrize(
    'config, target',
    [
        pytest.param('default_ch390', 'esp32', marks=[pytest.mark.eth_ch390]),
        pytest.param('poll_ch390', 'esp32', marks=[pytest.mark.eth_ch390]),
    ],
    indirect=['target'],
)
def test_eth_ch390(dut: Dut, eth_test_runner) -> None:
    eth_test_runner.run_ethernet_test_apps(dut)
    dut.serial.hard_reset()
    eth_test_runner.run_ethernet_l2_test(dut, TEST_IF)
    dut.serial.hard_reset()
    eth_test_runner.run_ethernet_heap_alloc_test(dut, TEST_IF)
