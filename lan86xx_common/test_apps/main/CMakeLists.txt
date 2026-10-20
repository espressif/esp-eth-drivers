set(srcs "")
if(CONFIG_EXAMPLE_LAN86XX_ROLE_SERVER)
    list(APPEND srcs "lan86xx_plca_server.c")
elseif(CONFIG_EXAMPLE_LAN86XX_ROLE_BRIDGE)
    list(APPEND srcs "lan86xx_bridge.c")
else()
    list(APPEND srcs "lan86xx_client.c")
endif()

idf_component_register(SRCS ${srcs}
                    PRIV_REQUIRES esp_eth esp_netif console iperf-cmd
                    INCLUDE_DIRS ".")
