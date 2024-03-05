idf_component_register(SRCS "src/esp_eth_mac_ch390.c" "src/esp_eth_phy_ch390.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES esp_eth esp_timer)