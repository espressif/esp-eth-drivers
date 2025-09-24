set(priv_requires esp_eth esp_driver_gpio esp_driver_spi esp_timer)

idf_component_register(SRCS "src/esp_eth_mac_w5500.c"
                            "src/esp_eth_phy_w5500.c"
                       PRIV_REQUIRES ${priv_requires}
                       INCLUDE_DIRS "include")
