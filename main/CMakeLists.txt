idf_component_register(SRCS "main.c""app.c" "_ble.c" "sensor.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES bt nvs_flash esp_driver_i2c)
