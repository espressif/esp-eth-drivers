/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "string.h"
#include "esp_console.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_system.h"
#include "argtable3/argtable3.h"
#include "esp_eth_ksz8863.h"
#include "ksz8863_cmd.h"

static const char *TAG = "ksz8863_test_apps";

typedef struct {
    struct arg_int *port;
    struct arg_rex *action;
    struct arg_rex *parameter;
    struct arg_str *value;
    struct arg_end *end;
} switch_args_t;

static switch_args_t s_switch_args;
static esp_eth_handle_t s_host_handle;
static esp_eth_handle_t s_port_handles[2] = {NULL, NULL};

static int cmd_switch(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &s_switch_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, s_switch_args.end, argv[0]);
        return 1;
    }

    const int port = s_switch_args.port->ival[0];
    const char *action = s_switch_args.action->sval[0];
    const char *parameter = s_switch_args.parameter->sval[0];

    if (port >= 1 && port <= 2) {
        fprintf(stderr, "Error: Unexpected value of --port: %d. Expected either 1 or 2.\n", port);
        return -1;
    }

    if (strcmp(action, "set") == 0) {
        // set parameters such as tx/rx status
        if (strcmp(parameter, "rx") == 0) {
            bool new_value = strtol(s_switch_args.value->sval[0], NULL, 10) == 1;
            esp_eth_ioctl(s_port_handles[port - 1], KSZ8863_ETH_CMD_S_RX_EN, &new_value);
        } else if (strcmp(parameter, "tx") == 0) {
            bool new_value = strtol(s_switch_args.value->sval[0], NULL, 10) == 1;
            esp_eth_ioctl(s_port_handles[port - 1], KSZ8863_ETH_CMD_S_TX_EN, &new_value);
        } else if (strcmp(parameter, "tailtag") == 0) {
            bool new_value = strtol(s_switch_args.value->sval[0], NULL, 10) == 1;
            esp_eth_ioctl(s_port_handles[port - 1], KSZ8863_ETH_CMD_S_TAIL_TAG, &new_value);
        } else if (strcmp(parameter, "learning") == 0) {
            bool new_value = strtol(s_switch_args.value->sval[0], NULL, 10) == 0;
            esp_eth_ioctl(s_port_handles[port - 1], KSZ8863_ETH_CMD_S_LEARN_DIS, &new_value);
        } else if (strcmp(parameter, "enabled") == 0) {
            bool new_value = strtol(s_switch_args.value->sval[0], NULL, 10) == 1;
            esp_eth_ioctl(s_host_handle, KSZ8863_ETH_CMD_S_START_SWITCH, &new_value);
        } else if (strcmp(parameter, "macstatbl") == 0) {
            int index;
            uint8_t mac[ETH_ADDR_LEN];
            char ports_b0;
            char ports_b1;
            char ports_b2;
            char va;
            char ov;
            char uf;
            int fid;
            sscanf(s_switch_args.value->sval[0], "%d %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx %c%c%c %c%c%c %d", &index, &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5], &ports_b2, &ports_b1, &ports_b0, &va, &ov, &uf, &fid);
            if (index < 0 || index > 7) {
                fprintf(stderr, "Invalid index provided - %d. Index must be in range 0 .. 7\n", index);
                return -1;
            }
            printf("Entry at %d\n", index);
            printf("|-MAC: %02x:%02x:%02x:%02x:%02x:%02x\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            printf("|-Forward ports: %c%c%c (%d)\n", ports_b2, ports_b1, ports_b0, (ports_b2 == '1' ? 0b100 : 0) + (ports_b1 == '1' ? 0b010 : 0) + (ports_b0 == '1' ? 0b001 : 0));
            printf("|-Valid: %c\n", va == 'E' ? 'T' : 'F');
            printf("|-Override: %c\n", ov == 'O' ? 'T' : 'F');
            printf("|-Use FID: %c\n", uf == 'F' ? 'T' : 'F');
            printf("|-FID: %d\n", fid);
            ksz8863_sta_mac_table_t sta_mac_tbl = { .mac_addr[0] = mac[0],
                                                    .mac_addr[1] = mac[1],
                                                    .mac_addr[2] = mac[2],
                                                    .mac_addr[3] = mac[3],
                                                    .mac_addr[4] = mac[4],
                                                    .mac_addr[5] = mac[5],
                                                    .fwd_ports = (ports_b2 == '1' ? 0b100 : 0) + (ports_b1 == '1' ? 0b010 : 0) + (ports_b0 == '1' ? 0b001 : 0),
                                                    .entry_val = (va == 'E'),
                                                    .override = (ov == 'O'),
                                                    .use_fid = (uf == 'F'),
                                                    .fid = fid
                                                  };
            ksz8863_mac_tbl_info_t set_tbl_info = {
                .start_entry = index,  // set index of the entry to set
                .entries_num = 1,   // write only one entry
                .sta_tbls = &sta_mac_tbl,
            };
            esp_eth_ioctl(s_port_handles[0], KSZ8863_ETH_CMD_S_MAC_STA_TBL, &set_tbl_info);
        }
    } else if (strcmp(action, "reset") == 0) {
        // perform either hard or soft reset of the switch
        if (strcmp(parameter, "soft") == 0) {
            ksz8863_sw_reset(s_host_handle);
        } else if (strcmp(parameter, "hard") == 0) {
            ESP_LOGW(TAG, "WIP feature");
        } else {
            printf("Invalid argument provided \"%s\"\n\n", parameter);
            return 1;
        }
    } else if (strcmp(action, "show") == 0) {
        if (strcmp(parameter, "rx") == 0) {
            bool value;
            esp_eth_ioctl(s_port_handles[port - 1], KSZ8863_ETH_CMD_G_RX_EN, &value);
            printf("Port %d rx - %s\n", port, value ? "ON" : "OFF");
        } else if (strcmp(parameter, "tx") == 0) {
            bool value;
            esp_eth_ioctl(s_port_handles[port - 1], KSZ8863_ETH_CMD_G_TX_EN, &value);
            printf("Port %d tx - %s\n", port, value ? "ON" : "OFF");
        } else if (strcmp(parameter, "tailtag") == 0) {
            bool value;
            esp_eth_ioctl(s_port_handles[port - 1], KSZ8863_ETH_CMD_G_TAIL_TAG, &value);
            printf("Port %d tail tag - %s\n", port, value ? "ON" : "OFF");
        } else if (strcmp(parameter, "learning") == 0) {
            bool value;
            esp_eth_ioctl(s_port_handles[port - 1], KSZ8863_ETH_CMD_G_LEARN_DIS, &value);
            printf("Port %d learning - %s\n", port, !value ? "ON" : "OFF");
        } else if (strcmp(parameter, "enabled") == 0) {
            bool value;
            esp_eth_ioctl(s_host_handle, KSZ8863_ETH_CMD_G_START_SWITCH, &value);
            printf("Port %d is %s\n", port, value ? "enabled" : "disabled");
        } else if (strcmp(parameter, "macstatbl") == 0) {
            ksz8863_sta_mac_table_t sta_mac_tbls[8];
            ksz8863_mac_tbl_info_t get_tbl_info = {
                .start_entry = 0,  // read from the first entry
                .entries_num = 8,   // read all 8 entries
                .sta_tbls = sta_mac_tbls,
            };
            esp_eth_ioctl(s_port_handles[0], KSZ8863_ETH_CMD_G_MAC_STA_TBL, &get_tbl_info);
            ESP_LOGI(TAG, "Static MAC Table content:");
            for (int i = 0; i < 8; i++) {
                ESP_LOGI(TAG, "%d: %02x:%02x:%02x:%02x:%02x:%02x %c%c%c %c%c%c FID: %d", i + 1,   sta_mac_tbls[i].mac_addr[0],
                         sta_mac_tbls[i].mac_addr[1],
                         sta_mac_tbls[i].mac_addr[2],
                         sta_mac_tbls[i].mac_addr[3],
                         sta_mac_tbls[i].mac_addr[4],
                         sta_mac_tbls[i].mac_addr[5],
                         (sta_mac_tbls[i].fwd_ports & 0b100) ? '1' : '0',
                         (sta_mac_tbls[i].fwd_ports & 0b010) ? '1' : '0',
                         (sta_mac_tbls[i].fwd_ports & 0b001) ? '1' : '0',
                         sta_mac_tbls[i].entry_val ? 'E' : '-',
                         sta_mac_tbls[i].override ? 'O' : '-',
                         sta_mac_tbls[i].use_fid ? 'F' : '-',
                         sta_mac_tbls[i].fid);
            }

        } else if (strcmp(parameter, "macdyntbl") == 0) {
            int count = strtol(s_switch_args.value->sval[0], NULL, 10);
            ksz8863_dyn_mac_table_t *dyn_mac_tbls = malloc(count * sizeof(ksz8863_dyn_mac_table_t));
            ksz8863_mac_tbl_info_t get_tbl_info = {
                .start_entry = 0,  // read from the first entry
                .entries_num = count,   // read the entries
                .dyn_tbls = dyn_mac_tbls,
            };
            esp_eth_ioctl(s_port_handles[0], KSZ8863_ETH_CMD_G_MAC_DYN_TBL, &get_tbl_info);
            ESP_LOGI(TAG, "Dynamic MAC Table content:");
            ESP_LOGI(TAG, "valid entries %" PRIu16, dyn_mac_tbls[0].val_entries + 1);
            int entries_to_display = dyn_mac_tbls[0].val_entries > count ? count : dyn_mac_tbls[0].val_entries + 1;
            for (int i = 0; i < entries_to_display; i++) {
                ESP_LOGI(TAG, "port %" PRIu8, dyn_mac_tbls[i].src_port + 1);
                ESP_LOG_BUFFER_HEX(TAG, dyn_mac_tbls[i].mac_addr, 6);
            }
            printf("\n");
            free(dyn_mac_tbls);
        } else {
            printf("Invalid argument provided \"%s\"\n\n", parameter);
            return 1;
        }
    } else {
        fprintf(stderr, "Invalid argument provided.\n");
        return 1;
    }
    return 0;
}

void register_ksz8863_config_commands(esp_eth_handle_t h_handle, esp_eth_handle_t p1_handle, esp_eth_handle_t p2_handle)
{
    s_host_handle = h_handle;
    s_port_handles[0] = p1_handle;
    s_port_handles[1] = p2_handle;

    s_switch_args.port = arg_int0("p", "port", "<int 1-2>", "Port for which the parameter will be set");
    s_switch_args.action = arg_rex1(NULL, NULL, "(reset|set|show)", "<str>", 0, "reset/set show");
    s_switch_args.parameter = arg_rex1(NULL, NULL, "(tx|rx|tailtag|learning|enabled|macstatbl|macdyntbl|soft|hard)", "<str>", 0, "rx <int> / tx <int> / tailtag <int> / learning <int> / enabled <int> / macstatbl \"<0-7> <mac> <ports> <[E]nable/-><[O]vveride/-><use [F]id/-> <fid 0-15>\" | macdyntbl <show only> / soft (reset only) / hard (reset only)");
    s_switch_args.value = arg_str0(NULL, NULL, "<value>", "New value for the parameter");
    s_switch_args.end = arg_end(4);

    const esp_console_cmd_t s_switch_cmd = {
        .command = "switch",
        .help = "Control the KSZ8863 switch",
        .hint = NULL,
        .func = &cmd_switch,
        .argtable = &s_switch_args
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&s_switch_cmd));
}
