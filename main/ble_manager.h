/*************************************************************
 *                     FIRMINIA 3.4.1                          *
 *  File: ble_manager.h                                      *
 *  Author: Andrea Mancini     E-mail: biso@biso.it          *
 ************************************************************/

#ifndef BLE_MANAGER_H
#define BLE_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * @brief Callback per la ricezione di una configurazione via BLE.
 * 
 * Questa callback viene chiamata ogni volta che viene ricevuto un JSON completo
 * tramite BLE, passando il JSON ricevuto come stringa.
 *
 * @param json_str La stringa JSON contenente la configurazione.
 */
typedef void (*ble_config_callback_t)(const char *json_str);

/**
 * @brief Inizializza lo stack BLE e registra le callback necessarie.
 *
 * Imposta il nome del dispositivo a "ESP32-FIRMINIA" e inizializza il
 * Bluedroid stack. Se la configurazione BLE è integrata con il modulo di UI,
 * aggiorna anche lo stato della UI.
 */
void ble_manager_init(void);

/**
 * @brief Avvia l'advertising BLE.
 *
 * Imposta i parametri di advertising e avvia l'advertising.
 */
void ble_manager_start_advertising(void);

/**
 * @brief Ferma l'advertising BLE.
 *
 * Ferma l'advertising attivo.
 */
void ble_manager_stop_advertising(void);

/**
 * @brief Imposta una callback per notificare la ricezione di una nuova configurazione via BLE.
 *
 * La callback verrà eseguita ogni volta che viene ricevuto un JSON completo
 * tramite un'operazione di scrittura GATT.
 *
 * @param callback Puntatore alla callback da registrare.
 */
void ble_manager_set_config_callback(ble_config_callback_t callback);

void ble_manager_disconnect(void);

/**
 * @brief Get the current BLE device name.
 *
 * @return Pointer to the device name string.
 */
const char* ble_manager_get_device_name(void);

/**
 * @brief Set the BLE device name.
 *
 * @param name The new device name.
 * @return true if successful, false otherwise.
 */
bool ble_manager_set_device_name(const char* name);

#ifdef __cplusplus
}
#endif

#endif /* BLE_MANAGER_H */
