# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""
Target test using EthTestRunner from eth_test_app component.
"""

import pytest

from pytest_embedded import Dut

# W6100 hardware is not yet available in CI; skip the whole module at collection
# time so the dynamic pipeline does not schedule a stuck job. Remove this skip
# once a CI runner with the `eth_w6100` tag exists.
pytest.skip('W6100 hardware not yet available in CI', allow_module_level=True)

TEST_IF = ''


@pytest.mark.parametrize(
    'config, target',
    [
        pytest.param('default_w6100', 'esp32', marks=[pytest.mark.eth_w6100]),
        pytest.param('poll_w6100', 'esp32', marks=[pytest.mark.eth_w6100]),
    ],
    indirect=['target'],
)
def test_eth_w6100(dut: Dut, eth_test_runner) -> None:
    eth_test_runner.run_ethernet_test_apps(dut)
    dut.serial.hard_reset()
    eth_test_runner.run_ethernet_l2_test(dut, TEST_IF)
    dut.serial.hard_reset()
    eth_test_runner.run_ethernet_heap_alloc_test(dut, TEST_IF)
