idf_component_register(SRCS "ksz8863_cmd.c"
                       PRIV_REQUIRES esp_driver_gpio esp_eth console esp_netif
                       INCLUDE_DIRS ".")
