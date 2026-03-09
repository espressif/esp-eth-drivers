# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""
Target test using EthTestRunner from eth_test_app component.
"""

import logging
import sys

from pathlib import Path

import pytest

from pytest_embedded import Dut

TEST_IF = ''


def _is_idf_54() -> bool:
    """True if current IDF version is 5.4.x (for xfail condition in CI where ESP_IDF_VERSION may be unset)."""
    # Use the interpreter actually running pytest (e.g. .../idf5.4_py3.9_env/bin/python) -> .../idf_version.txt
    env_dir = Path(sys.prefix)
    idf_version_file = env_dir / 'idf_version.txt'
    logging.info(f'IDF version file: {idf_version_file}')
    if not idf_version_file.exists():
        return False
    try:
        content = idf_version_file.read_text().strip()
        logging.info(f'IDF version file content: {content}')
        return content.startswith('v5.4') or content.startswith('5.4')
    except (OSError, ValueError):
        return False


@pytest.mark.parametrize(
    'config, target',
    [
        pytest.param('default_generic', 'esp32', marks=[pytest.mark.eth_ip101]),
        pytest.param('default_generic_esp32p4', 'esp32p4', marks=[pytest.mark.eth_ip101, pytest.mark.rev_default]),
    ],
    indirect=['target'],
)
@pytest.mark.xfail(
    condition=_is_idf_54(),
    reason='Fails on ESP32P4 with IDF 5.4 since IDF 5.4 only supports older chip revision'
)
def test_eth_ip101(dut: Dut, eth_test_runner) -> None:
    eth_test_runner.run_ethernet_test_apps(dut)
    dut.serial.hard_reset()
    eth_test_runner.run_ethernet_l2_test(dut, TEST_IF)
    dut.serial.hard_reset()
    eth_test_runner.run_ethernet_heap_alloc_test(dut, TEST_IF)
