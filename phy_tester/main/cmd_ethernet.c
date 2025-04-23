/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_console.h"
#include "esp_bit_defs.h"
#include "argtable3/argtable3.h"
#include "esp_event.h"
#include "esp_err.h"
#include "esp_check.h"
#include "test_functions.h"
#include "eth_common.h"
#include "ethernet_init.h"

static const char *TAG = "eth_phy_tester_cmd";

static esp_eth_handle_t *s_eth_handles;
static uint8_t s_eth_port_cnt;

static char *supported_phys[PHY_ID_END] = {
    "IP101",        /* PHY_IP101, */
    "LAN87XX",      /* PHY_LAN87XX */
    "KSZ80XX",      /* PHY_KSZ80XX */
    "RTL8201",      /* PHY_RTL8201 */
    "DP83848"       /* PHY_DP83848 */
};

static struct {
    struct arg_str *info;
    struct arg_lit *read;
    struct arg_lit *write;
    struct arg_int *addr;
    struct arg_int *decimal;
    struct arg_str *hex;
    struct arg_end *end;
} phy_control_args;

/* "dump_regs" command */
static struct {
    struct arg_lit *dump_802_3;
    struct arg_int *dump_range_start;
    struct arg_int *dump_range_stop;
    struct arg_end *end;
} phy_dump_regs_args;

static struct {
    struct arg_lit *enable;
    struct arg_lit *disable;
    struct arg_end *end;
} phy_farend_loopback_args;

static struct {
    struct arg_lit *enable;
    struct arg_lit *disable;
    struct arg_end *end;
} phy_nearend_loopback_args;

static struct {
    struct arg_int *length;
    struct arg_int *count;
    struct arg_int *interval;
    struct arg_lit *verbose;
    struct arg_end *end;
} phy_nearend_loopback_test_args;

static struct {
    struct arg_int *length;
    struct arg_int *count;
    struct arg_int *interval;
    struct arg_lit *verbose;
    struct arg_end *end;
} dummy_transmit_args;

static struct {
    struct arg_int *timeout_ms;
    struct arg_str *eth_type_filter;
    struct arg_lit *verbose;
    struct arg_end *end;
} loop_server_args;

static struct {
    struct arg_int *verbosity;
    struct arg_end *end;
} verbosity_args;

// TODO
static esp_err_t check_eth_state(void)
{
    if (s_eth_handles == NULL) {
        ESP_LOGW(TAG, "Ethernet init failed, command is not available");
        return ESP_ERR_INVALID_STATE;
    }
    return ESP_OK;
}

static esp_err_t check_phy_reg_addr_valid(int addr)
{
    if (addr >= 0 && addr <= 31) {
        return ESP_OK;
    }
    ESP_LOGE(TAG, "invalid PHY register address range");
    return ESP_FAIL;
}

static int phy_cmd_control(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&phy_control_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, phy_control_args.end, argv[0]);
        return 1;
    }

    if (!strncmp(phy_control_args.info->sval[0], "info", 4)) {
        // Get the device information of the ethernet handle
        eth_dev_info_t eth_info = ethernet_init_get_dev_info(s_eth_handles[0]);

        printf("--- PHY Chip under Test ---\n");
        printf("Name: %s\n", eth_info.name);
        printf("MCD pin: %u\n", eth_info.pin.eth_internal_mdc);
        printf("MDIO pin: %u\n", eth_info.pin.eth_internal_mdio);
    }

    if (phy_control_args.read->count != 0) {
        if (phy_control_args.addr->count != 0) {
            if (check_phy_reg_addr_valid(phy_control_args.addr->ival[0]) != ESP_OK) {
                return 1;
            }
            dump_phy_regs(s_eth_handles[0], phy_control_args.addr->ival[0], phy_control_args.addr->ival[0]);
        } else {
            ESP_LOGE(TAG, "register address is missing");
        }
    }

    if (phy_control_args.write->count != 0) {
        if (phy_control_args.addr->count != 0) {
            if (check_phy_reg_addr_valid(phy_control_args.addr->ival[0]) != ESP_OK) {
                return 1;
            }
            int data;
            if (phy_control_args.decimal->count != 0 && phy_control_args.hex->count == 0) {
                data = phy_control_args.decimal->ival[0];
            } else if (phy_control_args.decimal->count == 0 && phy_control_args.hex->count != 0) {
                data = strtol(phy_control_args.hex->sval[0], NULL, 16);
            } else {
                ESP_LOGE(TAG, "invalid combination of register data formats");
                return -1;
            }
            write_phy_reg(s_eth_handles[0], phy_control_args.addr->ival[0], data);
        } else {
            ESP_LOGE(TAG, "register address is missing");
        }
    }
    return 0;
}

static int phy_dump(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&phy_dump_regs_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, phy_dump_regs_args.end, argv[0]);
        return 1;
    }

    if (phy_dump_regs_args.dump_802_3->count != 0) {
        dump_phy_regs(s_eth_handles[0], 0, 15);
    } else if (phy_dump_regs_args.dump_range_start->count != 0 && phy_dump_regs_args.dump_range_stop->count != 0) {
        dump_phy_regs(s_eth_handles[0], phy_dump_regs_args.dump_range_start->ival[0], phy_dump_regs_args.dump_range_stop->ival[0]);
    } else if (phy_dump_regs_args.dump_range_start->count != 0) {
        dump_phy_regs(s_eth_handles[0], phy_dump_regs_args.dump_range_start->ival[0], phy_dump_regs_args.dump_range_start->ival[0]);
    } else {
        ESP_LOGE(TAG, "invalid arguments");
    }

    return 0;
}

static int nearend_loopback_test(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&phy_nearend_loopback_test_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, phy_nearend_loopback_test_args.end, argv[0]);
        return 1;
    }

    // default configuration
    uint32_t length = 256;
    uint32_t count = 10;
    uint32_t interval = 1000000; // us => 1000 ms
    bool verbose = false;

    if (phy_nearend_loopback_test_args.length->count != 0) {
        length = phy_nearend_loopback_test_args.length->ival[0];
    }

    if (phy_nearend_loopback_test_args.count->count != 0) {
        count = phy_nearend_loopback_test_args.count->ival[0];
    }

    if (phy_nearend_loopback_test_args.interval->count != 0) {
        if (phy_nearend_loopback_test_args.interval->ival[0] >= 10000) {
            interval = phy_nearend_loopback_test_args.interval->ival[0];
        } else {
            ESP_LOGW(TAG, "currently, 10ms is the smallest interval to be set");
            interval = 10000; // 10 ms
        }
    }

    if (phy_nearend_loopback_test_args.verbose->count != 0) {
        verbose = true;
    }
    loopback_near_end_test(s_eth_handles[0], verbose, length, count, interval);

    return 0;
}

static int dummy_transmit(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&dummy_transmit_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, dummy_transmit_args.end, argv[0]);
        return 1;
    }

    // default configuration
    uint32_t length = 256;
    uint32_t count = 10;
    uint32_t interval = 1000000; // us => 1000 ms
    bool verbose = false;

    if (dummy_transmit_args.length->count != 0) {
        length = dummy_transmit_args.length->ival[0];
    }

    if (dummy_transmit_args.count->count != 0) {
        count = dummy_transmit_args.count->ival[0];
    }

    if (dummy_transmit_args.interval->count != 0) {
        if (dummy_transmit_args.interval->ival[0] >= 10000) {
            interval = dummy_transmit_args.interval->ival[0];
        } else {
            ESP_LOGW(TAG, "currently, 10ms is the smallest interval to be set");
            interval = 10000; // 10 ms
        }
    }

    if (dummy_transmit_args.verbose->count != 0) {
        verbose = true;
    }
    transmit_to_host(s_eth_handles[0], verbose, length, count, interval);

    return 0;
}

static int nearend_loopback_enable(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&phy_nearend_loopback_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, phy_nearend_loopback_args.end, argv[0]);
        return 1;
    }

    phy_id_t phy_id;
    eth_dev_info_t eth_info = ethernet_init_get_dev_info(s_eth_handles[0]);
    for (phy_id = 0; phy_id < PHY_ID_END; phy_id++) {
        if (strcmp(eth_info.name, supported_phys[phy_id]) == 0) {
            break;
        }
    }
    if (phy_id >= PHY_ID_END) {
        ESP_LOGE(TAG, "unsupported PHY");
        return 1;
    }

    // TODO add check that only one option at a time
    if (phy_nearend_loopback_args.enable->count != 0) {
        loopback_near_end_en(s_eth_handles[0], 0, true);
    }

    if (phy_nearend_loopback_args.disable->count != 0) {
        loopback_near_end_en(s_eth_handles[0], 0, false);
    }

    return 0;
}

static int farend_loopback_enable(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&phy_farend_loopback_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, phy_farend_loopback_args.end, argv[0]);
        return 1;
    }

    phy_id_t phy_id;
    eth_dev_info_t eth_info = ethernet_init_get_dev_info(s_eth_handles[0]);
    for (phy_id = 0; phy_id < PHY_ID_END; phy_id++) {
        if (strcmp(eth_info.name, supported_phys[phy_id]) == 0) {
            break;
        }
    }
    if (phy_id >= PHY_ID_END) {
        ESP_LOGE(TAG, "unsupported PHY");
        return 1;
    }

    if (phy_farend_loopback_args.enable->count != 0) {
        loopback_far_end_en(s_eth_handles[0], phy_id, true);
    }

    if (phy_farend_loopback_args.disable->count != 0) {
        loopback_far_end_en(s_eth_handles[0], phy_id, false);
    }

    return 0;
}

static int loop_server_start(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&loop_server_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, loop_server_args.end, argv[0]);
        return 1;
    }

    // default values
    uint32_t timeout_ms = 5000;
    uint16_t eth_type_filter = 0xFFFF; // don't filter
    bool verbose = false;

    if (loop_server_args.timeout_ms->count != 0) {
        timeout_ms = loop_server_args.timeout_ms->ival[0];
    }
    if (loop_server_args.eth_type_filter->count != 0) {
        eth_type_filter = strtol(loop_server_args.eth_type_filter->sval[0], NULL, 16);
    }
    if (loop_server_args.verbose->count != 0) {
        verbose = true;
    }

    loop_server(s_eth_handles[0], verbose, eth_type_filter, timeout_ms);
    return 0;
}

static int set_verbosity(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&verbosity_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, verbosity_args.end, argv[0]);
        return 1;
    }

    if (verbosity_args.verbosity->count != 0) {
        esp_log_level_t verbosity_level = verbosity_args.verbosity->ival[0];
        if (verbosity_level >= ESP_LOG_NONE && verbosity_level <= ESP_LOG_VERBOSE) {
            esp_log_level_set("*", verbosity_level);
        } else {
            ESP_LOGE(TAG, "invalid range of ESP log verbosity level");
        }
    }

    return 0;
}

void register_ethernet(void)
{
    // Create default event loop that running in background
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Initialize Ethernet driver
    esp_err_t ret = ethernet_init_all(&s_eth_handles, &s_eth_port_cnt);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Ethernet init successful!");
    } else {
        ESP_LOGE(TAG, "Ethernet init failed!");
        goto err;
    }

    if (s_eth_port_cnt > 1) {
        ESP_LOGE(TAG, "only one PHY can be tested");
        goto err;
    }
    // Get the device information of the ethernet handle
    eth_dev_info_t eth_info = ethernet_init_get_dev_info(s_eth_handles[0]);
    if (eth_info.type == ETH_DEV_TYPE_SPI) {
        ESP_LOGE(TAG, "test of SPI modules is not supported");
        goto err;
    }

    phy_control_args.info = arg_str0(NULL, NULL, "<info>", "Get info of Ethernet");
    phy_control_args.read = arg_lit0(NULL, "read", "read PHY register");
    phy_control_args.write = arg_lit0(NULL, "write", "write PHY register");
    phy_control_args.addr = arg_int0("a", NULL, "<address>", "register address (used in combination with read/write)");
    phy_control_args.decimal = arg_int0("d", NULL, "<data>", "register data in dec format (used in combination with write)");
    phy_control_args.hex = arg_str0("h", NULL, "<data in hex>", "register data in hex format (used in combination with write)");
    phy_control_args.end = arg_end(1);
    const esp_console_cmd_t cmd = {
        .command = "phy",
        .help = "Ethernet PHY control",
        .hint = NULL,
        .func = phy_cmd_control,
        .argtable = &phy_control_args
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));

    phy_dump_regs_args.dump_802_3 = arg_lit0("a", "all", "Dump IEEE 802.3 registers");
    phy_dump_regs_args.dump_range_start = arg_int0(NULL, NULL, "<first reg>", "Dump a range of registers start addr");
    phy_dump_regs_args.dump_range_stop = arg_int0(NULL, NULL, "<last reg>", "Dump a range of registers end addr");
    phy_dump_regs_args.end = arg_end(1);
    const esp_console_cmd_t dump_cmd = {
        .command = "dump",
        .help = "Dump PHY registers",
        .hint = NULL,
        .func = phy_dump,
        .argtable = &phy_dump_regs_args
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&dump_cmd));

    phy_nearend_loopback_args.enable = arg_lit0("e", "enable", "enable near-end loopback");
    phy_nearend_loopback_args.disable = arg_lit0("d", "disable", "disable near-end loopback");
    phy_nearend_loopback_args.end = arg_end(1);
    const esp_console_cmd_t nearend_loopback_cmd = {
        .command = "near-loop-en",
        .help = "Enables near-end loopback, frames are looped at R/MII PHY back to ESP32",
        .hint = NULL,
        .func = nearend_loopback_enable,
        .argtable = &phy_nearend_loopback_args
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&nearend_loopback_cmd));

    phy_farend_loopback_args.enable = arg_lit0("e", "enable", "enable far-end loopback");
    phy_farend_loopback_args.disable = arg_lit0("d", "disable", "disable far-end loopback");
    phy_farend_loopback_args.end = arg_end(1);
    const esp_console_cmd_t farend_loopback_cmd = {
        .command = "farend-loop-en",
        .help = "Enables far-end loopback, frames are looped at PHY back to host",
        .hint = NULL,
        .func = farend_loopback_enable,
        .argtable = &phy_farend_loopback_args
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&farend_loopback_cmd));

    phy_nearend_loopback_test_args.length = arg_int0("s", "size", "<size>", "size of the frame");
    phy_nearend_loopback_test_args.count = arg_int0("c", "count", "<count>", "number of frames to be loopedback");
    phy_nearend_loopback_test_args.interval = arg_int0("i", "interval", "<interval_us>", "microseconds between sending each frame");
    phy_nearend_loopback_test_args.verbose = arg_lit0("v", "verbose", "enable verbose test output");
    phy_nearend_loopback_test_args.end = arg_end(1);
    const esp_console_cmd_t nearend_loopback_test_cmd = {
        .command = "loop-test",
        .help = "Runs Loopback test, frames are looped by PHY back to ESP32 (near-end loopback)",
        .hint = NULL,
        .func = nearend_loopback_test,
        .argtable = &phy_nearend_loopback_test_args
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&nearend_loopback_test_cmd));

    dummy_transmit_args.length = arg_int0("s", "size", "<size>", "size of the frame");
    dummy_transmit_args.count = arg_int0("c", "count", "<count>", "number of frames to be transmitted");
    dummy_transmit_args.interval = arg_int0("i", "interval", "<interval_us>", "microseconds between sending each frame");
    dummy_transmit_args.verbose = arg_lit0("v", "verbose", "enable verbose test output");
    dummy_transmit_args.end = arg_end(1);
    const esp_console_cmd_t dummy_transmit_cmd = {
        .command = "dummy-tx",
        .help = "Transmits dummy test frames",
        .hint = NULL,
        .func = dummy_transmit,
        .argtable = &dummy_transmit_args
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&dummy_transmit_cmd));

    loop_server_args.timeout_ms = arg_int0("t", "timeout", "<msec>", "receive timeout (if no message is received, loop is closed)");
    loop_server_args.eth_type_filter = arg_str0("f", "filter", "<Ethertype in hex>", "Ethertype in hex to be filtered at recv function (FFFF to not filter)");
    loop_server_args.verbose = arg_lit0("v", "verbose", "enable verbose test output");
    loop_server_args.end = arg_end(1);
    const esp_console_cmd_t loop_server_cmd = {
        .command = "loop-server",
        .help = "Start a Ethernet loop `server`",
        .hint = NULL,
        .func = loop_server_start,
        .argtable = &loop_server_args
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&loop_server_cmd));

    verbosity_args.verbosity = arg_int0("l", "level", "<0-6>", "set ESP logs verbosity level");
    verbosity_args.end = arg_end(1);
    const esp_console_cmd_t verbosity_cmd = {
        .command = "verbosity",
        .help = "set ESP log verbosity level",
        .hint = NULL,
        .func = set_verbosity,
        .argtable = &verbosity_args
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&verbosity_cmd));

    return;
err:
    if (s_eth_handles) {
        ethernet_deinit_all(s_eth_handles);
    }
    esp_event_loop_delete_default();
    return;
}
