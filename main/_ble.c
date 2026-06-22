#include "_ble.h"
#include "esp_err.h"
#include "host/ble_gatt.h"
#include "host/ble_hs.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#define DEVICE_NAME "TEMP1_TEST"
static const char *TAG = "TEMP1_TEST";

static void ble_advertise(void);
static int ble_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                         struct ble_gatt_access_ctxt *ctxt, void *arg);
static int ble_gap_event_cb(struct ble_gap_event *event, void *arg);
static void ble_stack_sync_cb(void);
static void ble_stack_reset_cb(int reason);

static uint8_t own_addr_type;
static uint8_t ble_device_connected = 0;
static uint16_t temp_chr_val_handle;
static uint16_t conn_handle;

static const struct ble_gatt_svc_def gatt_svcs[] = {
    {.type = BLE_GATT_SVC_TYPE_PRIMARY,
     .uuid = BLE_UUID16_DECLARE(BLE_SVC_UUID),
     .characteristics = (struct ble_gatt_chr_def[]){{
         .uuid = BLE_UUID16_DECLARE(BLE_CHAR_UUID),
         .access_cb = ble_access_cb,
         .flags =
             BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_NOTIFY,
         .val_handle = &temp_chr_val_handle,
     }}}};

void BLE_HostTask(void *param) {
  ESP_LOGI(TAG, "BLE Host Task Started");
  nimble_port_run();
  nimble_port_freertos_deinit();
}

int BLE_Init(void) {
  ESP_ERROR_CHECK(nimble_port_init());
  ble_svc_gap_device_name_set("Temp1");

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

  err = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER, &adv_params,
                          ble_gap_event_cb, NULL);
  if (err != 0) {
    MODLOG_DFLT(ERROR, "error enabling advertisement; err=%d\n", err);
    return;
  }
}

static int ble_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                         struct ble_gatt_access_ctxt *ctxt, void *arg) {
  // dummy response data
  static char characteristic_value[32] = "Hello From ESP32-v6";

  switch (ctxt->op) {
  case BLE_GATT_ACCESS_OP_READ_CHR:
    ESP_LOGI(TAG, "GATT Read Request received.");
    os_mbuf_append(ctxt->om, characteristic_value,
                   strlen(characteristic_value));
    return 0;

  case BLE_GATT_ACCESS_OP_WRITE_CHR:
    ESP_LOGI(TAG, "GATT Write Request received.");
    uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
    if (len < sizeof(characteristic_value)) {
      ble_hs_mbuf_to_flat(ctxt->om, characteristic_value, len, NULL);
      characteristic_value[len] = '\0';
      ESP_LOGI(TAG, "Updated Characteristic Value: %s", characteristic_value);
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
    ESP_LOGI(TAG, "GAP Event: Connected status=%d", event->connect.status);
    if (event->connect.status == 0) {
      ble_device_connected = 1;
      conn_handle = event->connect.conn_handle;
    } else {
      ble_advertise(); // Restart advertising if pairing failed
    }
    return 0;

  case BLE_GAP_EVENT_DISCONNECT:
    ESP_LOGI(TAG, "GAP Event: Disconnected; reason=%d",
             event->disconnect.reason);
    ble_device_connected = 0;
    ble_advertise();
    return 0;

  case BLE_GAP_EVENT_ADV_COMPLETE:
    ESP_LOGI(TAG, "GAP Event: Complete; reason=%d", event->adv_complete.reason);
    ble_advertise();
    return 0;
  }
  return 0;
};

static void ble_stack_sync_cb(void) {
  own_addr_type = BLE_OWN_ADDR_RANDOM;

  int err = ble_hs_id_infer_auto(0, &own_addr_type);
  if (err != 0) {
    MODLOG_DFLT(ERROR, "error determining address type; err=%d\n", err);
    return;
  }

  uint8_t addr_val[6] = {0};
  err = ble_hs_id_copy_addr(own_addr_type, addr_val, NULL);
  if (err != 0) {
    ESP_LOGI(TAG, "Failed to copy address; err=%d", err);
    return;
  }

  ESP_LOGI(TAG, "Device Address: %02x:%02x:%02x:%02x:%02x:%02x", addr_val[5],
           addr_val[4], addr_val[3], addr_val[2], addr_val[1], addr_val[0]);

  ble_advertise();
}

static void ble_stack_reset_cb(int reason) {
  MODLOG_DFLT(ERROR, "Resetting state; reason=%d\n", reason);
}

// FreeRTOS Task to handle periodic sensor polling and BLE notifying
void sensor_poll_and_notify_task(void *pvParameters) {
  char tx_buffer[32];
  float mock_temperature = 22.5f;

  while (1) {
    // 1. (Placeholder) Read your physical sensor via I2C here
    // example: read_i2c_sensor(&mock_temperature);
    mock_temperature += 0.1f; // Simulate data fluctuation

    // Format the payload string
    snprintf(tx_buffer, sizeof(tx_buffer), "Temp: %.2f C", mock_temperature);

    // 2. Check if a BLE central device is actively connected
    if (ble_device_connected) {

      // Create a NimBLE memory buffer allocation
      struct os_mbuf *om = ble_hs_mbuf_from_flat(tx_buffer, strlen(tx_buffer));

      if (om != NULL) {
        // Push the notification over the radio
        int rc = ble_gatts_notify_custom(conn_handle, temp_chr_val_handle, om);

        if (rc == 0) {
          ESP_LOGI(TAG, "Notification pushed successfully: %s", tx_buffer);
        } else {
          ESP_LOGE(TAG, "Failed to send notification; rc=%d", rc);
        }
      }
    } else {
      ESP_LOGD(TAG, "Sensor updated, but no BLE client connected to notify.");
    }

    // Poll every 2 seconds
    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}