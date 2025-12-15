set(srcs "src/wiznet_spi.c"
         "src/wiznet_mac_common.c"
         "src/wiznet_phy_common.c")

set(priv_requires esp_driver_spi
                  esp_driver_gpio
                  esp_eth
                  esp_timer)

idf_component_register(SRCS ${srcs}
                       PRIV_REQUIRES ${priv_requires}
                       INCLUDE_DIRS "include"
                       PRIV_INCLUDE_DIRS "src")
