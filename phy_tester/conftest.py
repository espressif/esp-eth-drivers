import pytest
from _pytest.fixtures import FixtureRequest

@pytest.fixture(scope='session')
def eth_nic(request: FixtureRequest) -> str:
    return request.config.getoption('eth_nic') or ''

def pytest_addoption(parser: pytest.Parser) -> None:
    idf_group = parser.getgroup('idf')
    idf_group.addoption(
        '--eth-nic',
        help='Network interface (NIC) name the DUT is connected to',
    )
