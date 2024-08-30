idf_component_register(SRCS "src/esp_eth_phy_ch182.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_eth driver
                    PRIV_REQUIRES esp_timer)