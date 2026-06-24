#include "main.h"
//
#include "stdint.h"
//
#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"
//
#include "driver/i2c_master.h"
//
#include "freertos/projdefs.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
//
#include "host/ble_gatt.h"
#include "host/ble_hs.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

static const char *TAG = "TEMP_TEST";

static App_t app;

static int ble_init(void);
static void ble_advertise(void);
static int ble_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                         struct ble_gatt_access_ctxt *ctxt, void *arg);
static int ble_gap_event_cb(struct ble_gap_event *event, void *arg);
static void ble_stack_sync_cb(void);
static void ble_stack_reset_cb(int reason);

static const struct ble_gatt_svc_def gatt_svcs[] = {
    {.type = BLE_GATT_SVC_TYPE_PRIMARY,
     .uuid = BLE_UUID16_DECLARE(BLE_SVC_UUID),
     .characteristics =
         (struct ble_gatt_chr_def[]){
             {
                 .uuid = BLE_UUID16_DECLARE(BLE_CHAR_UUID),
                 .access_cb = ble_access_cb,
                 .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE |
                          BLE_GATT_CHR_F_NOTIFY,
                 .val_handle = &app.ble_val_handle,
             },
             {0}}},
    {0}};

void sensor_init(App_t *app);

void sensor_task(void *pvParameters);
void ble_host_task(void *pvParameters);

void app_main(void) {
  app.data_mutex = xSemaphoreCreateMutex();
  assert(app.data_mutex != NULL);

  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
      err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(err);

  i2c_master_bus_config_t i2c_bus_cfg = {
      .clk_source = I2C_CLK_SRC_DEFAULT,
      .i2c_port = I2C_NUM_0,
      .sda_io_num = I2C_SDA_PIN,
      .scl_io_num = I2C_SCL_PIN,
      .glitch_ignore_cnt = 7,
      .flags.enable_internal_pullup = false,
  };
  ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &app.i2c_bus_handle));

  app.temp_poll_interval = 2000;
  sensor_init(&app);
  ble_init();

  nimble_port_freertos_init(ble_host_task);

  xTaskCreate(sensor_task, "sensor_task", 3072, &app, 3, NULL);
}

/* BLE Stuff */

int ble_init(void) {
  ESP_ERROR_CHECK(nimble_port_init());

  ble_svc_gap_device_name_set(DEVICE_NAME);
  ble_svc_gap_init();
  ble_svc_gatt_init();

  int err = ble_gatts_count_cfg(gatt_svcs);
  if (err != 0) {
    return err;
  }

  err = ble_gatts_add_svcs(gatt_svcs);
  if (err != 0) {
    return err;
  }
  if (err != 0) {
    ESP_LOGE(TAG, "Failed to init GATT server: %d", err);
    return err;
  }

  ble_hs_cfg.reset_cb = ble_stack_reset_cb;
  ble_hs_cfg.sync_cb = ble_stack_sync_cb;
  ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
  ble_hs_cfg.sm_bonding = 1;
  ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC;
  ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC;

  return 0;
}

static void ble_advertise(void) {
  struct ble_hs_adv_fields adv_fields;

  memset(&adv_fields, 0, sizeof adv_fields);

  adv_fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
  adv_fields.tx_pwr_lvl_is_present = 1;
  adv_fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;
  adv_fields.name = (uint8_t *)DEVICE_NAME;
  adv_fields.name_len = strlen(DEVICE_NAME);
  adv_fields.name_is_complete = 1;

  adv_fields.uuids16 = (ble_uuid16_t[]){BLE_UUID16_INIT(0x1811)};
  adv_fields.num_uuids16 = 1;
  adv_fields.uuids16_is_complete = 1;

  int err = ble_gap_adv_set_fields(&adv_fields);
  if (err != 0) {
    MODLOG_DFLT(ERROR, "error setting advertisement data; err=%d\n", err);
    return;
  }

  struct ble_gap_adv_params adv_params;

  memset(&adv_params, 0, sizeof adv_params);
  adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
  adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

  err = ble_gap_adv_start(app.ble_own_addr_type, NULL, BLE_HS_FOREVER,
                          &adv_params, ble_gap_event_cb, NULL);
  if (err != 0) {
    MODLOG_DFLT(ERROR, "error enabling advertisement; err=%d\n", err);
    return;
  }
}

static int ble_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                         struct ble_gatt_access_ctxt *ctxt, void *arg) {

  char tx_buffer[32];

  switch (ctxt->op) {
  case BLE_GATT_ACCESS_OP_READ_CHR:

    if (xSemaphoreTake(app.data_mutex, pdMS_TO_TICKS(50)) != pdTRUE) {
      return BLE_ATT_ERR_UNLIKELY;
    }

    snprintf(tx_buffer, sizeof(tx_buffer), "%.2f", app.temp_last_value);
    xSemaphoreGive(app.data_mutex);
    os_mbuf_append(ctxt->om, tx_buffer, strlen(tx_buffer));

    return 0;

  case BLE_GATT_ACCESS_OP_WRITE_CHR:
    uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
    if (len < sizeof(tx_buffer)) {
      ble_hs_mbuf_to_flat(ctxt->om, tx_buffer, len, NULL);
      tx_buffer[len] = '\0';
      return 0;
    }
    return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;

  default:
    return BLE_ERR_UNSUPPORTED;
  }

  return BLE_ATT_ERR_UNLIKELY;
}

static int ble_gap_event_cb(struct ble_gap_event *event, void *arg) {
  switch (event->type) {
  case BLE_GAP_EVENT_CONNECT:
    if (event->connect.status == 0) {
      app.ble_connected_to_hub = 1;
      app.ble_conn_handle = event->connect.conn_handle;
    } else {
      ble_advertise();
    }
    return 0;

  case BLE_GAP_EVENT_DISCONNECT:
    app.ble_connected_to_hub = 0;
    app.ble_conn_handle = 0;
    ble_advertise();
    return 0;

  case BLE_GAP_EVENT_ADV_COMPLETE:
    ble_advertise();
    return 0;
  }
  return 0;
};

static void ble_stack_sync_cb(void) {
  app.ble_own_addr_type = BLE_OWN_ADDR_RANDOM;

  int err = ble_hs_id_infer_auto(0, &app.ble_own_addr_type);
  if (err != 0) {
    MODLOG_DFLT(ERROR, "error determining address type; err=%d\n", err);
    return;
  }

  uint8_t addr_val[6] = {0};
  err = ble_hs_id_copy_addr(app.ble_own_addr_type, addr_val, NULL);
  if (err != 0) {
    ESP_LOGI(TAG, "Failed to copy address; err=%d", err);
    return;
  }

  ble_advertise();
}

static void ble_stack_reset_cb(int reason) {
  MODLOG_DFLT(ERROR, "Resetting state; reason=%d\n", reason);
}

/* Sensor Stuff */

void sensor_init(App_t *app) {
  i2c_device_config_t sensor_i2c_cfg = {
      .dev_addr_length = I2C_ADDR_BIT_LEN_7,
      .device_address = SENSOR_I2C_ADDR,
      .scl_speed_hz = 100000,
  };
  i2c_master_bus_add_device(app->i2c_bus_handle, &sensor_i2c_cfg,
                            &app->i2c_sensor_handle);
}

void read_temp(App_t *app) {
  uint8_t cmd[2] = {SENSOR_CMD_MEASURE_MSB, SENSOR_CMD_MEASURE_LSB};
  ESP_ERROR_CHECK(
      i2c_master_transmit(app->i2c_sensor_handle, cmd, sizeof(cmd), 100));
}

void update_temp(App_t *app) {
  float temp_c = 0.0f;
  uint8_t data[6];

  ESP_ERROR_CHECK(
      i2c_master_receive(app->i2c_sensor_handle, data, sizeof(data), 100));

  uint16_t raw = ((data[0] << 8) | data[1]);

  temp_c = -45.0f + (175.0f * (float)raw / 65535.0f);
  app->temp_last_value = temp_c * 1.8f + 32.0f;
}

/* Tasks */

void ble_host_task(void *pvParameters) {
  ESP_LOGI(TAG, "BLE Host Task Started");
  nimble_port_run();
  nimble_port_freertos_deinit();
}

void sensor_task(void *pvParameters) {
  App_t *app = (App_t *)pvParameters;

  char tx_buffer[32];

  while (1) {
    read_temp(app);

    vTaskDelay(pdMS_TO_TICKS(20));

    update_temp(app);

    // snapshot local values to send while mutex locked
    if (xSemaphoreTake(app->data_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
      snprintf(tx_buffer, sizeof(tx_buffer), "%.2f", app->temp_last_value);
      uint8_t connected = app->ble_connected_to_hub;
      uint16_t conn = app->ble_conn_handle;
      uint16_t handle = app->ble_val_handle;
      xSemaphoreGive(app->data_mutex);

      if (connected) {
        struct os_mbuf *om =
            ble_hs_mbuf_from_flat(tx_buffer, strlen(tx_buffer));

        if (om != NULL) {
          int rc = ble_gatts_notify_custom(conn, handle, om);

          if (rc == 0) {
            ESP_LOGI(TAG, "Notification pushed successfully: %s", tx_buffer);
          } else {
            ESP_LOGE(TAG, "Failed to send notification; rc=%d", rc);
          }
        }
      } else {
        ESP_LOGD(TAG, "Sensor updated, but no BLE client connected to notify.");
      }

    } else {
      ESP_LOGE(TAG, "Failed to acquire mutex lock! Data skipped.");
    }

    vTaskDelay(pdMS_TO_TICKS(app->temp_poll_interval));
  }
}