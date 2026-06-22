#ifndef __APP_H_
#define __APP_H_

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "sensor.h"
#include "oled.h"
#include <stdbool.h>

#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 22

typedef struct
{
    bool temp_new;
    bool ble_connected;
    SemaphoreHandle_t data_mutex;
    Sensor_t *sensor;
    OLED_t *oled;
} App_t;

void App_Init(App_t *app);
void App_Run(App_t *app);

#endif