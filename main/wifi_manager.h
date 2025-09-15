/*************************************************************
 *                     FIRMINIA 3.5.5                          *
 *  File: wifi_manager.h                                     *
 *  Author: Andrea Mancini     E-mail: biso@biso.it          *
 ************************************************************/

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void wifi_manager_init(void);
bool wifi_manager_connect(const char* ssid, const char* password);
bool wifi_manager_is_connected(void);

#ifdef __cplusplus
}
#endif

#endif // WIFI_MANAGER_H
