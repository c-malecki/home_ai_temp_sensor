#include "app.h"
#include "_ble.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "sensor.h"

static const char *TAG = "TEMP1_TEST";
static Sensor_t sensor;

void sensor_task(void *pvParameters);
void ble_task(void *pvParameters);

void App_Init(App_t *app) {
  app->read_interval = 2000;
  app->data_mutex = xSemaphoreCreateMutex();
  assert(app->data_mutex != NULL);

  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
      err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(err);

  // I2C
  i2c_master_bus_config_t i2c_bus_cfg = {
      .clk_source = I2C_CLK_SRC_DEFAULT,
      .i2c_port = I2C_NUM_0,
      .sda_io_num = I2C_SDA_PIN,
      .scl_io_num = I2C_SCL_PIN,
      .glitch_ignore_cnt = 7,
      .flags.enable_internal_pullup = false,
  };
  i2c_master_bus_handle_t i2c_bus_handle;
  ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_handle));

  Sensor_Init(&sensor, i2c_bus_handle);
  app->sensor = &sensor;

  // BLE_Init();
}

void App_Run(App_t *app) {

  // while (1) {
  //   vTaskDelay(pdMS_TO_TICKS(1000));
  // }

  xTaskCreate(sensor_task, "sensor_task", 3072, app, 3, NULL);
  // xTaskCreate(ble_task, "ble_task", 4096, app, 5, NULL);
}

void sensor_task(void *pvParameters) {
  App_t *app = (App_t *)pvParameters;

  while (1) {
    Sensor_StartRead(app->sensor);

    vTaskDelay(pdMS_TO_TICKS(20));

    float last_temp = Sensor_GetResult(app->sensor);

    if (xSemaphoreTake(app->data_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
      app->sensor->last_temp = last_temp;
      app->temp_new = true;
      ESP_LOGI("Temperature:", "%.2f F", last_temp);
      xSemaphoreGive(app->data_mutex);
    } else {
      ESP_LOGE("Temperature:", "Failed to acquire mutex lock! Data skipped.");
    }

    vTaskDelay(pdMS_TO_TICKS(app->read_interval));
  }
}

void ble_task(void *pvParameters) {
  App_t *app = (App_t *)pvParameters;
  char tx_buffer[32];
  float temp_to_send = 0.0f;

  while (1) {
    if (app->temp_new) {
      // 1. Safely copy the data out of the shared struct
      if (xSemaphoreTake(app->data_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        temp_to_send = app->sensor->last_temp;
        xSemaphoreGive(app->data_mutex);
      }

      // 2. Format and send data if a client is present
      if (app->ble_connected) {
        snprintf(tx_buffer, sizeof(tx_buffer), "Temp: %.2f C", temp_to_send);
        // Execute your nimble notification routines here...
      }
    }

    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}