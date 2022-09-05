idf_build_get_property(components_to_build BUILD_COMPONENTS)

set(srcs)
set(include)
# As CONFIG_ETH_USE_ESP32_EMAC comes from Kconfig, it is not evaluated yet
# when components are being registered.
# Thus, always add the (private) requirements, regardless of Kconfig
set(priv_requires driver log esp_eth)

# If Ethernet disabled in Kconfig, this is a config-only component
if(CONFIG_ETH_USE_ESP32_EMAC)
    set(srcs "src/esp_eth_ksz8863.c"
             "src/ksz8863_ctrl.c"
             "src/esp_eth_phy_ksz8863.c"
             "src/esp_eth_pmac_ksz8863.c")
    set(include "include")

    if(NOT CMAKE_BUILD_EARLY_EXPANSION)
        # esp_netif related
        if(esp_netif IN_LIST components_to_build)
            list(APPEND srcs "src/esp_eth_netif_glue_ksz8863.c")
            list(APPEND priv_requires esp_netif)
        endif()
    endif()
endif()

idf_component_register(SRCS "${srcs}"
                       INCLUDE_DIRS ${include}
		       REQUIRES "esp_event" "esp_netif" # For using "ESP_EVENT_DECLARE_BASE" in header file
                       PRIV_REQUIRES ${priv_requires})
