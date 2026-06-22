#ifndef __SENSOR_H_
#define __SENSOR_H_

#include "driver/i2c_master.h"
#include <stdint.h>
#include <stdbool.h>

#define SENSOR_I2C_ADDR (0x44)

#define SENSOR_CMD_MEASURE_MSB (0x24)
#define SENSOR_CMD_MEASURE_LSB (0x00)
#define SENSOR_CMD_SOFT_RESET_MSB (0x30)
#define SENSOR_CMD_SOFT_RESET_LSB (0xA2)

typedef enum {
    SENSOR_STATE_INIT = 0,
    SENSOR_STATE_READY,
    SENSOR_STATE_START_READ,
    SENSOR_STATE_WAIT_RESULT,
    SENSOR_STATE_GET_RESULT,
    SENSOR_STATE_WAIT_READ,
} Sensor_State;

typedef struct
{
    int16_t last_temp;
    uint32_t last_read;
    uint32_t last_result;
    i2c_master_dev_handle_t i2c_handle;
} Sensor_t;

void Sensor_Init(Sensor_t *sensor, i2c_master_bus_handle_t i2c_bus_handle);
void Sensor_StartRead(Sensor_t *sensor);
float Sensor_GetResult(Sensor_t *sensor);

#endif