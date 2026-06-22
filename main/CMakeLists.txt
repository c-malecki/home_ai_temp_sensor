idf_component_register(SRCS "oled.c" "sensor.c" "app.c" "_ble.c" "main.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES bt nvs_flash esp_driver_i2c)
