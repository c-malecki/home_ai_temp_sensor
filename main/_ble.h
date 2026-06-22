#ifndef __BLE_H_
#define __BLE_H_

#define BLE_SVC_UUID 0x1100
#define BLE_CHAR_UUID 0x1101

int BLE_Init(void);
void BLE_HostTask(void *param);

#endif