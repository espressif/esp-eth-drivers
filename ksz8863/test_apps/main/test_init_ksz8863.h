#ifndef TEST_INIT_KSZ8863_H_
#define TEST_INIT_KSZ8863_H_
#include "esp_err.h"
#include "esp_netif.h"
#include "esp_eth.h"

esp_err_t init_ksz8863_interface();
esp_err_t init_ksz8863_auto(esp_netif_t *eth_netif, esp_eth_handle_t *host_eth_handle_ptr, esp_eth_handle_t *p1_eth_handle_ptr, esp_eth_handle_t *p2_eth_handle_ptr);

#endif