idf_component_register(SRCS "src/esp_eth_phy_ch395.c" "src/esp_eth_mac_ch395.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_eth
                    PRIV_REQUIRES esp_timer)
