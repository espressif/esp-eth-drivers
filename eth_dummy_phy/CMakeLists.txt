set(priv_requires log esp_eth)

if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER "5.3")
    list(APPEND priv_requires esp_driver_gpio)
else()
    list(APPEND priv_requires driver)
endif()

idf_component_register(SRCS "src/esp_eth_phy_dummy.c"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES ${priv_requires})
