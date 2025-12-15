idf_component_register(SRCS "src/esp_eth_mac_w5500.c"
                            "src/esp_eth_phy_w5500.c"
                       PRIV_REQUIRES esp_eth
                       INCLUDE_DIRS "include")
