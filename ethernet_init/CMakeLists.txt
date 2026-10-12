idf_component_register(SRCS "src/ethernet_init.c"
                       PRIV_REQUIRES esp_driver_gpio esp_driver_spi esp_eth
                       INCLUDE_DIRS "include"
                       PRIV_INCLUDE_DIRS "src")
