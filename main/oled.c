#include "driver/i2c_master.h"
#include "esp_log.h"

#include "oled.h"
#include "ssd1306.h"

static const char *TAG = "TEMP1_TEST";

void OLED_Init(OLED_t *oled, i2c_master_bus_handle_t i2c_bus_handle) {
  ssd1306_config_t cfg = {
      .bus = SSD1306_I2C,
      .width = 128,
      .height = 64,
      .iface.i2c =
          {
              .port = I2C_NUM_0,
              .addr = 0x3C,
              .rst_gpio = 17,
          },
  };

  ssd1306_handle_t disp;
  ssd1306_new_i2c(&cfg, &disp);
  ssd1306_clear(disp);
  ssd1306_draw_text(disp, 0, 0, "SSD1306 I2C", true);
  ssd1306_display(disp);

  ESP_LOGI(TAG, "OLED initialized successfully");
}