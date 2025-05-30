idf_component_register(SRCS "ksz8863_console_ctrl.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES esp_netif esp_eth ksz8863)
