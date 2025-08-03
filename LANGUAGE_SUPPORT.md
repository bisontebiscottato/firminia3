# Sistema di Localizzazione - Firminia V3

## Panoramica

Firminia V3 ora supporta il multi-lingua per l'interfaccia utente. Il sistema permette di visualizzare i testi in inglese, italiano, francese e spagnolo.

## Lingue Supportate

- **0** - English (Inglese) - Lingua di default
- **1** - Italiano
- **2** - Francais (Francese)
- **3** - Espanol (Spagnolo)

## Configurazione via BLE

La lingua può essere configurata tramite BLE aggiungendo il parametro `language` al JSON di configurazione:

```json
{
    "ssid": "your_wifi_ssid",
    "password": "your_wifi_password",
    "server": "sign.askme.it",
    "port": "443",
    "url": "https://sign.askme.it/api/v2/files/pending?page=0&size=1",
    "token": "your_api_token",
    "user": "your_user_identifier",
    "interval": "30000",
    "language": "1"
}
```

### Parametri del JSON

| Parametro | Descrizione | Valori |
|-----------|-------------|---------|
| `language` | Lingua dell'interfaccia | "0" (EN), "1" (IT), "2" (FR), "3" (ES) |

## Testi Tradotti

### Stati del Display

| Stato | Inglese | Italiano | Francese | Spagnolo |
|-------|---------|----------|----------|----------|
| Warming up | Warming up... | Riscaldamento in corso... | Mise en marche... | Calentando... |
| Waiting config | Waiting for config... | In attesa di configurazione... | En attente de configuration... | Esperando configuración... |
| Config updated | Configuration updated! | Configurazione aggiornata! | Configuration mise a jour! | Configuracion actualizada! |
| Connecting WiFi | Connecting to Wi-Fi... | Connessione a Wi-Fi... | Connexion au Wi-Fi... | Conectando a Wi-Fi... |
| Checking signatures | Checking signatures for %s... | Controllo firme per %s... | Verification des signatures pour %s... | Verificando firmas para %s... |
| Dossier to sign | dossier to sign! | pratica da firmare! | enveloppe a signer! | practica para firmar! |
| Dossiers to sign | dossiers to sign! | pratiche da firmare! | enveloppes a signer! | practicas para firmar! |
| No dossiers | No dossiers to sign. Relax. | Nessuna pratica da firmare. Rilassati. | Aucune enveloppe a signer. Detendez-vous. | No hay practicas para firmar. Relajate. |
| No WiFi sleeping | No Wi-Fi. sleeping... | Nessun Wi-Fi. in attesa... | Pas de Wi-Fi. en veille... | Sin Wi-Fi. durmiendo... |
| API Error | API error! E-002 | Errore API! E-002 | Erreur API! E-002 | Error API! E-002 |
| Unknown state | Unknown state. E-003 | Stato sconosciuto. E-003 | Etat inconnu. E-003 | Estado desconocido. E-003 |

## Implementazione Tecnica

### File Principali

- `main/translations.h` - Header con definizioni delle lingue e stringhe
- `main/translations.c` - Implementazione del sistema di traduzioni
- `main/device_config.h` - Aggiunta del parametro language
- `main/device_config.c` - Gestione del salvataggio/caricamento della lingua
- `main/ble_manager.c` - Validazione e gestione del parametro language
- `main/display_manager.c` - Utilizzo delle traduzioni nell'interfaccia

### Funzioni Principali

```c
// Ottiene una stringa tradotta
const char* get_translated_string(string_id_t string_id, language_t language);

// Imposta la lingua corrente
void set_current_language(language_t language);

// Ottiene la lingua corrente
language_t get_current_language(void);

// Verifica se una lingua è valida
bool is_valid_language(language_t language);
```

### Salvataggio in NVS

La lingua viene salvata nel NVS con la chiave `"language"` come stringa numerica:
- "0" = English
- "1" = Italiano  
- "2" = Français
- "3" = Español

## Esempi di Utilizzo

### Configurazione via React App

```javascript
const config = {
    ssid: "MyWiFi",
    password: "mypassword",
    server: "sign.askme.it",
    port: "443",
    url: "https://sign.askme.it/api/v2/files/pending?page=0&size=1",
    token: "your_token",
    user: "user@example.com",
    interval: "30000",
    language: "1"  // Italiano
};
```

### Verifica della Lingua

Per verificare la lingua corrente, controllare i log del dispositivo:

```
I (1234) DeviceConfig: Language: 1
I (1235) Translations: Language set to: Italiano
```

## Note di Implementazione

1. **Fallback**: Se viene specificata una lingua non valida, il sistema usa l'inglese come fallback
2. **Persistenza**: La lingua viene salvata nel NVS e ripristinata al riavvio
3. **Validazione**: Il parametro language viene validato nel BLE manager
4. **Aggiornamento dinamico**: La lingua può essere cambiata senza riavviare il dispositivo

## Compatibilità

Il sistema di localizzazione è retrocompatibile:
- Se il parametro `language` non è presente nel JSON, viene usata la lingua di default (inglese)
- I dispositivi esistenti continueranno a funzionare normalmente
- La lingua di default è l'inglese per garantire la compatibilità 