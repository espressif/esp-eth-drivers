# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""
Conftest for the phy_tester.
"""
import pytest

from _pytest.fixtures import FixtureRequest


@pytest.fixture(scope='session')
def eth_nic(request: FixtureRequest) -> str:
    """
    Get the network interface (NIC) name option from the command line.
    """
    return request.config.getoption('eth_nic') or ''

def pytest_addoption(parser: pytest.Parser) -> None:
    """
    Add a command line option to specify the network interface (NIC) name the DUT is connected to.
    """
    idf_group = parser.getgroup('idf')
    idf_group.addoption(
        '--eth-nic',
        help='Network interface (NIC) name the DUT is connected to',
    )
