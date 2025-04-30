#include "string.h"
#include "esp_console.h"
#include "argtable3/argtable3.h"
#include "esp_eth_ksz8863.h"
#include "ksz8863_console_cmd.h"

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

static switch_set_args_t s_set_args;
static switch_start_args_t s_start_args;
static esp_eth_handle_t port_handles[2] = {NULL, NULL};

static int cmd_switch_start(int argc, char **argv)
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

void register_ksz8863_config_commands(esp_eth_handle_t p1_hdl, esp_eth_handle_t p2_hdl)
{
    s_set_args.port = arg_int0(NULL, "port", "<port_num>", "Port in which the change will be made");
    s_set_args.property = arg_str0(NULL, NULL, "<property>", "Property to be set");
    s_set_args.value = arg_int0(NULL, NULL, "<value>", "New value for the property");
    s_set_args.end = arg_end(3);

    s_start_args.port = arg_int0(NULL, "port", "<port_num>", "Port in which the change will be made");
    s_start_args.status = arg_str0(NULL, NULL, "<status>", "Up to enable port and down to disable it"),
    s_start_args.end = arg_end(1);

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
        .func = &cmd_switch_start,
        .argtable = &s_start_args
    };
    
    ESP_ERROR_CHECK(esp_console_cmd_register(&switch_set_cmd));
    ESP_ERROR_CHECK(esp_console_cmd_register(&switch_start_cmd));

    port_handles[0] = p1_hdl;
    port_handles[1] = p2_hdl;
}