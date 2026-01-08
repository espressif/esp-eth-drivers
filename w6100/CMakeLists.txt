idf_component_register(SRCS "src/esp_eth_mac_w6100.c"
                            "src/esp_eth_phy_w6100.c"
                       PRIV_REQUIRES esp_eth
                       INCLUDE_DIRS "include")
