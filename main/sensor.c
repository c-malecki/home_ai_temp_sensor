#include "driver/i2c_master.h"
#include "esp_log.h"
#include <stdint.h>

#include "sensor.h"

static const char *TAG = "TEMP1_TEST";

void Sensor_Init(Sensor_t *sensor, i2c_master_bus_handle_t i2c_bus_handle) {
  i2c_device_config_t sensor_i2c_cfg = {
      .dev_addr_length = I2C_ADDR_BIT_LEN_7,
      .device_address = SENSOR_I2C_ADDR,
      .scl_speed_hz = 100000,
  };
  i2c_master_bus_add_device(i2c_bus_handle, &sensor_i2c_cfg,
                            &sensor->i2c_handle);

  ESP_LOGI(TAG, "Temp Sensor initialized successfully");
}

void Sensor_StartRead(Sensor_t *sensor) {
  uint8_t cmd[2] = {SENSOR_CMD_MEASURE_MSB, SENSOR_CMD_MEASURE_LSB};
  ESP_ERROR_CHECK(
      i2c_master_transmit(sensor->i2c_handle, cmd, sizeof(cmd), 100));
}

float Sensor_GetResult(Sensor_t *sensor) {
  float temp_c = 0.0f;
  uint8_t data[6];

  ESP_ERROR_CHECK(
      i2c_master_receive(sensor->i2c_handle, data, sizeof(data), 100));

  uint16_t raw = ((data[0] << 8) | data[1]);

  temp_c = -45.0f + (175.0f * (float)raw / 65535.0f);

  return temp_c;
}