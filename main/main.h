#ifndef __MAIN_H_
#define __MAIN_H_

#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/semphr.h"
#include <stdbool.h>
#include "stdint.h"

#define DEVICE_NAME "TEMP_SENSOR1"

#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 22

#define BLE_SVC_UUID 0x1100
#define BLE_CHAR_UUID 0x1101

#define SENSOR_I2C_ADDR (0x45)
#define SENSOR_CMD_MEASURE_MSB (0x24)
#define SENSOR_CMD_MEASURE_LSB (0x00)
#define SENSOR_CMD_SOFT_RESET_MSB (0x30)
#define SENSOR_CMD_SOFT_RESET_LSB (0xA2)

typedef struct
{
    SemaphoreHandle_t data_mutex;
    i2c_master_bus_handle_t i2c_bus_handle;
    i2c_master_dev_handle_t i2c_sensor_handle;

    TickType_t temp_poll_interval;
    float temp_last_value;
    uint32_t temp_last_read;

    uint8_t ble_connected_to_hub;
    uint16_t ble_conn_handle;
    uint16_t ble_val_handle;
    uint8_t ble_own_addr_type;
} App_t;

#endif