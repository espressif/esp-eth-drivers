set (priv_requires "esp_timer")

# Starting from esp-idf v5.3, the GPIO and SPI drivers are moved to separate components
if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER_EQUAL "5.3")
    list(APPEND priv_requires "esp_driver_gpio" "esp_driver_spi")
else()
    list(APPEND priv_requires "driver")
endif()

idf_component_register(SRCS "src/esp_eth_mac_ch390.c" "src/esp_eth_phy_ch390.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_eth
                    PRIV_REQUIRES ${priv_requires})
