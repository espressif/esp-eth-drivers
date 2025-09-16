set(priv_requires esp_eth)

if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER "5.3")
    list(APPEND priv_requires esp_driver_gpio esp_driver_spi esp_timer)
else()
    list(APPEND priv_requires driver esp_timer)
endif()

idf_component_register(SRCS "src/esp_eth_mac_lan865x.c"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES ${priv_requires})
