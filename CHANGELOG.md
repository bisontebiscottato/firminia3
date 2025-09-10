# Changelog - Firminia V3

## Versione 3.5.3 - Sistema OTA Sicuro

### 🚀 Nuove Funzionalità

#### Sistema OTA (Over-The-Air Updates)
- **Aggiornamenti automatici** tramite GitHub Releases API
- **Download sicuro HTTPS** con verifica certificati SSL/TLS
- **Dual-boot partitions** per rollback automatico in caso di errori
- **Controllo manuale** tramite pressione prolungata del tasto (5 secondi)
- **Controllo periodico** automatico ogni ora quando connesso al WiFi
- **Display dedicato** con sospensione LVGL per evitare conflitti SPI
- **Progress tracking** dettagliato nei log ESP per monitoraggio

#### Controlli Bottone Migliorati
- **5 secondi**: Avvia aggiornamento OTA (se WiFi connesso)
- **10 secondi**: Reset configurazione a default (come prima)
- **Pressione breve**: Controllo pratiche immediato (come prima)
- **Eliminato doppio click** per evitare conflitti con controllo pratiche

#### Gestione Display OTA
- **Sospensione LVGL** durante download per eliminare conflitti SPI
- **Display statico** con messaggio "Downloading... Please wait"
- **Simbolo LVGL** standard (`LV_SYMBOL_DOWNLOAD`) per coerenza UI
- **Riattivazione automatica** al completamento dell'aggiornamento

### 🔧 Modifiche Tecniche

#### Partizioni Flash
- **Partizione factory rimossa** per ottimizzare spazio
- **ota_0 e ota_1** da 1920KB ciascuna per firmware da 1.8MB
- **ota_data** per gestione dual-boot

#### Configurazione OTA
- **Timeout HTTP aumentato** a 120 secondi per download grandi
- **Buffer ottimizzati** (8KB RX, 2KB TX) per performance migliori
- **Retry logic** e rilevamento stalli per affidabilità
- **Verifica firma** preparata (attualmente disabilitata per testing)

#### API GitHub Integration
- **GitHub Releases API** per controllo automatico versioni
- **JSON parsing** robusto con gestione errori
- **URL validation** e costruzione automatica URL firma
- **User-Agent** personalizzato per identificazione

### 🛡️ Sicurezza

#### SSL/TLS
- **esp_crt_bundle_attach** per verifica certificati
- **Validazione hostname** abilitata
- **Connessioni sicure** per API GitHub e download firmware

#### Gestione Errori
- **Rollback automatico** in caso di firmware corrotto
- **Validazione firmware** prima dell'installazione
- **Cleanup automatico** risorse in caso di errori
- **Logging dettagliato** per troubleshooting

### 📊 Performance

#### Ottimizzazioni Display
- **Eliminazione conflitti SPI** tramite sospensione LVGL
- **Riduzione aggiornamenti** durante OTA (da 100ms a 50ms)
- **Throttling intelligente** per progress updates
- **Memory management** migliorato

#### Network
- **Keep-alive HTTP** per connessioni efficienti
- **Buffer sizing** ottimizzato per firmware grandi
- **Timeout management** adattivo
- **Retry strategy** per connessioni instabili

### 🐛 Correzioni

#### Display
- **Risolto "Unknown state E-003"** aggiungendo `DISPLAY_STATE_OTA_UPDATE`
- **Corretto testo sbiadito** durante inizializzazione OTA
- **Eliminati conflitti SPI** tra display e download HTTP

#### Memory Management
- **Risolto double-free** nella pulizia JSON
- **Corretta gestione handle** OTA (`esp_https_ota_handle_t`)
- **String null-termination** forzata per URL

#### API
- **Corretti tipi handle** per compatibilità ESP-IDF v5.5
- **Aggiornate API calls** per HTTPS OTA
- **Risolti warning compilazione** per funzioni deprecated

---

## Versione 3.5.1 - Miglioramenti QR Code (Precedente)

### 📱 Interfaccia Utente

#### QR Code Ottimizzato
- **QR Code reale implementato** con dati RGB565 da LVGL Image Converter
- **Dimensioni aumentate del 25%** per migliore leggibilità (da 100x100 a 125x125 pixel)
- **Centratura perfetta** tramite transform pivot point
- **Qualità migliorata** mantenendo la risoluzione originale
- **Alternanza fluida** tra testo e QR code ogni 3 secondi durante BLE advertising

#### Modifiche Tecniche
- **`main/qr_image.c`**: Aggiornato da LV_COLOR_FORMAT_I1 a LV_COLOR_FORMAT_RGB565
- **`main/display_manager.c`**: Implementata scalatura con `lv_obj_set_style_transform_scale()`
- **Transform pivot**: Impostato al centro (50%, 50%) per scalatura bilanciata
- **Logging migliorato**: Aggiunta informazione sulle dimensioni QR code

### 🔧 Correzioni
- **Risolto problema centratura**: QR code ora perfettamente centrato dopo scaling
- **Compatibilità LVGL 9.x**: Struttura dati immagine corretta
- **Performance ottimizzate**: Scaling hardware-accelerated

---

## Versione 3.5.0 - Sicurezza e Gestione Configurazione

### 🔒 Sicurezza Migliorata

#### Rimozione Dati Sensibili
- **Rimossi valori di default sensibili** dal firmware
  - `DEFAULT_WIFI_SSID`: da "AndroidAPE857" → `""` (vuoto)
  - `DEFAULT_WIFI_PASSWORD`: da "thisisatest" → `""` (vuoto)
  - `DEFAULT_API_TOKEN`: da token reale → `""` (vuoto)
  - `DEFAULT_ASKMESIGN_USER`: da email reale → `""` (vuoto)
- **Nessun dato sensibile** più presente nel codice sorgente
- **Configurazione sicura** richiesta per il funzionamento

#### Rilevamento Configurazione di Default
- **Nuova funzione `is_config_default()`** per rilevamento robusto
- **Controlli multipli** su parametri critici (SSID, password, token, utente)
- **Validazione stringhe vuote** e confronto con valori di default
- **Gestione intelligente** delle configurazioni non valide

### 🔵 Modalità BLE Automatica

#### Comportamento Automatico
- **Attivazione automatica BLE** quando rilevata configurazione di default
- **Nessun intervento manuale richiesto** per nuovi dispositivi
- **Attesa indefinita** per configurazione valida
- **Log di stato** ogni 30 secondi durante l'attesa
- **Riavvio automatico** dopo ricezione configurazione

#### Flusso di Avvio Migliorato
```
Nuovo Dispositivo:
Power On → Warm-up → Config Default Rilevata → BLE Auto → Configurazione → Riavvio

Dispositivo Configurato:
Power On → Warm-up → Config Valida → Wi-Fi → Funzionamento Normale

Modalità Manuale:
Power On → Warm-up + Pulsante → BLE Manuale → Configurazione → Riavvio
```

### 🔄 Funzionalità di Reset Configurazione

#### Meccanismo di Reset
- **Pressione prolungata** del pulsante per 5 secondi
- **Disponibile in tutti gli stati** tranne `WARMING_UP`
- **Feedback progressivo** con log ogni secondo
- **Reset automatico** a valori di default
- **Riavvio e BLE automatico** dopo reset

#### Stati Supportati per Reset
- ✅ `STATE_BLE_ADVERTISING`
- ✅ `STATE_WIFI_CONNECTING`
- ✅ `STATE_CHECKING_API`
- ✅ `STATE_SHOW_PRACTICES`
- ✅ `STATE_NO_PRACTICES`
- ✅ `STATE_NO_WIFI`
- ✅ `STATE_API_ERROR`
- ❌ `STATE_WARMING_UP` (escluso per sicurezza)

#### Processo di Reset
```
🔘 Button press detected, checking for reset hold...
🔘 Button held for 1000/5000 ms...
🔘 Button held for 2000/5000 ms...
🔘 Button held for 3000/5000 ms...
🔘 Button held for 4000/5000 ms...
🔘 Button held for 5000/5000 ms...
🔄 Configuration reset triggered!
🔄 Resetting configuration to default values...
✅ Configuration reset to default and saved to NVS!
```

### 📝 File Modificati

#### `main/device_config.h`
- **Aggiornati valori di default** per rimuovere dati sensibili
- **Aggiunta dichiarazione** `reset_config_to_default()`
- **Aggiunta dichiarazione** `is_config_default()`
- **Documentazione migliorata** per sicurezza

#### `main/device_config.c`
- **Implementata `is_config_default()`** con controlli multipli
- **Implementata `reset_config_to_default()`** per reset completo
- **Aggiornata `is_config_valid()`** per usare nuova logica
- **Log migliorati** con emoji Unicode

#### `main/main_flow.c`
- **Aggiunta costante** `RESET_BUTTON_HOLD_TIME_MS` (5000ms)
- **Implementata `check_button_held_for_reset()`** per rilevamento pressione
- **Integrata logica BLE automatica** dopo warm-up
- **Integrata logica reset** in tutti i loop operativi
- **Aggiornata documentazione** nel header del file
- **Gestione stati migliorata** per nuovi comportamenti

### 🎯 Benefici per l'Utente

#### Sicurezza
- **Nessun dato sensibile** esposto nel firmware
- **Configurazione obbligatoria** per il funzionamento
- **Reset sicuro** in caso di problemi

#### Usabilità
- **Setup automatico** per nuovi dispositivi
- **Nessuna procedura complessa** richiesta
- **Recovery facile** tramite reset con pulsante
- **Feedback chiaro** durante tutte le operazioni

#### Manutenibilità
- **Troubleshooting semplificato** con reset rapido
- **Configurazione pulita** sempre disponibile
- **Log dettagliati** per debugging

### 🔧 Compatibilità

#### Dispositivi Esistenti
- **Compatibilità completa** con configurazioni esistenti
- **Nessuna migrazione richiesta** per dispositivi già configurati
- **Comportamento invariato** per configurazioni valide

#### Nuovi Dispositivi
- **BLE automatico** al primo avvio
- **Configurazione obbligatoria** prima dell'uso
- **Esperienza utente ottimizzata**

---

## Versione 3.4.1 - Aggiunta Supporto Multi-lingua

### Nuovi File Creati

#### `main/translations.h`
- Header file per il sistema di traduzioni
- Definizione degli enum per lingue e stringhe
- Dichiarazioni delle funzioni di gestione traduzioni

#### `main/translations.c`
- Implementazione del sistema di traduzioni
- Tabella delle traduzioni per 4 lingue (EN, IT, FR, ES)
- Funzioni per gestione lingua corrente
- Validazione delle lingue supportate

#### `LANGUAGE_SUPPORT.md`
- Documentazione completa del sistema di localizzazione
- Esempi di configurazione JSON
- Tabella delle traduzioni per tutti gli stati
- Note tecniche per sviluppatori

#### `example_config.json`
- Esempio di configurazione JSON con parametro language
- Configurazione completa per test

#### `test_translations.c`
- File di test per verificare il sistema di traduzioni
- Test di tutte le lingue e stringhe
- Validazione del sistema

### File Modificati

#### `main/device_config.h`
- Aggiunta definizione `NVS_LANGUAGE`
- Aggiunta costante `LANGUAGE_SIZE`
- Aggiunta variabile globale `language`
- Aggiunta costante `DEFAULT_LANGUAGE`

#### `main/device_config.c`
- Aggiunta gestione caricamento lingua da NVS
- Aggiunta gestione salvataggio lingua in NVS
- Aggiunta log per lingua corrente
- Gestione fallback per lingua non valida

#### `main/ble_manager.c`
- Aggiunta inclusione `translations.h`
- Aggiunta funzione `validate_language()`
- Aggiunta validazione parametro `language` nel JSON
- Aggiunta aggiornamento lingua quando ricevuta nuova configurazione
- Aggiunta log per cambio lingua

#### `main/display_manager.c`
- Aggiunta inclusione `translations.h`
- Modifica `display_manager_init()` per inizializzare lingua
- Modifica `display_manager_update()` per usare traduzioni
- Sostituzione di tutte le stringhe hardcoded con chiamate a `get_translated_string()`
- Gestione fallback per lingua non valida

#### `main/CMakeLists.txt`
- Aggiunta `translations.c` alla lista dei file sorgente

#### `README.md`
- Aggiunta feature "Multi-language Support"
- Aggiunta parametro `language` nella tabella configurazione
- Aggiunta sezione dedicata al supporto multi-lingua
- Aggiornamento esempio JSON con parametro language

### Funzionalità Implementate

#### Sistema di Traduzioni
- Supporto per 4 lingue: Inglese, Italiano, Francese, Spagnolo
- 12 stringhe tradotte per tutti gli stati del display
- Gestione automatica del fallback all'inglese
- Validazione delle lingue supportate

#### Configurazione via BLE
- Nuovo parametro `language` nel JSON di configurazione
- Validazione del parametro language
- Aggiornamento dinamico della lingua senza riavvio
- Persistenza della lingua nel NVS

#### Gestione Lingua
- Inizializzazione automatica della lingua all'avvio
- Aggiornamento della lingua quando ricevuta nuova configurazione
- Log dettagliati per debugging
- Gestione robusta degli errori

### Compatibilità

#### Retrocompatibilità
- Dispositivi esistenti continuano a funzionare normalmente
- Se il parametro `language` non è presente, viene usato l'inglese
- Nessuna modifica alle API esistenti

#### Configurazione
- Il parametro `language` è opzionale nel JSON
- Valori validi: "0" (EN), "1" (IT), "2" (FR), "3" (ES)
- Valori non validi causano fallback all'inglese

### Test e Validazione

#### Test Implementati
- Test di tutte le traduzioni per tutte le lingue
- Test di validazione delle lingue
- Test di configurazione via JSON
- Test di fallback per lingue non valide

#### Log di Debug
- Log per inizializzazione lingua
- Log per cambio lingua
- Log per errori di validazione
- Log per fallback automatico

### Documentazione

#### Documentazione Utente
- README.md aggiornato con sezione multi-lingua
- Esempi di configurazione JSON
- Tabella delle lingue supportate

#### Documentazione Sviluppatore
- LANGUAGE_SUPPORT.md con dettagli tecnici
- Esempi di codice per integrazione
- Note di implementazione

### Note di Sviluppo

#### Architettura
- Sistema modulare per facile aggiunta di nuove lingue
- Separazione tra logica di traduzione e interfaccia
- Gestione centralizzata delle configurazioni

#### Performance
- Traduzioni caricate in memoria statica
- Nessun overhead significativo per le traduzioni
- Validazione efficiente delle lingue

#### Manutenibilità
- Codice ben documentato
- Struttura modulare
- Test di validazione inclusi
- Log dettagliati per debugging 