set(priv_requires esp_netif esp_eth)

if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER "5.3")
    list(APPEND priv_requires esp_driver_gpio)
else()
    list(APPEND priv_requires driver)
endif()

idf_component_register(SRCS "generic_switch.c"
                       PRIV_REQUIRES ${priv_requires})
