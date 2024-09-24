idf_component_register(SRCS "src/esp_eth_mac_enc28j60.c"
                            "src/esp_eth_phy_enc28j60.c"
                       PRIV_REQUIRES esp_driver_gpio esp_driver_spi esp_eth
                       INCLUDE_DIRS "include")
