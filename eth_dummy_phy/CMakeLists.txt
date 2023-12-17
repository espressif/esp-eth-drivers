idf_component_register(SRCS "src/esp_eth_phy_dummy.c"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES log esp_eth driver)