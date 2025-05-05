idf_component_register(SRCS "iperf.c"
                    PRIV_REQUIRES esp_netif esp_eth console
                    INCLUDE_DIRS ".")
