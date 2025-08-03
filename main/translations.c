/*************************************************************
 *                     FIRMINIA 3.4.0                          *
 *  File: translations.c                                      *
 *  Author: Andrea Mancini     E-mail: biso@biso.it          *
 ************************************************************/

#include "translations.h"
#include "esp_log.h"

static const char *TAG = "Translations";

// Translation table: [string_id][language]
static const char* translations[STR_COUNT][LANGUAGE_COUNT] = {
    // STR_WARMING_UP
    {
        "Warming\nup...",           // English
        "Avvio\nin corso...", // Italian
        "Mise en\nmarche...",       // French
        "Calentando..."             // Spanish
    },
    // STR_WAITING_CONFIG
    {
        "Waiting for\nconfig...",   // English
        "In attesa di\nconfigurazione...", // Italian
        "En attente de\nconfiguration...", // French
        "Esperando\nconfiguración..." // Spanish
    },
    // STR_CONFIG_UPDATED
    {
        "Configuration\nupdated!",   // English
        "Configurazione\naggiornata!", // Italian
        "Configuration\nmise a jour!", // French
        "Configuracion\nactualizada!" // Spanish
    },
    // STR_CONNECTING_WIFI
    {
        "Connecting\nto Wi-Fi...",  // English
        "Connessione\na Wi-Fi...",  // Italian
        "Connexion\nau Wi-Fi...",   // French
        "Conectando\na Wi-Fi..."    // Spanish
    },
    // STR_CHECKING_SIGNATURES
    {
        "Checking\nsignatures for\n%s...", // English
        "Controllo\nfirme per\n%s...",     // Italian
        "Verification\ndes signatures pour\n%s...", // French
        "Verificando\nfirmas para\n%s..."   // Spanish
    },
    // STR_DOSSIER_TO_SIGN
    {
        "\n\ndossier\nto sign!",        // English
        "\n\npratica\nda firmare!",     // Italian
        "\n\nenveloppe\na signer!",       // French
        "\n\npractica\npara firmar!"     // Spanish
    },
    // STR_DOSSIERS_TO_SIGN
    {
        "\n\ndossiers\nto sign!",       // English
        "\n\npratiche\nda firmare!",     // Italian
        "\n\nenveloppes\na signer!",      // French
        "\n\npracticas\npara firmar!"    // Spanish
    },
    // STR_NO_DOSSIERS
    {
        "No dossiers\nto sign.\nRelax.", // English
        "Nessuna pratica\nda firmare.\nRilassati.", // Italian
        "Aucune enveloppe\na signer.\nDetendez-vous.", // French
        "No hay practicas\npara firmar.\nRelajate." // Spanish
    },
    // STR_NO_WIFI_SLEEPING
    {
        "No Wi-Fi.\nsleeping...",   // English
        "Nessun Wi-Fi,\nin attesa...", // Italian
        "Pas de Wi-Fi.\nen veille...", // French
        "Sin Wi-Fi.\ndurmiendo..."  // Spanish
    },
    // STR_API_ERROR
    {
        "API error!\nE-002",        // English
        "Errore API!\nE-002",       // Italian
        "Erreur API!\nE-002",       // French
        "Error API!\nE-002"        // Spanish
    },
    // STR_UNKNOWN_STATE
    {
        "Unknown state.\nE-003",    // English
        "Stato sconosciuto.\nE-003", // Italian
        "Etat inconnu.\nE-003",     // French
        "Estado desconocido.\nE-003" // Spanish
    },
    // STR_RELAX
    {
        "Relax.",                   // English
        "Rilassati.",               // Italian
        "Detendez-vous.",           // French
        "Relajate."                 // Spanish
    }
};

// Current language (default: English)
static language_t current_language = LANGUAGE_ENGLISH;

const char* get_translated_string(string_id_t string_id, language_t language) {
    if (string_id >= STR_COUNT || language >= LANGUAGE_COUNT) {
        ESP_LOGE(TAG, "Invalid string_id (%d) or language (%d)", string_id, language);
        return translations[STR_UNKNOWN_STATE][LANGUAGE_ENGLISH];
    }
    
    return translations[string_id][language];
}

language_t get_current_language(void) {
    return current_language;
}

void set_current_language(language_t language) {
    if (language < LANGUAGE_COUNT) {
        current_language = language;
        ESP_LOGI(TAG, "Language set to: %s", get_language_name(language));
    } else {
        ESP_LOGE(TAG, "Invalid language: %d", language);
    }
}

const char* get_language_name(language_t language) {
    switch (language) {
        case LANGUAGE_ENGLISH:
            return "English";
        case LANGUAGE_ITALIAN:
            return "Italiano";
        case LANGUAGE_FRENCH:
            return "Francais";
        case LANGUAGE_SPANISH:
            return "Espanol";
        default:
            return "Unknown";
    }
}

int is_valid_language(language_t language) {
    return language < LANGUAGE_COUNT;
} 