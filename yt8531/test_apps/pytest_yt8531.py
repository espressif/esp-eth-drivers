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


def _is_idf_before_61() -> bool:
    """True if current IDF version is older than 6.1 (for xfail condition in CI where ESP_IDF_VERSION may be unset)."""
    # Use the interpreter actually running pytest (e.g. .../idf5.4_py3.9_env/bin/python) -> .../idf_version.txt
    env_dir = Path(sys.prefix)
    idf_version_file = env_dir / 'idf_version.txt'
    logging.info(f'IDF version file: {idf_version_file}')
    if not idf_version_file.exists():
        return False
    try:
        content = idf_version_file.read_text().strip().lstrip('v')
        logging.info(f'IDF version file content: {content}')
        major, minor, *_ = (int(part) for part in content.split('.'))
        return (major, minor) < (6, 1)
    except (OSError, ValueError):
        return False


@pytest.mark.parametrize(
    'config, target',
    [
        pytest.param('default_yt8531_esp32s31', 'esp32s31', marks=[pytest.mark.eth_yt8531]),
    ],
    indirect=['target'],
)
@pytest.mark.xfail(
    condition=_is_idf_before_61(),
    reason='ESP32-S31 not supported before IDF 6.1',
)
def test_eth_yt8531(dut: Dut, eth_test_runner) -> None:
    eth_test_runner.run_ethernet_test_apps(dut)
    dut.serial.hard_reset()
    eth_test_runner.run_ethernet_l2_test(dut, TEST_IF)
    dut.serial.hard_reset()
    eth_test_runner.run_ethernet_heap_alloc_test(dut, TEST_IF)
