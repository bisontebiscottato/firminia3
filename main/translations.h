/*************************************************************
 *                     FIRMINIA 3.6.1                          *
 *  File: translations.h                                      *
 *  Author: Andrea Mancini     E-mail: biso@biso.it          *
 ************************************************************/

#ifndef TRANSLATIONS_H
#define TRANSLATIONS_H

#include <stdint.h>

// Supported languages
typedef enum {
    LANGUAGE_ENGLISH = 0,
    LANGUAGE_ITALIAN = 1,
    LANGUAGE_FRENCH = 2,
    LANGUAGE_SPANISH = 3,
    LANGUAGE_COUNT
} language_t;

// String identifiers
typedef enum {
    STR_WARMING_UP = 0,
    STR_WAITING_CONFIG,
    STR_CONFIG_UPDATED,
    STR_CONNECTING_WIFI,
    STR_CHECKING_SIGNATURES,
    STR_DOSSIER_TO_SIGN,
    STR_DOSSIERS_TO_SIGN,
    STR_NO_DOSSIERS,
    STR_NO_WIFI_SLEEPING,
    STR_API_ERROR,
    STR_UNKNOWN_STATE,
    STR_RELAX,
    // OTA Status Messages
    STR_OTA_CHECKING,
    STR_OTA_DOWNLOADING,
    STR_OTA_VERIFYING,
    STR_OTA_INSTALLING,
    STR_OTA_COMPLETE,
    STR_OTA_UPDATING,
    // OTA Error Messages
    STR_OTA_NETWORK_ERROR,
    STR_OTA_DOWNLOAD_FAILED,
    STR_OTA_INVALID_SIGNATURE,
    STR_OTA_UPDATE_ERROR,
    // OTA Display Messages
    STR_OTA_DOWNLOADING_DISPLAY,
    STR_OTA_PLEASE_WAIT,
    STR_OTA_NO_UPDATES_AVAILABLE,
    // Working mode messages
    STR_CHECKING_EDITOR_DOCUMENTS,
    STR_CHECKING_SIGNER_PRACTICES,
    STR_EDITOR_DOCUMENTS_TO_SIGN,
    STR_EDITOR_DOCUMENT_TO_SIGN,
    STR_EDITOR_DOCUMENTS_WAITING,      // Text without number for editor mode (plural)
    STR_EDITOR_DOCUMENT_WAITING,       // Text without number for editor mode (singular)
    STR_NO_EDITOR_DOCUMENTS,
    STR_COUNT
} string_id_t;

// Function declarations
const char* get_translated_string(string_id_t string_id, language_t language);
language_t get_current_language(void);
void set_current_language(language_t language);
const char* get_language_name(language_t language);
int is_valid_language(language_t language);

#endif // TRANSLATIONS_H 