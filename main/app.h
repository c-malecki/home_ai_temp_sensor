#ifndef __APP_H_
#define __APP_H_

#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/semphr.h"
#include "sensor.h"
#include <stdbool.h>

#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 22

typedef struct
{
    bool temp_new;
    bool ble_connected;
    TickType_t read_interval;
    SemaphoreHandle_t data_mutex;
    Sensor_t *sensor;
} App_t;

void App_Init(App_t *app);
void App_Run(App_t *app);

#endif