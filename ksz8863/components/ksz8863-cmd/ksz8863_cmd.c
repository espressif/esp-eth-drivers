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

/*typedef struct {
    struct arg_str *action;
    struct arg_str *mode;
    struct arg_end *end;
} switch_init_args_t;

typedef struct {
    struct arg_str *type;
    struct arg_end *end;
} switch_reset_args_t;

typedef struct {
    struct arg_int *port;
    struct arg_str *status;
    struct arg_end *end;
} switch_start_args_t;

typedef struct {
    struct arg_int *port;
    struct arg_str *property;
    struct arg_int *value;
    struct arg_end *end;
} switch_set_args_t;

static switch_init_args_t s_init_args;
static switch_reset_args_t s_reset_args;
static switch_set_args_t s_set_args;
static switch_start_args_t s_start_args;*/
typedef struct {
    struct arg_int *port;
    struct arg_rex *action;
    struct arg_rex *parameter;
    struct arg_str *value;
    struct arg_end *end;
} switch_args_t;

static switch_args_t s_switch_args;
static esp_eth_handle_t host_handle;
static esp_eth_handle_t port_handles[2] = {NULL, NULL};

/*static int cmd_switch_reset(int argc, char** argv)
{
    printf("argc: %d\n", argc);
    for(int i = 0; i < argc; i++) {
        printf("%s | ", argv[i]);
    }
    fflush(stdout);
    int nerrors = arg_parse(argc, argv, (void **) &s_reset_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, s_reset_args.end, argv[0]);
        return 1;
    }
    if(strcmp(s_reset_args.type->sval[0], "soft") == 0) {
        ksz8863_sw_reset(host_handle);
    } else if(strcmp(s_reset_args.type->sval[0], "hard") == 0) {
        //ksz8863_hw_reset();
    }
    return 0;
}

static int cmd_switch_bring(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &s_start_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, s_start_args.end, argv[0]);
        return 1;
    }

    bool new_val = false;
    if(strcmp(s_start_args.status->sval[0], "up") == 0) {
        new_val = true;
    } else if(strcmp(s_start_args.status->sval[0], "down") == 0) {
        new_val = false;
    }
    esp_eth_ioctl(port_handles[s_start_args.port->ival[0] - 1], KSZ8863_ETH_CMD_S_START_SWITCH, &new_val);
    return 0;
}

static int cmd_switch_set(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &s_set_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, s_set_args.end, argv[0]);
        return 1;
    }

    if(strcmp(s_set_args.property->sval[0], "rx") == 0) {
        bool newval = s_set_args.value->ival[0] == 1;
        esp_eth_ioctl(port_handles[s_set_args.port->ival[0] - 1], KSZ8863_ETH_CMD_S_RX_EN, &newval);
    } else if(strcmp(s_set_args.property->sval[0], "tx") == 0) {
        bool newval = s_set_args.value->ival[0] == 1;
        esp_eth_ioctl(port_handles[s_set_args.port->ival[0] - 1], KSZ8863_ETH_CMD_S_TX_EN, &newval);
    } else {
        fprintf(stderr, "Invalid argument provided.\n");
        return 1;
    }
    return 0;
}

void register_ksz8863_config_commands(esp_eth_handle_t h_handle, esp_eth_handle_t p1_handle, esp_eth_handle_t p2_handle)
{
    host_handle = h_handle;
    port_handles[0] = p1_handle;
    port_handles[1] = p2_handle;

    s_reset_args.type = arg_str0(NULL, NULL, "<reset type>", "Specify either 'soft' or 'hard' to perform reset");
    s_reset_args.end = arg_end(1);

    s_set_args.port = arg_int0(NULL, "port", "<port_num>", "Port in which the change will be made");
    s_set_args.property = arg_str0(NULL, NULL, "<property>", "Property to be set");
    s_set_args.value = arg_int0(NULL, NULL, "<value>", "New value for the property");
    s_set_args.end = arg_end(3);

    s_start_args.port = arg_int0(NULL, "port", "<port_num>", "Port in which the change will be made");
    s_start_args.status = arg_str0(NULL, NULL, "<status>", "Up to enable port and down to disable it"),
    s_start_args.end = arg_end(1);

    const esp_console_cmd_t switch_reset_cmd = {
        .command = "swreset",
        .help = "Perofrm either soft or hard reset of the switch",
        .hint = NULL,
        .func = &cmd_switch_reset,
        .argtable = &s_reset_args
    };
    const esp_console_cmd_t switch_set_cmd = {
        .command = "set",
        .help = "Set control value for given property",
        .hint = NULL,
        .func = &cmd_switch_set,
        .argtable = &s_set_args
    };
    const esp_console_cmd_t switch_start_cmd = {
        .command = "bring",
        .help = "Bring selected port up or down",
        .hint = NULL,
        .func = &cmd_switch_bring,
        .argtable = &s_start_args
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&switch_reset_cmd));
    ESP_ERROR_CHECK(esp_console_cmd_register(&switch_set_cmd));
    ESP_ERROR_CHECK(esp_console_cmd_register(&switch_start_cmd));
}*/

static int cmd_switch(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &s_switch_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, s_switch_args.end, argv[0]);
        return 1;
    }

    int port = s_switch_args.port->ival[0];
    char *action = s_switch_args.action->sval[0];
    char *parameter = s_switch_args.parameter->sval[0];
    char *value = s_switch_args.value->sval[0];

    if (strcmp(action, "set") == 0) {
        // set parameters such as tx/rx status
        if (strcmp(parameter, "rx") == 0) {
            bool new_value = atoi(s_switch_args.value->sval[0]) == 1;
            esp_eth_ioctl(port_handles[port - 1], KSZ8863_ETH_CMD_S_RX_EN, &new_value);
        } else if (strcmp(parameter, "tx") == 0) {
            bool new_value = atoi(s_switch_args.value->sval[0]) == 1;
            esp_eth_ioctl(port_handles[port - 1], KSZ8863_ETH_CMD_S_TX_EN, &new_value);
        } else if (strcmp(parameter, "tailtag") == 0) {
            bool new_value = atoi(s_switch_args.value->sval[0]) == 1;
            esp_eth_ioctl(port_handles[port - 1], KSZ8863_ETH_CMD_S_TAIL_TAG, &new_value);
        } else if (strcmp(parameter, "learning") == 0) {
            bool new_value = atoi(s_switch_args.value->sval[0]) == 0;
            esp_eth_ioctl(port_handles[port - 1], KSZ8863_ETH_CMD_S_LEARN_DIS, &new_value);
        } else if (strcmp(parameter, "enabled") == 0) {
            bool new_value = atoi(s_switch_args.value->sval[0]) == 1;
            esp_eth_ioctl(host_handle, KSZ8863_ETH_CMD_S_START_SWITCH, &new_value);
        } else if (strcmp(parameter, "macstatbl") == 0) {
            int index;
            uint8_t mac[6];
            char ports_b0;
            char ports_b1;
            char ports_b2;
            char va;
            char ov;
            char uf;
            int fid;
            sscanf(s_switch_args.value->sval[0], "%d %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx %c%c%c %c%c%c %d", &index, &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5], &ports_b2, &ports_b1, &ports_b0, &va, &ov, &uf, &fid);
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
                .etries_num = 1,   // write only one entry
                .sta_tbls = &sta_mac_tbl,
            };
            esp_eth_ioctl(port_handles[0], KSZ8863_ETH_CMD_S_MAC_STA_TBL, &set_tbl_info);
        }
    } else if (strcmp(action, "reset") == 0) {
        // perform either hard or soft reset of the switch
        if (strcmp(parameter, "soft") == 0) {
            ksz8863_sw_reset(host_handle);
        } else if (strcmp(parameter, "hard") == 0) {
            ESP_LOGW(TAG, "WIP feature");
        } else {
            printf("Invalid argument provided \"%s\"\n\n", parameter);
            return 1;
        }
    } else if (strcmp(action, "show") == 0) {
        if (strcmp(parameter, "rx") == 0) {
            bool value;
            esp_eth_ioctl(port_handles[port - 1], KSZ8863_ETH_CMD_G_RX_EN, &value);
            printf("Port %d rx - %s\n", port, value ? "ON" : "OFF");
        } else if (strcmp(parameter, "tx") == 0) {
            bool value;
            esp_eth_ioctl(port_handles[port - 1], KSZ8863_ETH_CMD_G_TX_EN, &value);
            printf("Port %d tx - %s\n", port, value ? "ON" : "OFF");
        } else if (strcmp(parameter, "tailtag") == 0) {
            bool value;
            esp_eth_ioctl(port_handles[port - 1], KSZ8863_ETH_CMD_G_TAIL_TAG, &value);
            printf("Port %d tail tag - %s\n", port, value ? "ON" : "OFF");
        } else if (strcmp(parameter, "tailtag") == 0) {
            bool value;
            esp_eth_ioctl(port_handles[port - 1], KSZ8863_ETH_CMD_G_LEARN_DIS, &value);
            printf("Port %d learning - %s\n", port, !value ? "ON" : "OFF");
        } else if (strcmp(parameter, "enabled") == 0) {
            bool value;
            esp_eth_ioctl(host_handle, KSZ8863_ETH_CMD_G_START_SWITCH, &value);
            printf("Port %d is %s\n", port, value ? "enabled" : "disabled");
        } else if (strcmp(parameter, "macstatbl") == 0) {
            ksz8863_sta_mac_table_t sta_mac_tbls[8];
            ksz8863_mac_tbl_info_t get_tbl_info = {
                .start_entry = 0,  // read from the first entry
                .etries_num = 8,   // read all 8 entries
                .sta_tbls = sta_mac_tbls,
            };
            esp_eth_ioctl(port_handles[0], KSZ8863_ETH_CMD_G_MAC_STA_TBL, &get_tbl_info);
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
            ksz8863_dyn_mac_table_t dyn_mac_tbls[5];
            ksz8863_mac_tbl_info_t get_tbl_info = {
                .start_entry = 0,  // read from the first entry
                .etries_num = 5,   // read 5 entries
                .dyn_tbls = dyn_mac_tbls,
            };
            esp_eth_ioctl(port_handles[0], KSZ8863_ETH_CMD_G_MAC_DYN_TBL, &get_tbl_info);
            ESP_LOGI(TAG, "Dynamic MAC Table content:");
            ESP_LOGI(TAG, "valid entries %" PRIu16, dyn_mac_tbls[0].val_entries + 1);
            for (int i = 0; i < (dyn_mac_tbls[0].val_entries + 1); i++) {
                ESP_LOGI(TAG, "port %" PRIu8, dyn_mac_tbls[i].src_port + 1);
                ESP_LOG_BUFFER_HEX(TAG, dyn_mac_tbls[i].mac_addr, 6);
            }
            printf("\n");
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
    host_handle = h_handle;
    port_handles[0] = p1_handle;
    port_handles[1] = p2_handle;

    s_switch_args.port = arg_int0("p", "port", "<int 1-2>", "Port for which the parameter will be set");
    s_switch_args.action = arg_rex1(NULL, NULL, "(reset|set|show)", "<str>", 0, "reset/set show");
    s_switch_args.parameter = arg_rex1(NULL, NULL, "(tx|rx|tailtag|learning|enabled|macstatbl|macdyntbl|soft|hard)", "<str>", 0, "rx <int> / tx <int> / tailtag <int> / learning <int> / enabled <int> / macstatbl \"<0-7> <mac> <ports> <entry val><override><use fid> <fid 0-15>\" | macdyntbl <show only> / soft (reset only) / hard (reset only)");
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
