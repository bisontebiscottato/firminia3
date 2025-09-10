/*************************************************************
 *                     FIRMINIA 3.5.2                          *
 *  File: test_translations.c                                 *
 *  Author: Andrea Mancini     E-mail: biso@biso.it          *
 ************************************************************/

#include <stdio.h>
#include <string.h>
#include "translations.h"

// Test function to verify translations work correctly
void test_translations(void) {
    printf("=== Translation System Test ===\n\n");
    
    // Test all languages
    language_t languages[] = {LANGUAGE_ENGLISH, LANGUAGE_ITALIAN, LANGUAGE_FRENCH, LANGUAGE_SPANISH};
    const char* lang_names[] = {"English", "Italiano", "Francais", "Espanol"};
    
    for (int lang_idx = 0; lang_idx < LANGUAGE_COUNT; lang_idx++) {
        printf("--- %s ---\n", lang_names[lang_idx]);
        
        // Test all string types
        string_id_t strings[] = {
            STR_WARMING_UP,
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
            STR_RELAX
        };
        
        const char* string_names[] = {
            "WARMING_UP",
            "WAITING_CONFIG", 
            "CONFIG_UPDATED",
            "CONNECTING_WIFI",
            "CHECKING_SIGNATURES",
            "DOSSIER_TO_SIGN",
            "DOSSIERS_TO_SIGN",
            "NO_DOSSIERS",
            "NO_WIFI_SLEEPING",
            "API_ERROR",
            "UNKNOWN_STATE",
            "RELAX"
        };
        
        for (int str_idx = 0; str_idx < STR_COUNT; str_idx++) {
            const char* translated = get_translated_string(strings[str_idx], languages[lang_idx]);
            printf("%s: %s\n", string_names[str_idx], translated);
        }
        printf("\n");
    }
    
    // Test language validation
    printf("=== Language Validation Test ===\n");
    printf("Valid languages: ");
    for (int i = 0; i < LANGUAGE_COUNT; i++) {
        if (is_valid_language(i)) {
            printf("%d ", i);
        }
    }
    printf("\n");
    
    // Test invalid language
    printf("Invalid language test: %s\n", is_valid_language(99) ? "FAIL" : "PASS");
    
    // Test language names
    printf("\n=== Language Names ===\n");
    for (int i = 0; i < LANGUAGE_COUNT; i++) {
        printf("Language %d: %s\n", i, get_language_name(i));
    }
    
    printf("\n=== Test Complete ===\n");
}

// Test function to simulate device configuration
void test_language_configuration(void) {
    printf("=== Language Configuration Test ===\n");
    
    // Simulate different language configurations
    const char* test_languages[] = {"0", "1", "2", "3", "99"};
    const char* expected_results[] = {"English", "Italiano", "Francais", "Espanol", "English"};
    
    for (int i = 0; i < 5; i++) {
        language_t lang = (language_t)atoi(test_languages[i]);
        if (!is_valid_language(lang)) {
            lang = LANGUAGE_ENGLISH; // Fallback
        }
        
        printf("Config '%s' -> Language: %s\n", 
               test_languages[i], get_language_name(lang));
    }
    
    printf("\n=== Configuration Test Complete ===\n");
}

int main(void) {
    test_translations();
    test_language_configuration();
    return 0;
} 