# eth_test_app example

Minimal ESP-IDF test project that runs the **eth_test_app** Ethernet test suite (Unity apps, L2, heap allocation) via `EthTestRunner`. Use it as a **starting point when adding tests for a specific PHY chip**.

## Create a new test project from this example

From your driver repo (or any IDF project directory):

```bash
idf.py create-project-from-example "espressif/eth_test_app=*:test_apps_example"
```

Then open the created project and adapt:

- Rename `pytest_ethernet.py` to the chip-under-test name (e.g. `pytest_dm9051.py`).
- **TEST_IF** in `pytest_ethernet.py` – host Ethernet interface for L2/heap tests (`ip -c a` to list) or keep empty.
- **config** and **pytest marks** – set the sdkconfig preset and PHY marker for your chip (e.g. `pytest.mark.eth_dm9051`, `pytest.mark.eth_ip101`).

See the `CONFIGURE FOR YOUR CHIP` block at the top of `pytest_ethernet.py` for details.
