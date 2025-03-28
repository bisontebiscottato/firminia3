#ifndef BLE_MANAGER_H
#define BLE_MANAGER_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Callback type invoked when a JSON config is received over BLE
typedef void (*ble_config_callback_t)(const char* json_str);

void ble_manager_init(void);
void ble_manager_start_advertising(void);
void ble_manager_stop_advertising(void);
void ble_manager_set_config_callback(ble_config_callback_t callback);

#ifdef __cplusplus
}
#endif

#endif // BLE_MANAGER_H
