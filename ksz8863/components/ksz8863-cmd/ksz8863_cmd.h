#ifndef _KSZ8863_CONSOLE_CMD_H_
#define _KSZ8863_CONSOLE_CMD_H_

#include <stdint.h>
#include "esp_eth.h"

#ifdef __cplusplus
extern "C" {
#endif

// Register Bridge configuration commands
void register_ksz8863_config_commands(esp_eth_handle_t h_handle, esp_eth_handle_t p1_handle, esp_eth_handle_t p2_handle);

#ifdef __cplusplus
}
#endif


#endif