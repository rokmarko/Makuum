idf_component_register(SRCS "main.c" "led_control.c" "bt_manager.c"
                    INCLUDE_DIRS "."
                    REQUIRES bt nvs_flash esp_driver_gpio esp_timer)