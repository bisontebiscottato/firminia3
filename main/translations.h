/*************************************************************
 *                     FIRMINIA 3.4.1                          *
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
    STR_COUNT
} string_id_t;

// Function declarations
const char* get_translated_string(string_id_t string_id, language_t language);
language_t get_current_language(void);
void set_current_language(language_t language);
const char* get_language_name(language_t language);
int is_valid_language(language_t language);

#endif // TRANSLATIONS_H 