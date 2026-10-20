# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Conftest for the LAN86xx bridge three-board test."""

from __future__ import annotations

import logging
import os

import pytest

from pytest_embedded.plugin import multi_dut_fixture

# Bridge on ttyUSB0 - clients on ttyUSB1/ttyUSB2 (BrnoVM0420 job nodes).
DEFAULT_SERIAL_PORTS = '/dev/ttyUSB0|/dev/ttyUSB1|/dev/ttyUSB2'
BRIDGE_DUT_COUNT = 3


def _serial_ports_for_test(config: pytest.Config) -> str:
    cli_port = config.getoption('port', default=None)
    if cli_port:
        ports = cli_port.split('|')
        if len(ports) >= BRIDGE_DUT_COUNT:
            return '|'.join(ports[:BRIDGE_DUT_COUNT])
    return os.environ.get('LAN86XX_SERIAL_PORTS', DEFAULT_SERIAL_PORTS)


@pytest.fixture(scope='session')
def lan86xx_serial_ports(request: pytest.FixtureRequest) -> str:
    return _serial_ports_for_test(request.config)


@pytest.fixture
@multi_dut_fixture
def build_dir(
    request: pytest.FixtureRequest,
    app_path: str,
    target: str | None,
    config: str | None,
) -> str:
    """Resolve per-DUT build directory (build_<target>_<config>)."""
    check_dirs: list[str] = []
    build_dir_arg = request.config.getoption('build_dir', default=None)
    if build_dir_arg:
        check_dirs.append(build_dir_arg)
    if target is not None and config is not None:
        check_dirs.append(f'build_{target}_{config}')
    if target is not None:
        check_dirs.append(f'build_{target}')
    if config is not None:
        check_dirs.append(f'build_{config}')
    check_dirs.append('build')

    for check_dir in check_dirs:
        binary_path = os.path.join(app_path, check_dir)
        if os.path.isdir(binary_path):
            logging.info('found valid binary path: %s', binary_path)
            return check_dir

    # No binaries: the bridge test app is only built on IDF >=6.0 (see
    # .build-test-rules.yml). idf-ci does not auto-skip this multi-DUT case, so
    # skip it here where it was not built (e.g. IDF <6.0 target-test images).
    # This is a skip, not an error, so the CI job stays green on those versions.
    pytest.skip(
        f'no build dir found under {app_path}; the LAN86xx bridge firmware is '
        f'only built on IDF >=6.0. To run locally, build first, e.g.: '
        f'idf.py -B {check_dirs[0]} -D SDKCONFIG={check_dirs[0]}/sdkconfig '
        f'-D SDKCONFIG_DEFAULTS="sdkconfig.defaults.esp32;sdkconfig.ci.<role>" build'
    )


_THREE_BOARD_CONFIGS = {
    'test_lan86xx_client1_iperf': 'lan8650_bridge|lan8670_client1|lan8670_client2',
    'test_lan86xx_bridge': 'lan8650_bridge|lan8670_client1|lan8670_client2',
    'test_lan86xx_bridge_iperf_parallel': 'lan8650_bridge|lan8670_client1|lan8670_client2',
    'test_lan86xx_dut_to_dut_iperf': 'lan8650_bridge|lan8670_client1|lan8670_client2',
    'test_lan86xx_plca_multi_ids': 'lan8650_bridge_plca|lan8670_client1_plca|lan8670_client2',
}


def pytest_generate_tests(metafunc: pytest.Metafunc) -> None:
    config_names = _THREE_BOARD_CONFIGS.get(metafunc.definition.name)
    if config_names is None:
        return
    ports = _serial_ports_for_test(metafunc.config)
    metafunc.parametrize(
        'count, config, port',
        [(BRIDGE_DUT_COUNT, config_names, ports)],
        indirect=True,
    )
