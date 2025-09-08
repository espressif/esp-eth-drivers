# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""
Target test of simple-ethernet example.
"""
import platform
import subprocess

import pytest

from pytest_embedded import Dut
from pytest_embedded_idf.utils import idf_parametrize


@pytest.mark.parametrize(
    'config',
    [
        pytest.param('ip101', marks=[pytest.mark.eth_ip101]),
        pytest.param('dm9051', marks=[pytest.mark.eth_dm9051]),
    ],
    indirect=True,
)
@idf_parametrize('target', ['esp32'], indirect=['target'])
def test_esp_eth_simple(dut: Dut) -> None:
    """
    Test basic Ethernet functionality on ESP32 SoCs with different PHY configurations.

    This test waits for the device under test (DUT) to obtain an IP address via Ethernet,
    then pings the DUT from the host to verify network connectivity.

    Parameters:
        dut (Dut): The device under test, provided by pytest_embedded.
    """
    # wait for ip received
    dut_ip = dut.expect(r'esp_netif_handlers: .+ ip: (\d+\.\d+\.\d+\.\d+),').group(1)
    # ping it once
    param = '-n' if platform.system().lower() == 'windows' else '-c'
    command = ['ping', param, '1', dut_ip]
    output = subprocess.run(command, capture_output=True, check=False)  # noqa: S603 known false positive (https://github.com/astral-sh/ruff/issues/4045)
    if 'unreachable' in str(output.stdout):
        raise RuntimeError('Host unreachable')
