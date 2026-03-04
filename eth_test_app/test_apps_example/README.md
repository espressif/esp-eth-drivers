# eth_test_app example

Minimal ESP-IDF test project that runs the **eth_test_app** Ethernet test suite (Ethernet apps, L2, heap allocation) via [`EthTestRunner`](../eth_test_runner.py). Use it as a **starting point when adding tests for a specific PHY chip**.

## Create a new test project from this example

1) From your driver repo (or any IDF project directory):

```bash
idf.py create-project-from-example "espressif/eth_test_app=*:test_apps_example"
```

2) Open the created project and:

- Rename `pytest_ethernet.py` to the chip-under-test name (e.g. `pytest_dm9051.py`).
- Set **TEST_IF** in `pytest_ethernet.py` – host Ethernet interface for L2/heap tests (`ip -c a` to list) or keep empty.
- Set **config** and **pytest marks** – the sdkconfig preset and PHY marker for your chip (e.g. `pytest.mark.eth_dm9051`, `pytest.mark.eth_ip101`).
- Reconfigure `sdkconfig.ci.default` to align with hardware setup and Ethernet chip under test needs, see [Chip-Specific Configuration](../README.md#chip-specific-configuration) in upper level README.

See the `CONFIGURE FOR YOUR CHIP` block at the top of `pytest_ethernet.py` for details.

3) And run your tests as:
```python
def test_eth_example(dut: Dut, eth_test_runner) -> None:
    eth_test_runner.run_ethernet_test_apps(dut)
    dut.serial.hard_reset()
    eth_test_runner.run_ethernet_l2_test(dut, TEST_IF)
    dut.serial.hard_reset()
    eth_test_runner.run_ethernet_heap_alloc_test(dut, TEST_IF)
```