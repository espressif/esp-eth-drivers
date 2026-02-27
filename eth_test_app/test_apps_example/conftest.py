# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""
Conftest for driver test apps that use `eth_test_app` component.

Copy this file to your test app directory as conftest.py (e.g. dm9051/test_apps/conftest.py).
Adds the eth_test_app component directory to sys.path so EthTestRunner can be imported directly.
"""
import sys

from pathlib import Path

import pytest

_root = Path(__file__).resolve().parent
for _name in ('espressif__eth_test_app', 'eth_test_app'):
    _p = _root / 'managed_components' / _name
    if (_p / 'eth_test_runner.py').exists():
        sys.path.insert(0, str(_p))
        break
else:
    for _d in (_root.parent.parent / 'eth_test_app', _root.parent):
        if (_d / 'eth_test_runner.py').exists():
            sys.path.insert(0, str(_d))
            break

from eth_test_runner import EthTestRunner  # noqa: E402


@pytest.fixture
def eth_test_runner() -> EthTestRunner:
    return EthTestRunner()
