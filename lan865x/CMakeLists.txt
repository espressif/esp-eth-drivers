idf_component_register(SRCS "src/esp_eth_mac_lan865x.c"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES log esp_eth esp_timer esp_driver_gpio)
