#ifndef __OLED_H_
#define __OLED_H_

#include "driver/i2c_master.h"

#define OLED_I2C_ADDR (0x3C)

typedef struct
{
    i2c_master_dev_handle_t i2c_handle;
} OLED_t;

void OLED_Init(OLED_t *oled, i2c_master_bus_handle_t i2c_bus_handle);

#endif