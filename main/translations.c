/*************************************************************
 *                     FIRMINIA 3.6.1                          *
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
        "Niente\nda firmare.\nRilassati.", // Italian
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
    },
    // STR_OTA_CHECKING
    {
        "Checking...",              // English
        "Controllo...",             // Italian
        "Verification...",          // French
        "Verificando..."            // Spanish
    },
    // STR_OTA_DOWNLOADING
    {
        "Downloading...",           // English
        "Scaricamento...",          // Italian
        "Telechargement...",        // French
        "Descargando..."            // Spanish
    },
    // STR_OTA_VERIFYING
    {
        "Verifying...",             // English
        "Verifica...",              // Italian
        "Verification...",          // French
        "Verificando..."            // Spanish
    },
    // STR_OTA_INSTALLING
    {
        "Installing...",            // English
        "Installazione...",         // Italian
        "Installation...",          // French
        "Instalando..."             // Spanish
    },
    // STR_OTA_COMPLETE
    {
        "Complete!",                // English
        "Completato!",              // Italian
        "Termine!",                 // French
        "Completado!"               // Spanish
    },
    // STR_OTA_UPDATING
    {
        "Updating...",              // English
        "Aggiornamento...",         // Italian
        "Mise a jour...",           // French
        "Actualizando..."           // Spanish
    },
    // STR_OTA_NETWORK_ERROR
    {
        "Network Error",            // English
        "Errore di Rete",           // Italian
        "Erreur Reseau",            // French
        "Error de Red"              // Spanish
    },
    // STR_OTA_DOWNLOAD_FAILED
    {
        "Download Failed",          // English
        "Download Fallito",         // Italian
        "Echec Telechargement",     // French
        "Descarga Fallida"          // Spanish
    },
    // STR_OTA_INVALID_SIGNATURE
    {
        "Invalid Signature",        // English
        "Firma Non Valida",         // Italian
        "Signature Invalide",       // French
        "Firma Invalida"            // Spanish
    },
    // STR_OTA_UPDATE_ERROR
    {
        "Update Error",             // English
        "Errore Aggiornamento",     // Italian
        "Erreur Mise a Jour",       // French
        "Error Actualizacion"       // Spanish
    },
    // STR_OTA_DOWNLOADING_DISPLAY
    {
        "\nDownloading...",           // English
        "\nScaricamento...",          // Italian
        "\nTelechargement...",        // French
        "\nDescargando..."            // Spanish
    },
    // STR_OTA_PLEASE_WAIT
    {
        "Please wait",              // English
        "Attendere prego",          // Italian
        "Veuillez patienter",       // French
        "Por favor espere"          // Spanish
    },
    // STR_OTA_NO_UPDATES_AVAILABLE
    {
        "No firmware\nupdates\navailable",     // English
        "Nessun\naggiornamento\ndisponibile",  // Italian
        "Aucune\nmise a jour\ndisponible",     // French
        "No hay\nactualizaciones\ndisponibles" // Spanish
    },
    // STR_CHECKING_EDITOR_DOCUMENTS
    {
        "Checking\neditor documents\nfor %s...",    // English
        "Controllo\ndocumenti redattore\nper %s...", // Italian
        "Verification\ndocuments redacteur\npour %s...", // French
        "Verificando\ndocumentos editor\npara %s..."     // Spanish
    },
    // STR_CHECKING_SIGNER_PRACTICES
    {
        "Checking\nsignatures for\n%s...",      // English
        "Controllo\nfirme per\n%s...",          // Italian
        "Verification\nsignatures pour\n%s...", // French
        "Verificando\nfirmas para\n%s..."       // Spanish
    },
    // STR_EDITOR_DOCUMENTS_TO_SIGN
    {
        "%d documents\nwaiting for\nsignature",     // English
        "%d documenti\nin attesa di\nfirma",        // Italian
        "%d documents\nen attente de\nsignature",   // French
        "%d documentos\nesperando\nfirma"           // Spanish
    },
    // STR_EDITOR_DOCUMENT_TO_SIGN
    {
        "%d document\nwaiting for\nsignature",      // English
        "%d documento\nin attesa di\nfirma",        // Italian
        "%d document\nen attente de\nsignature",    // French
        "%d documento\nesperando\nfirma"            // Spanish
    },
    // STR_EDITOR_DOCUMENTS_WAITING
    {
        "Documents\nwaiting for\nsignature",     // English
        "Documenti\nin attesa di\nfirma",        // Italian
        "Documents\nen attente de\nsignature",   // French
        "Documentos\nesperando\nfirma"           // Spanish
    },
    // STR_EDITOR_DOCUMENT_WAITING
    {
        "Document\nwaiting for\nsignature",      // English
        "Documento\nin attesa di\nfirma",        // Italian
        "Document\nen attente de\nsignature",    // French
        "Documento\nesperando\nfirma"            // Spanish
    },
    // STR_NO_EDITOR_DOCUMENTS
    {
        "No documents\nwaiting.\nRelax.",  // English
        "Nessun\ndocumento\nin attesa.\nRilassati.", // Italian
        "Aucun\ndocument\nen attente.\nRelax.", // French
        "Ningun\ndocumento\nen espera.\nRelájate."    // Spanish
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