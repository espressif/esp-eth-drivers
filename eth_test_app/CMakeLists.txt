set(requires unity esp_eth esp_netif esp_event)
set(priv_requires esp_http_client esp_driver_gpio esp_driver_uart)

idf_component_register(SRCS "src/esp_eth_test_apps.c"
                            "src/esp_eth_test_l2.c"
                            "src/esp_eth_test_utils.c"
                       INCLUDE_DIRS "include"
                       REQUIRES ${requires}
                       PRIV_REQUIRES ${priv_requires}
                       EMBED_TXTFILES "dl_espressif_com_root_cert.pem"
                       WHOLE_ARCHIVE)
