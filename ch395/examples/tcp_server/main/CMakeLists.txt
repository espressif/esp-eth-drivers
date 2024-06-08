idf_component_register(SRCS "basic.c" "main.c"
                       INCLUDE_DIRS "."
                       REQUIRES  driver esp_netif esp_eth lwip ch395)