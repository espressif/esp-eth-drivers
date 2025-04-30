idf_component_register(SRCS "test_apps.c" "test_init_ksz8863.c" "ksz8863_console_cmd.c"
                    INCLUDE_DIRS "."
                    PRIV_INCLUDE_DIRS "."
                    PRIV_REQUIRES unity esp_eth esp_netif esp_driver_gpio console)
