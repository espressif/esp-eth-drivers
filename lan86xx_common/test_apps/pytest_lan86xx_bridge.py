# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""
LAN86xx bridge test app (three-board topology).

Tests:
- test_lan86xx_bridge: ping matrix + PC→client1/client2 iperf (TCP + UDP, sequential)
- test_lan86xx_client1_iperf: client1 iperf only (quick performance check)
- test_lan86xx_bridge_iperf_parallel: PC→both clients iperf concurrently
- test_lan86xx_dut_to_dut_iperf: client2 (iperf client) → client1 (iperf server) over T1S
- test_lan86xx_plca_multi_ids: both clients send UDP to the PC; client1 has four PLCA TOs
"""

from __future__ import annotations

import logging
import re
import shutil
import subprocess
import time

from concurrent.futures import ThreadPoolExecutor
from typing import NamedTuple

import pytest

from pytest_embedded_idf.dut import IdfDut
from pytest_embedded_idf.utils import idf_parametrize

BRIDGE_DUT_COUNT = 3
IP_BIN = shutil.which('ip') or '/usr/sbin/ip'
PING_BIN = shutil.which('ping') or '/usr/bin/ping'
IPERF_BIN = shutil.which('iperf') or '/usr/bin/iperf'
KILLALL_BIN = shutil.which('killall') or '/usr/bin/killall'

IPERF_BW_LIM = 10
IPERF_BW_LIM_PARALLEL = 3  # UDP -b per client
IPERF_TCP_BW_LIM_PARALLEL = 4  # TCP -b per client in parallel
IPERF_INTERVAL = 5  # iperf -t (seconds) and -i 1 reporting interval count
IPERF_PARALLEL_STAGGER_TCP_S = 0.0
IPERF_PARALLEL_STAGGER_UDP_S = 0.0
IPERF_SERVER_GAP_S = 0.4  # gap between DUT server starts (serial console)
IPERF_ABORT_TIMEOUT_S = 4
IPERF_POST_TCP_SETTLE_S = 5  # bridge LAN8650 recovery after heavy TCP
PING_SETTLE_AFTER_DHCP_S = 3  # PLCA/T1S stable before first ICMP after client DHCP
PING_HOST_COUNT = 3
PING_HOST_DEADLINE_S = 2  # per-packet wait (linux ping -W)
PING_DUT_TIMEOUT_S = 20
PING_RETRIES = 3
PING_RETRY_DELAY_S = 2
PING_DUT_OK_MIN_RX = 2
PRECHECK_RETRIES = 2
MIN_TCP_THROUGHPUT_T1S = 3.0            # PC->DUT sequential
MIN_UDP_THROUGHPUT_T1S = 4.0            # PC->DUT sequential
MIN_TCP_THROUGHPUT_T1S_PARALLEL_SUM = 2.5   # both clients combined
MIN_UDP_THROUGHPUT_T1S_PARALLEL_SUM = 3.0   # both clients combined
MIN_THROUGHPUT_T1S_PARALLEL_ALIVE = 0.5     # per client
MIN_TCP_THROUGHPUT_T1S_DUT2DUT = 7.0    # client2->client1
MIN_UDP_THROUGHPUT_T1S_DUT2DUT = 7.5    # client2->client1
IPERF_BW_LIM_DUT2DUT_UDP = 10
PLCA_MULTI_IDS_MIN_RATIO = 1.4
PLCA_MULTI_IDS_MIN_EACH = 1.0
PLCA_MULTI_IDS_HOST_PORT_C1 = 5001
PLCA_MULTI_IDS_HOST_PORT_C2 = 5002

IPERF_SUMMARY_RE = re.compile(
    r'0\.0-\s*[\d.]+\s+sec\s+(?:[\d.]+\s+\w+Bytes\s+)?([\d.]+)\s+Mbits/sec'
)
IPERF_AGGREGATE_RE = re.compile(
    rf'0\.0-\s*{IPERF_INTERVAL}\.0\s+sec\s+(?:[\d.]+\s+\w+Bytes\s+)?([\d.]+)\s+Mbits/sec'
)
HOST_IPERF_SUMMARY_RE = re.compile(
    r'\[\s*\d+\]\s+0\.0+\-[\d.]+\s+sec\s+[\d.]+\s+\w+Bytes\s+([\d.]+)\s+Mbits/sec'
)


_PARALLEL_PEER_COUNT = 2  # parallel iperf test always has exactly 2 clients


class IperfCaseError(Exception):
    """Single PC->DUT iperf case failure (test continues with remaining boards)."""


class Lan86xxBoardIps(NamedTuple):
    host: str
    bridge: str
    client1: str
    client2: str


class _DutEndpoint(NamedTuple):
    """One side of a DUT->DUT iperf run (server or client)."""
    name: str
    dut: IdfDut
    ip: str = ''  # only the server side uses ip; client passes default ''


@pytest.mark.eth_lan86xx
@idf_parametrize('target', ['esp32'], indirect=['target'])
def test_lan86xx_client1_iperf(
    dut: tuple[IdfDut, IdfDut, IdfDut],
    lan86xx_serial_ports: str,
) -> None:
    """PC -> client1 throughput only (TCP + UDP across T1S bridge)."""
    bridge, client1, _, _, _, client1_ip, _ = _setup_boards(dut, lan86xx_serial_ports)

    client1_ip = _precheck_client_reachable(bridge, 'client1', client1, client1_ip)
    _host_ping(client1_ip, 'host->client1')

    iperf_failures: list[str] = []
    for proto in ('tcp', 'udp'):
        try:
            _run_pc_to_dut_iperf('client1', client1, client1_ip, proto)
        except IperfCaseError as err:
            logging.error('%s', err)
            iperf_failures.append(str(err))
        if proto == 'tcp':
            logging.info(
                'Waiting %ds after TCP for bridge/client settle (check dut-0 for lan865x WDT/SPI errors)',
                IPERF_POST_TCP_SETTLE_S,
            )
            time.sleep(IPERF_POST_TCP_SETTLE_S)

    if iperf_failures:
        pytest.fail('client1 iPerf failures:\n' + '\n'.join(f'  - {msg}' for msg in iperf_failures))


@pytest.mark.eth_lan86xx
@idf_parametrize('target', ['esp32'], indirect=['target'])
def test_lan86xx_bridge(
    dut: tuple[IdfDut, IdfDut, IdfDut],
    lan86xx_serial_ports: str,
) -> None:
    bridge, client1, client2, host_ip, bridge_ip, client1_ip, client2_ip = _setup_boards(
        dut, lan86xx_serial_ports
    )

    _run_ping_matrix(
        bridge,
        client1,
        client2,
        Lan86xxBoardIps(host=host_ip, bridge=bridge_ip, client1=client1_ip, client2=client2_ip),
    )

    iperf_failures: list[str] = []
    iperf_clients = (
        ('client1', client1, client1_ip),
        ('client2', client2, client2_ip),
    )
    for name, board, initial_ip in iperf_clients:
        # Precheck once per client, a hard reset re-syncs PLCA/T1S before iperf.
        dut_ip = _precheck_client_reachable(bridge, name, board, initial_ip)
        for proto in ('tcp', 'udp'):
            try:
                _run_pc_to_dut_iperf(name, board, dut_ip, proto)
            except IperfCaseError as err:
                logging.error('%s', err)
                iperf_failures.append(str(err))

    if iperf_failures:
        pytest.fail('iPerf failures:\n' + '\n'.join(f'  - {msg}' for msg in iperf_failures))


@pytest.mark.eth_lan86xx
@idf_parametrize('target', ['esp32'], indirect=['target'])
def test_lan86xx_bridge_iperf_parallel(
    dut: tuple[IdfDut, IdfDut, IdfDut],
    lan86xx_serial_ports: str,
) -> None:
    """PC -> client1 + client2 iperf concurrently (TCP then UDP)."""
    bridge, client1, client2, _, _, client1_ip, client2_ip = _setup_boards(
        dut, lan86xx_serial_ports
    )

    # Reachability-with-recovery before parallel iperf: both clients must be healthy,
    # otherwise the offending one is hard-reset and re-DHCP'd.
    client1_ip = _precheck_client_reachable(bridge, 'client1', client1, client1_ip)
    client2_ip = _precheck_client_reachable(bridge, 'client2', client2, client2_ip)

    iperf_cases = (
        ('client1', client1, client1_ip),
        ('client2', client2, client2_ip),
    )
    iperf_failures: list[str] = []
    for proto in ('tcp', 'udp'):
        try:
            _run_pc_to_dut_iperf_parallel(iperf_cases, proto)
        except IperfCaseError as err:
            logging.error('%s', err)
            iperf_failures.append(str(err))
        if proto == 'tcp':
            time.sleep(IPERF_POST_TCP_SETTLE_S)

    if iperf_failures:
        pytest.fail('iPerf parallel failures:\n' + '\n'.join(f'  - {msg}' for msg in iperf_failures))


@pytest.mark.eth_lan86xx
@idf_parametrize('target', ['esp32'], indirect=['target'])
def test_lan86xx_dut_to_dut_iperf(
    dut: tuple[IdfDut, IdfDut, IdfDut],
    lan86xx_serial_ports: str,
) -> None:
    """client2 (iperf client) -> client1 (iperf server) over a single T1S/PLCA hop.

    No PC/W5500 involvement on the data path: traffic is LAN8670 (node 2) ->
    shared T1S bus -> LAN8670 (node 1). The bridge LAN8650 sits on the same
    multidrop wire but does not have to forward (same segment), so this is the
    cleanest measure of the LAN867x-to-LAN867x throughput at the firmware level.
    """
    bridge, client1, client2, _, _, client1_ip, client2_ip = _setup_boards(
        dut, lan86xx_serial_ports
    )

    client1_ip = _precheck_client_reachable(bridge, 'client1', client1, client1_ip)
    client2_ip = _precheck_client_reachable(bridge, 'client2', client2, client2_ip)
    del client2_ip  # client2 is the iperf-client; only its DUT handle is needed below

    server_ep = _DutEndpoint('client1', client1, client1_ip)
    client_ep = _DutEndpoint('client2', client2)
    iperf_failures: list[str] = []
    for proto in ('tcp', 'udp'):
        try:
            _run_dut_to_dut_iperf(server=server_ep, client=client_ep, proto=proto)
        except IperfCaseError as err:
            logging.error('%s', err)
            iperf_failures.append(str(err))
        if proto == 'tcp':
            logging.info(
                'Waiting %ds after DUT->DUT TCP for T1S/PLCA settle before UDP',
                IPERF_POST_TCP_SETTLE_S,
            )
            time.sleep(IPERF_POST_TCP_SETTLE_S)

    if iperf_failures:
        pytest.fail(
            'DUT->DUT iPerf failures:\n' + '\n'.join(f'  - {msg}' for msg in iperf_failures)
        )


@pytest.mark.eth_lan86xx
@idf_parametrize('target', ['esp32'], indirect=['target'])
def test_lan86xx_plca_multi_ids(
    dut: tuple[IdfDut, IdfDut, IdfDut],
    lan86xx_serial_ports: str,
) -> None:
    """Both clients send UDP to the PC; client1 has two PLCA TOs, client2 has one.

    Extra slots only help the node that transmits, so the traffic is DUT->PC.
    If MULTI_IDS is applied, client1 must land a clearly higher host receive rate.
    """
    bridge, client1, client2, host_ip, _, client1_ip, client2_ip = _setup_boards(
        dut,
        lan86xx_serial_ports,
        plca_ncnt=8,
        client1_extra_id=3,
    )
    client1_ip = _precheck_client_reachable(bridge, 'client1', client1, client1_ip)
    client2_ip = _precheck_client_reachable(bridge, 'client2', client2, client2_ip)
    del client1_ip, client2_ip

    _run_dut_to_pc_iperf_plca_slots(
        host_ip,
        (
            ('client1', client1, PLCA_MULTI_IDS_HOST_PORT_C1),
            ('client2', client2, PLCA_MULTI_IDS_HOST_PORT_C2),
        ),
    )


def _setup_boards(
    dut: tuple[IdfDut, IdfDut, IdfDut],
    lan86xx_serial_ports: str,
    *,
    plca_ncnt: int | None = None,
    client1_extra_id: int | None = None,
) -> tuple[IdfDut, IdfDut, IdfDut, str, str, str, str]:
    expected_ports = lan86xx_serial_ports.split('|')
    if len(expected_ports) < BRIDGE_DUT_COUNT:
        pytest.fail(f'Expected {BRIDGE_DUT_COUNT} serial ports, got: {lan86xx_serial_ports}')

    bridge, client1, client2 = dut
    for idx, (board, expected_port) in enumerate(zip((bridge, client1, client2), expected_ports)):
        if board.serial.port != expected_port:
            pytest.fail(f'DUT {idx} serial mismatch: expected {expected_port}, got {board.serial.port}')

    bridge_ip = _wait_bridge_ready(bridge, plca_ncnt=plca_ncnt)
    logging.info('Bridge ready at %s; resetting client1 so DHCP runs with bridge up', bridge_ip)
    _hard_reset_dut(client1)
    extra_re = (
        rf'PLCA additional local ID: {client1_extra_id}' if client1_extra_id is not None else None
    )
    client1_ip = _wait_dhcp_client(client1, resync_boot=True, extra_boot_re=extra_re)
    _hard_reset_dut(client2)
    client2_ip = _wait_dhcp_client(client2, resync_boot=True)

    host_ip = _host_ip_for_target(bridge_ip)

    ip_set = {bridge_ip, client1_ip, client2_ip}
    if len(ip_set) != BRIDGE_DUT_COUNT:
        pytest.fail(f'Duplicate IP addresses detected: {bridge_ip}, {client1_ip}, {client2_ip}')

    logging.info(
        'All boards have DHCP; waiting %ds for PLCA/T1S settle before ping',
        PING_SETTLE_AFTER_DHCP_S,
    )
    time.sleep(PING_SETTLE_AFTER_DHCP_S)

    return bridge, client1, client2, host_ip, bridge_ip, client1_ip, client2_ip


def _wait_bridge_ready(bridge: IdfDut, *, plca_ncnt: int | None = None) -> str:
    logging.info('Waiting for bridge firmware (dut-0)...')
    if plca_ncnt is not None:
        bridge.expect(rf'PLCA node count {plca_ncnt}', timeout=120)
    bridge.expect(r'Bridge up: \d+ Ethernet port\(s\)', timeout=120)
    logging.info('Waiting for W5500 link on bridge (check PC cable + DHCP)...')
    try:
        # W5500: ethernet_init or lan86xx_bridge handler (first Link Up is W5500 side).
        bridge.expect(
            r'(Ethernet\(W5500\[.*Link Up|lan86xx_bridge: Ethernet .* Link Up)',
            timeout=90,
        )
    except Exception as err:
        pytest.fail(
            f'Bridge W5500 did not link within 90s on {bridge.serial.port}. '
            f'Check Ethernet cable (bridge W5500 -> PC) and that the host NIC is UP '
            f'with DHCP/shared network for the bridge. ({err})'
        )
    logging.info('Waiting for bridge DHCP...')
    try:
        bridge.expect('Bridge Got IP Address', timeout=120)
    except Exception as err:
        pytest.fail(
            f'Bridge did not get DHCP within 120s on {bridge.serial.port}. '
            f'Ensure PC shared network (10.42.0.0/24) is up. ({err})'
        )
    return _expect_ip(bridge)


def _run_ping_matrix(
    bridge: IdfDut,
    client1: IdfDut,
    client2: IdfDut,
    ips: Lan86xxBoardIps,
) -> None:
    _host_ping(ips.bridge, 'host->bridge')
    _host_ping(ips.client1, 'host->client1')
    _host_ping(ips.client2, 'host->client2')

    _dut_ping(bridge, ips.host, 'bridge->host')
    _dut_ping(bridge, ips.client1, 'bridge->client1')
    _dut_ping(bridge, ips.client2, 'bridge->client2')

    _dut_ping(client1, ips.host, 'client1->host')
    _dut_ping(client1, ips.bridge, 'client1->bridge')
    _dut_ping(client1, ips.client2, 'client1->client2')

    _dut_ping(client2, ips.host, 'client2->host')
    _dut_ping(client2, ips.bridge, 'client2->bridge')
    _dut_ping(client2, ips.client1, 'client2->client1')


def _dut_iperf_server_cmd(proto: str) -> str:
    if proto == 'udp':
        return f'iperf -u -s -i 1 -t {IPERF_INTERVAL}'
    return f'iperf -s -i 1 -t {IPERF_INTERVAL}'


def _pc_iperf_client_cmd(dut_ip: str, proto: str, *, parallel: bool = False) -> list[str]:
    cmd = [IPERF_BIN, '-c', dut_ip, '-t', str(IPERF_INTERVAL), '-i', '1']
    if proto == 'udp':
        cmd.insert(1, '-u')
        bw = IPERF_BW_LIM_PARALLEL if parallel else IPERF_BW_LIM
        cmd.extend(['-b', f'{bw}M'])
    elif parallel:
        cmd.extend(['-b', f'{IPERF_TCP_BW_LIM_PARALLEL}M'])
    return cmd


def _dut_iperf_ready_pattern(proto: str) -> str:
    # iperf 1.0 logs "Socket created" at info for both; UDP "socket bound" is debug-only.
    return r'Socket created'


def _start_dut_iperf_server(name: str, dut: IdfDut, proto: str) -> None:
    server_cmd = _dut_iperf_server_cmd(proto)
    logging.info('%s PC->%s %s: starting DUT server (%s)', name, name, proto.upper(), server_cmd)
    _wait_dut_console_ready(dut, timeout=5)
    _console_write(dut, server_cmd)
    try:
        dut.expect(_dut_iperf_ready_pattern(proto), timeout=20)
    except Exception as err:
        raise IperfCaseError(
            f'{name} PC->{proto.upper()} server did not start on {dut.serial.port}: {err}'
        ) from err


def _pexpect_chunk(dut: IdfDut, field: str) -> str:
    raw = getattr(dut.pexpect_proc, field, b'') or b''
    if isinstance(raw, bytes):
        return raw.decode(errors='replace')
    return str(raw) if raw else ''


def _read_dut_iperf_log(dut: IdfDut, name: str) -> str:
    """Collect serial from the current iperf run (intervals + aggregate + exit)."""
    chunks: list[str] = []
    agg_pat = (
        rf'0\.0-\s*{IPERF_INTERVAL}\.0\s+sec\s+'
        rf'(?:[\d.]+\s+\w+Bytes\s+)?[\d.]+\s+Mbits/sec'
    )
    try:
        dut.expect(agg_pat, timeout=IPERF_INTERVAL + 25)
        chunks.append(_pexpect_chunk(dut, 'before'))
        chunks.append(_pexpect_chunk(dut, 'after'))
    except Exception as err:
        logging.debug('%s: DUT iperf aggregate not seen: %s', name, err)
        chunks.append(_pexpect_chunk(dut, 'before'))
    try:
        dut.expect('iperf exit', timeout=25)
        chunks.append(_pexpect_chunk(dut, 'before'))
        chunks.append(_pexpect_chunk(dut, 'after'))
    except Exception as err:
        logging.debug('%s: DUT did not report iperf exit: %s', name, err)
    return ''.join(chunks)


def _dut_iperf_rates(dut_log: str) -> list[str]:
    agg = IPERF_AGGREGATE_RE.findall(dut_log)
    return agg if agg else IPERF_SUMMARY_RE.findall(dut_log)


def _iperf_bandwidth(proto: str, dut_log: str, pc_log: str) -> tuple[list[str], str]:
    del proto, pc_log  # PC→DUT: pass/fail only from DUT server bandwidth lines
    return _dut_iperf_rates(dut_log), 'DUT receive'


def _log_iperf_rates(name: str, proto: str, pc_log: str, dut_log: str) -> None:
    pc_rates = HOST_IPERF_SUMMARY_RE.findall(pc_log)
    dut_rates = _dut_iperf_rates(dut_log)
    pc_label = 'PC send' if proto == 'udp' else 'PC summary'
    if pc_rates:
        logging.info(
            '%s PC->%s %s %s: %.2f Mbits/s',
            name, name, proto.upper(), pc_label, float(pc_rates[-1]),
        )
    if dut_rates:
        logging.info(
            '%s PC->%s %s DUT receive: %.2f Mbits/s',
            name, name, proto.upper(), float(dut_rates[-1]),
        )


def _run_pc_to_dut_iperf(name: str, dut: IdfDut, dut_ip: str, proto: str) -> float:
    """Run iperf with DUT as server and PC as client (one client at a time)."""
    time.sleep(0.5)

    _kill_host_iperf()
    _stop_dut_iperf(dut)
    time.sleep(0.3)

    _start_dut_iperf_server(name, dut, proto)
    time.sleep(0.3)

    client_cmd = _pc_iperf_client_cmd(dut_ip, proto)
    logging.info('%s PC->%s %s: PC client (%s)', name, name, proto.upper(), ' '.join(client_cmd))

    client_proc = subprocess.Popen(  # noqa: S603
        client_cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    try:
        pc_log = client_proc.communicate(timeout=IPERF_INTERVAL + 15)[0]
        dut_log = _read_dut_iperf_log(dut, name)
        _stop_dut_iperf(dut)
        return _check_pc_to_dut_iperf(name, dut, proto, (pc_log, dut_log))
    finally:
        if client_proc.poll() is None:
            client_proc.terminate()
            client_proc.communicate(timeout=5)
        _stop_dut_iperf(dut)
        _kill_host_iperf()


def _min_throughput(proto: str, *, parallel: bool) -> float:
    if parallel:
        return MIN_THROUGHPUT_T1S_PARALLEL_ALIVE
    return MIN_TCP_THROUGHPUT_T1S if proto == 'tcp' else MIN_UDP_THROUGHPUT_T1S


def _min_parallel_sum(proto: str) -> float:
    if proto == 'tcp':
        return MIN_TCP_THROUGHPUT_T1S_PARALLEL_SUM
    return MIN_UDP_THROUGHPUT_T1S_PARALLEL_SUM


def _check_pc_to_dut_iperf(
    name: str,
    dut: IdfDut,
    proto: str,
    logs: tuple[str, str],
    *,
    parallel: bool = False,
) -> float:
    """Validate one PC->DUT iperf case and return the pass/fail bandwidth.

    ``logs`` is ``(pc_log, dut_log)`` — bundled to keep the signature within the
    ruff PLR0913 limit; the two strings are produced together by the caller.
    """
    pc_log, dut_log = logs
    min_bw = _min_throughput(proto, parallel=parallel)
    _log_iperf_rates(name, proto, pc_log, dut_log)
    matches, metric = _iperf_bandwidth(proto, dut_log, pc_log)
    if not matches:
        logging.info('--- DUT serial (%s) ---\n%s', name, dut_log)
        logging.info('--- PC iperf client (%s) ---\n%s', name, pc_log)
        raise IperfCaseError(f'{name} PC->{proto.upper()} summary missing on {dut.serial.port}')

    bandwidth = float(matches[-1])
    logging.info(
        '%s PC->%s %s throughput (%s): %.2f Mbits/s',
        name, name, proto.upper(), metric, bandwidth,
    )
    if bandwidth < min_bw:
        raise IperfCaseError(
            f'{name} PC->{proto.upper()} throughput {bandwidth:.2f} Mbits/s < {min_bw} Mbits/s'
        )
    return bandwidth


def _stop_all_dut_iperf(duts: list[IdfDut]) -> None:
    """Stop iperf on each DUT one at a time (shared USB host controller)."""
    _kill_host_iperf()
    for dut in duts:
        _stop_dut_iperf(dut)
    time.sleep(0.3)


def _parallel_pc_stagger_s(proto: str) -> float:
    if proto == 'tcp':
        return IPERF_PARALLEL_STAGGER_TCP_S
    return IPERF_PARALLEL_STAGGER_UDP_S


def _check_parallel_aggregate(rates: dict[str, float], proto: str, min_sum: float) -> None:
    """Require a combined DUT receive rate across both clients."""
    if len(rates) < _PARALLEL_PEER_COUNT:
        return
    ordered = sorted(rates.items())
    (n0, r0), (n1, r1) = ordered[0], ordered[1]
    total = r0 + r1
    logging.info(
        'parallel %s DUT receive: %s=%.2f, %s=%.2f, sum=%.2f Mbit/s',
        proto.upper(), n0, r0, n1, r1, total,
    )
    if total < min_sum:
        raise IperfCaseError(
            f'parallel {proto.upper()} combined {total:.2f} Mbits/s < {min_sum} Mbits/s'
        )


def _start_parallel_pc_clients(
    run_cases: tuple[tuple[str, IdfDut, str], ...],
    proto: str,
    stagger_s: float,
) -> dict[str, subprocess.Popen[str]]:
    """Spawn one PC iperf client per case, optionally staggered."""
    procs: dict[str, subprocess.Popen[str]] = {}
    for idx, (name, _, dut_ip) in enumerate(run_cases):
        if idx > 0 and stagger_s > 0:
            time.sleep(stagger_s)
        client_cmd = _pc_iperf_client_cmd(dut_ip, proto, parallel=True)
        logging.info(
            '%s PC->%s %s: PC client (%s)',
            name, name, proto.upper(), ' '.join(client_cmd),
        )
        procs[name] = subprocess.Popen(  # noqa: S603
            client_cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
        )
    return procs


def _collect_parallel_pc_logs(
    procs: dict[str, subprocess.Popen[str]],
    proto: str,
    communicate_timeout: float,
) -> dict[str, str]:
    """Wait on every PC client proc in parallel and return its stdout."""
    pc_logs: dict[str, str] = {}
    with ThreadPoolExecutor(max_workers=len(procs)) as pool:
        pc_futures = {
            name: pool.submit(proc.communicate, timeout=communicate_timeout)
            for name, proc in procs.items()
        }
        for name, future in pc_futures.items():
            try:
                pc_logs[name] = future.result()[0]
            except Exception as err:
                raise IperfCaseError(f'{name} PC->{proto.upper()} client failed: {err}') from err
    return pc_logs


def _collect_parallel_dut_logs(dut_by_name: dict[str, IdfDut]) -> dict[str, str]:
    """Read each DUT's iperf log in parallel via independent pexpect handles."""
    dut_logs: dict[str, str] = {}
    with ThreadPoolExecutor(max_workers=len(dut_by_name)) as pool:
        dut_futures = {
            name: pool.submit(_read_dut_iperf_log, dut, name)
            for name, dut in dut_by_name.items()
        }
        for name, future in dut_futures.items():
            dut_logs[name] = future.result()
    return dut_logs


def _evaluate_parallel_results(
    run_cases: tuple[tuple[str, IdfDut, str], ...],
    proto: str,
    pc_logs: dict[str, str],
    dut_logs: dict[str, str],
    min_sum: float,
) -> list[str]:
    """Check each per-DUT case + the combined-rate gate; return failure messages."""
    rates: dict[str, float] = {}
    failures: list[str] = []
    for name, dut, _ in run_cases:
        try:
            rates[name] = _check_pc_to_dut_iperf(
                name, dut, proto, (pc_logs[name], dut_logs[name]), parallel=True,
            )
        except IperfCaseError as err:
            failures.append(str(err))
    if not failures:
        try:
            _check_parallel_aggregate(rates, proto, min_sum)
        except IperfCaseError as err:
            failures.append(str(err))
    return failures


def _run_pc_to_dut_iperf_parallel(
    cases: tuple[tuple[str, IdfDut, str], ...],
    proto: str,
) -> None:
    """Run PC->DUT iperf to all cases concurrently (one DUT server + PC client each)."""
    time.sleep(0.5)
    duts = [dut for _, dut, _ in cases]
    dut_by_name = {name: dut for name, dut, _ in cases}
    communicate_timeout = IPERF_INTERVAL + 20
    stagger_s = _parallel_pc_stagger_s(proto)
    min_sum = _min_parallel_sum(proto)

    _stop_all_dut_iperf(duts)

    for idx, (name, dut, _) in enumerate(cases):
        _start_dut_iperf_server(name, dut, proto)
        if idx + 1 < len(cases):
            time.sleep(IPERF_SERVER_GAP_S)

    time.sleep(0.3)

    client_procs: dict[str, subprocess.Popen[str]] = {}
    try:
        client_procs = _start_parallel_pc_clients(cases, proto, stagger_s)
        pc_logs = _collect_parallel_pc_logs(client_procs, proto, communicate_timeout)
        dut_logs = _collect_parallel_dut_logs(dut_by_name)
        failures = _evaluate_parallel_results(cases, proto, pc_logs, dut_logs, min_sum)
        if failures:
            raise IperfCaseError('\n'.join(failures))
    finally:
        for proc in client_procs.values():
            if proc.poll() is None:
                proc.terminate()
                proc.communicate(timeout=5)
        _stop_all_dut_iperf(duts)


def _dut_iperf_client_cmd(target_ip: str, proto: str) -> str:
    """DUT-side iperf-client command.

    iperf-cmd 1.0 takes -b in bit/s. The ``m`` suffix keeps the offered rate in Mbit/s.
    """
    if proto == 'udp':
        return (
            f'iperf -u -c {target_ip} -i 1 -t {IPERF_INTERVAL} '
            f'-b {IPERF_BW_LIM_DUT2DUT_UDP}m'
        )
    return f'iperf -c {target_ip} -i 1 -t {IPERF_INTERVAL}'


def _check_dut_to_dut_iperf(
    label: str,
    server_dut: IdfDut,
    proto: str,
    server_log: str,
    client_log: str,
) -> float:
    """Validate a DUT->DUT iperf case against the DUT2DUT minimum thresholds."""
    server_rates = _dut_iperf_rates(server_log)
    client_rates = _dut_iperf_rates(client_log)
    if client_rates:
        logging.info(
            '%s %s DUT send (client side): %.2f Mbits/s',
            label, proto.upper(), float(client_rates[-1]),
        )
    if not server_rates:
        logging.info('--- DUT serial (server side) ---\n%s', server_log)
        logging.info('--- DUT serial (client side) ---\n%s', client_log)
        raise IperfCaseError(
            f'{label} {proto.upper()} server summary missing on {server_dut.serial.port}'
        )
    bandwidth = float(server_rates[-1])
    logging.info(
        '%s %s DUT receive (server side): %.2f Mbits/s',
        label, proto.upper(), bandwidth,
    )
    min_bw = (
        MIN_TCP_THROUGHPUT_T1S_DUT2DUT
        if proto == 'tcp'
        else MIN_UDP_THROUGHPUT_T1S_DUT2DUT
    )
    if bandwidth < min_bw:
        raise IperfCaseError(
            f'{label} {proto.upper()} throughput {bandwidth:.2f} Mbits/s < {min_bw} Mbits/s'
        )
    return bandwidth


def _run_dut_to_dut_iperf(
    *,
    server: _DutEndpoint,
    client: _DutEndpoint,
    proto: str,
) -> float:
    """Run iperf with one DUT as server and the other as client over T1S/PLCA.

    Both DUTs run the firmware ``iperf`` task. We use the **server** receive
    rate for pass/fail (consistent with PC->DUT cases) and log the client send
    rate for visibility. Server and client logs are read in parallel via
    independent /dev/ttyUSBx pexpect handles.
    """
    label = f'{client.name}->{server.name}'
    time.sleep(0.5)

    _stop_dut_iperf(server.dut)
    _stop_dut_iperf(client.dut)
    time.sleep(0.3)

    _start_dut_iperf_server(server.name, server.dut, proto)
    time.sleep(IPERF_SERVER_GAP_S)

    client_cmd = _dut_iperf_client_cmd(server.ip, proto)
    logging.info(
        '%s %s: DUT client (%s) on %s',
        label, proto.upper(), client_cmd, client.dut.serial.port,
    )
    _wait_dut_console_ready(client.dut, timeout=5)
    _console_write(client.dut, client_cmd)

    try:
        with ThreadPoolExecutor(max_workers=2) as pool:
            server_future = pool.submit(_read_dut_iperf_log, server.dut, server.name)
            client_future = pool.submit(_read_dut_iperf_log, client.dut, client.name)
            server_log = server_future.result()
            client_log = client_future.result()
        return _check_dut_to_dut_iperf(label, server.dut, proto, server_log, client_log)
    finally:
        _stop_dut_iperf(client.dut)
        _stop_dut_iperf(server.dut)


def _dut_iperf_client_to_host_cmd(host_ip: str, port: int) -> str:
    return (
        f'iperf -u -c {host_ip} -p {port} -i 1 -t {IPERF_INTERVAL} '
        f'-b {IPERF_BW_LIM}m'
    )


def _start_host_iperf_server(port: int) -> subprocess.Popen[str]:
    cmd = [IPERF_BIN, '-u', '-s', '-p', str(port), '-i', '1']
    logging.info('host UDP server: %s', ' '.join(cmd))
    return subprocess.Popen(  # noqa: S603
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )


def _host_iperf_rate(pc_log: str) -> float | None:
    rates = HOST_IPERF_SUMMARY_RE.findall(pc_log)
    return float(rates[-1]) if rates else None


def _plca_host_rate(
    name: str,
    server: subprocess.Popen[str],
    dut_log: str,
) -> float:
    send_rates = _dut_iperf_rates(dut_log)
    if send_rates:
        logging.info(
            '%s DUT->PC UDP DUT send (offered): %.2f Mbits/s',
            name, float(send_rates[-1]),
        )
    server.terminate()
    pc_log, _ = server.communicate(timeout=8)
    rate = _host_iperf_rate(pc_log)
    if rate is None:
        logging.info('--- host iperf server (%s) ---\n%s', name, pc_log)
        logging.info('--- DUT serial (%s) ---\n%s', name, dut_log)
        raise IperfCaseError(f'{name} DUT->PC UDP host summary missing')
    logging.info('%s DUT->PC UDP host receive: %.2f Mbits/s', name, rate)
    return rate


def _assert_plca_slot_advantage(c1: float, c2: float) -> None:
    ratio = c1 / c2 if c2 > 0 else 0.0
    logging.info(
        'PLCA slots client1=4 (ids 1,3,5,7) client2=1 (id 2): '
        'host recv client1=%.2f client2=%.2f ratio=%.2f (need >= %.2f)',
        c1, c2, ratio, PLCA_MULTI_IDS_MIN_RATIO,
    )
    if c1 < PLCA_MULTI_IDS_MIN_EACH or c2 < PLCA_MULTI_IDS_MIN_EACH:
        raise IperfCaseError(
            f'PLCA multi-id UDP too low: client1={c1:.2f} client2={c2:.2f} '
            f'(need >= {PLCA_MULTI_IDS_MIN_EACH} each)'
        )
    if ratio < PLCA_MULTI_IDS_MIN_RATIO:
        raise IperfCaseError(
            f'PLCA multi-id UDP did not favor client1: '
            f'client1={c1:.2f} client2={c2:.2f} ratio={ratio:.2f} '
            f'< {PLCA_MULTI_IDS_MIN_RATIO} (expected 4 TOs vs 1)'
        )


def _run_dut_to_pc_iperf_plca_slots(
    host_ip: str,
    cases: tuple[tuple[str, IdfDut, int], ...],
) -> None:
    """Both clients send UDP to the PC at once; client1 must outrun client2."""
    duts = [dut for _, dut, _ in cases]
    _stop_all_dut_iperf(duts)

    servers: dict[str, subprocess.Popen[str]] = {}
    try:
        for name, _, port in cases:
            servers[name] = _start_host_iperf_server(port)
        time.sleep(0.4)

        for name, dut, port in cases:
            cmd = _dut_iperf_client_to_host_cmd(host_ip, port)
            logging.info('%s DUT->PC UDP: %s', name, cmd)
            _wait_dut_console_ready(dut, timeout=5)
            _console_write(dut, cmd)

        dut_logs = _collect_parallel_dut_logs({name: dut for name, dut, _ in cases})
        host_rates = {
            name: _plca_host_rate(name, servers[name], dut_logs[name])
            for name, _, _ in cases
        }
        _assert_plca_slot_advantage(host_rates['client1'], host_rates['client2'])
    except IperfCaseError as err:
        pytest.fail(str(err))
    finally:
        for proc in servers.values():
            if proc.poll() is None:
                proc.terminate()
                proc.communicate(timeout=5)
        _stop_all_dut_iperf(duts)


def _hard_reset_dut(dut: IdfDut) -> None:
    """Reset DUT via pytest-embedded's already-open pyserial handle.

    Using ``subprocess esptool ... run`` here races the pyserial port that
    pytest-embedded keeps open: the external esptool can fail silently to take
    control of /dev/ttyUSBx mid-test, and the chip is never actually reset.
    ``IdfSerial.hard_reset()`` toggles DTR/RTS on the same fd we're already
    reading from, so the boot output lands in the pexpect buffer reliably.
    """
    port = dut.serial.port
    logging.info('Hard-resetting %s', port)
    try:
        dut.serial.hard_reset()
    except Exception as err:
        logging.warning('hard_reset() failed on %s: %s; retrying once', port, err)
        time.sleep(0.3)
        dut.serial.hard_reset()
    time.sleep(0.3)


def _wait_dhcp_client(
    dut: IdfDut,
    *,
    resync_boot: bool = False,
    extra_boot_re: str | None = None,
) -> str:
    """Wait until DHCP-only client is ready and return its ETHIP."""
    logging.info('Waiting for client DHCP on %s...', dut.serial.port)
    try:
        if extra_boot_re:
            dut.expect(extra_boot_re, timeout=30)
        if resync_boot:
            dut.expect('Waiting for IP address...', timeout=30)
        ip_match = dut.expect(r'ETHIP:(\d+\.\d+\.\d+\.\d+)', timeout=120)
        dut.expect('DHCP-only mode enabled; skipping TCP client traffic.', timeout=30)
    except Exception as err:
        pytest.fail(
            f'Client on {dut.serial.port} did not get DHCP within 120s '
            f'(may be stuck after link up). Check bridge W5500 link, T1S cable '
            f'(client1 must reach bridge LAN8650), PLCA NCNT=2, and PC DHCP. ({err})'
        )
    ip_addr = ip_match.group(1)
    if isinstance(ip_addr, bytes):
        ip_addr = ip_addr.decode()
    logging.info('Client %s got IP %s', dut.serial.port, ip_addr)
    # REPL is already up by the time DHCP-only log fires; brief pause to drain pending banner
    time.sleep(0.5)
    return ip_addr


def _console_write(dut: IdfDut, cmd: str) -> None:
    """Send a console command (REPL starts after DHCP-only on clients)."""
    dut.write(f'{cmd}\n')


def _expect_ip(dut: IdfDut) -> str:
    ip_addr = dut.expect(r'ETHIP:(\d+\.\d+\.\d+\.\d+)', timeout=10).group(1)
    if isinstance(ip_addr, bytes):
        ip_addr = ip_addr.decode()
    return ip_addr


def _dut_ping_attempt(dut: IdfDut, target_ip: str, *, min_rx: int) -> None:
    """Single ping attempt; raises with reason on console error or low-quality result."""
    result_pat = re.compile(rf'PING_RESULT {re.escape(target_ip)} (OK|FAIL) rx=(\d+)/(\d+)')
    _wait_dut_console_ready(dut, timeout=3)
    _console_write(dut, f'ping {target_ip}')
    match = dut.expect(result_pat, timeout=PING_DUT_TIMEOUT_S)
    status = match.group(1)
    if isinstance(status, bytes):
        status = status.decode()
    rx = int(match.group(2))
    total = int(match.group(3))
    if status != 'OK' or rx < min_rx:
        raise RuntimeError(f'{status} rx={rx}/{total} (need OK + rx>={min_rx})')


def _dut_ping_quality(
    dut: IdfDut,
    target_ip: str,
    label: str,
    *,
    min_rx: int = PING_DUT_OK_MIN_RX,
    retries: int = PING_RETRIES,
) -> bool:
    """Try ping with retries; return True on success, False on persistent failure (no fail)."""
    last_err: object = None
    for attempt in range(1, retries + 1):
        try:
            _dut_ping_attempt(dut, target_ip, min_rx=min_rx)
            return True
        except Exception as err:
            last_err = err
            logging.warning(
                '%s %s->%s attempt %d/%d failed: %s',
                dut.serial.port, label, target_ip, attempt, retries, err,
            )
            if attempt < retries:
                time.sleep(PING_RETRY_DELAY_S)
    logging.warning(
        '%s %s->%s exhausted %d attempts (last: %s)',
        dut.serial.port, label, target_ip, retries, last_err,
    )
    return False


def _dut_ping(
    dut: IdfDut,
    target_ip: str,
    label: str,
    *,
    min_rx: int = PING_DUT_OK_MIN_RX,
) -> None:
    """Ping with retries; pytest.fail on persistent failure (matrix path)."""
    if not _dut_ping_quality(dut, target_ip, label, min_rx=min_rx):
        pytest.fail(
            f'{label} ping from {dut.serial.port} to {target_ip} failed after {PING_RETRIES} '
            f'attempts (need OK + rx>={min_rx}). Check T1S/PLCA path via bridge.'
        )


def _precheck_client_reachable(
    bridge: IdfDut,
    name: str,
    client: IdfDut,
    client_ip: str,
) -> str:
    """Verify bridge<->client connectivity right before iperf.

    On poor quality (rx<min or timeout), hard-reset the client MCU and re-run DHCP so
    PLCA/T1S re-syncs from a clean state. Returns the (possibly new) client IP. Fails
    the test only if the client is still unreachable after recovery.
    """
    label = f'precheck bridge->{name}'
    if _dut_ping_quality(bridge, client_ip, label, retries=PRECHECK_RETRIES):
        return client_ip

    logging.warning(
        '%s reachability poor at %s; hard-resetting %s and re-running DHCP',
        name, client_ip, client.serial.port,
    )
    _hard_reset_dut(client)
    new_ip = _wait_dhcp_client(client, resync_boot=True)
    if new_ip != client_ip:
        logging.warning('%s IP changed after recovery: %s -> %s', name, client_ip, new_ip)
    logging.info('Waiting %ds for PLCA/T1S settle after recovery', PING_SETTLE_AFTER_DHCP_S)
    time.sleep(PING_SETTLE_AFTER_DHCP_S)
    if not _dut_ping_quality(bridge, new_ip, f'post-recover bridge->{name}', retries=PING_RETRIES):
        pytest.fail(
            f'{name} still unreachable after hard-reset on {client.serial.port}. '
            f'Check T1S cable/termination, PLCA NCNT alignment, and bridge LAN8650 SPI.'
        )
    return new_ip


def _host_ip_for_target(target_ip: str) -> str:
    route = subprocess.check_output([IP_BIN, '-4', 'route', 'get', target_ip], text=True).strip()  # noqa: S603
    match = re.search(r'\bsrc\s+(\d+\.\d+\.\d+\.\d+)\b', route)
    if match is None:
        raise RuntimeError(f'Cannot resolve host source IP for target {target_ip}. route={route}')
    return match.group(1)


def _host_ping(target_ip: str, label: str) -> None:
    last_err: str | None = None
    for attempt in range(1, PING_RETRIES + 1):
        ret = subprocess.call(  # noqa: S603
            [
                PING_BIN,
                target_ip,
                '-c',
                str(PING_HOST_COUNT),
                '-W',
                str(PING_HOST_DEADLINE_S),
            ],
        )
        if ret == 0:
            return
        last_err = f'exit code {ret}'
        logging.warning(
            '%s %s attempt %d/%d failed (%s)',
            label, target_ip, attempt, PING_RETRIES, last_err,
        )
        time.sleep(PING_RETRY_DELAY_S)
    raise RuntimeError(
        f'{label} host ping to {target_ip} failed after {PING_RETRIES} attempts ({last_err})'
    )


def _kill_host_iperf() -> None:
    subprocess.run(  # noqa: S603
        [KILLALL_BIN, 'iperf'],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        check=False,
    )


def _wait_dut_console_ready(dut: IdfDut, timeout: float = IPERF_ABORT_TIMEOUT_S) -> None:
    """Wait until the console can accept a new command."""
    try:
        dut.expect(r'(lan86xx>|t1sbridge>|DONE\.IPERF_STOP,OK)', timeout=timeout)
    except Exception:
        time.sleep(0.5)


def _stop_dut_iperf(dut: IdfDut) -> None:
    _console_write(dut, 'iperf --abort')
    try:
        dut.expect(r'DONE\.IPERF_STOP,OK', timeout=IPERF_ABORT_TIMEOUT_S)
    except Exception:
        _console_write(dut, 'iperf --abort')
        try:
            dut.expect(r'DONE\.IPERF_STOP,OK', timeout=5)
        except Exception as err:
            logging.debug(
                'iperf abort retry on %s did not ack within 5s: %s',
                dut.serial.port, err,
            )
    _wait_dut_console_ready(dut, timeout=5)
