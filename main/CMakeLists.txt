idf_component_register(SRCS "hello_world_main.c"
                       INCLUDE_DIRS "."
                       REQUIRES nvs_flash esp_wifi esp_netif esp_event esp_http_server driver)