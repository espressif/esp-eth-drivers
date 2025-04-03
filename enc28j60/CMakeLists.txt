set(priv_requires esp_eth)

if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER "5.3")
    list(APPEND priv_requires esp_driver_gpio esp_driver_spi)
else()
    list(APPEND priv_requires driver)
endif()

idf_component_register(SRCS "src/esp_eth_mac_enc28j60.c"
                            "src/esp_eth_phy_enc28j60.c"
                       PRIV_REQUIRES ${priv_requires}
                       INCLUDE_DIRS "include")
