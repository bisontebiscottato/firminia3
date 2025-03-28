#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DISPLAY_STATE_WARMING_UP,
    DISPLAY_STATE_BLE_ADVERTISING,
    DISPLAY_STATE_CONFIG_UPDATED,
    DISPLAY_STATE_WIFI_CONNECTING,
    DISPLAY_STATE_CHECKING_API,
    DISPLAY_STATE_SHOW_PRACTICES,
    DISPLAY_STATE_NO_PRACTICES,
    DISPLAY_STATE_NO_WIFI_SLEEPING,
    DISPLAY_STATE_API_ERROR    
} display_state_t;

void display_manager_init(void);
void display_manager_update(display_state_t state, int practices_count);

#ifdef __cplusplus
}
#endif

#endif // DISPLAY_MANAGER_H