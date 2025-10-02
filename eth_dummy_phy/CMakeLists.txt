set (priv_requires "log" "esp_eth")

# Starting from esp-idf v5.3, the GPIO driver is moved to separate components
if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER_EQUAL "5.3")
    list(APPEND priv_requires "esp_driver_gpio")
else()
    list(APPEND priv_requires "driver")
endif()

idf_component_register(SRCS "src/esp_eth_phy_dummy.c"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES ${priv_requires})
