# 🔄 Guida al Nuovo Standard JSON con Aggiornamenti Parziali

## 📋 Panoramica

Firminia 3.6.1 ora supporta un nuovo standard JSON che permette aggiornamenti parziali della configurazione utilizzando flag di aggiornamento. Questo consente di modificare solo specifici parametri senza dover inviare l'intera configurazione.

## 🆕 Due Modalità Supportate

### 1. **Modalità Tradizionale** (Retrocompatibile)
Tutti i campi devono essere presenti e validi. Questa modalità continua a funzionare come prima per mantenere la compatibilità con le applicazioni esistenti.

### 2. **Modalità Aggiornamenti Parziali** (Nuova)
Solo i campi con flag `_updated_*` impostati a `true` vengono validati e aggiornati.

---

## 📝 Formato JSON Tradizionale

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
    "language": "1",
    "working_mode": "0"
}
```

**Comportamento:** Tutti i campi sono obbligatori e vengono aggiornati.

---

## 🎯 Formato JSON con Aggiornamenti Parziali

```json
{
    "ssid": "NewWiFi",
    "password": "newpassword123",
    "server": "sign.askme.it",
    "port": "443",
    "url": "https://sign.askme.it/api/v2/files/pending?page=0&size=1",
    "token": "abc123",
    "user": "mario.rossi",
    "interval": "300000",
    "language": "0",
    "working_mode": "0",
    "_updated_ssid": true,
    "_updated_password": true,
    "_updated_server": false,
    "_updated_port": false,
    "_updated_url": false,
    "_updated_token": false,
    "_updated_user": false,
    "_updated_interval": true,
    "_updated_language": false,
    "_updated_working_mode": false
}
```

**Comportamento:** Solo i campi con flag `true` vengono validati e aggiornati (in questo esempio: `ssid`, `password` e `interval`).

---

## 🏷️ Flag di Aggiornamento Disponibili

| Flag | Campo Corrispondente | Descrizione |
|------|---------------------|-------------|
| `_updated_ssid` | `ssid` | Nome rete Wi-Fi |
| `_updated_password` | `password` | Password Wi-Fi |
| `_updated_server` | `server` | Server di destinazione |
| `_updated_port` | `port` | Porta del server |
| `_updated_url` | `url` | URL dell'API |
| `_updated_token` | `token` | Token di autenticazione |
| `_updated_user` | `user` | Identificativo utente |
| `_updated_interval` | `interval` | Intervallo di polling (ms) |
| `_updated_language` | `language` | Lingua interfaccia (0=EN, 1=IT, 2=FR, 3=ES) |
| `_updated_working_mode` | `working_mode` | Modalità operativa (0=Signer, 1=Editor) |

---

## ⚙️ Come Funziona il Rilevamento

Il sistema rileva automaticamente il tipo di JSON:

1. **Se è presente almeno un flag `_updated_*`** → Modalità aggiornamenti parziali
2. **Se non sono presenti flag** → Modalità tradizionale

---

## ✅ Esempi di Utilizzo

### Esempio 1: Aggiornare solo Wi-Fi
```json
{
    "ssid": "NuovaRete",
    "password": "nuovapassword",
    "_updated_ssid": true,
    "_updated_password": true,
    "_updated_server": false,
    "_updated_port": false,
    "_updated_url": false,
    "_updated_token": false,
    "_updated_user": false,
    "_updated_interval": false,
    "_updated_language": false,
    "_updated_working_mode": false
}
```

### Esempio 2: Cambiare solo lingua e modalità operativa
```json
{
    "language": "2",
    "working_mode": "1",
    "_updated_ssid": false,
    "_updated_password": false,
    "_updated_server": false,
    "_updated_port": false,
    "_updated_url": false,
    "_updated_token": false,
    "_updated_user": false,
    "_updated_interval": false,
    "_updated_language": true,
    "_updated_working_mode": true
}
```

### Esempio 3: Aggiornare solo l'intervallo di polling
```json
{
    "interval": "60000",
    "_updated_ssid": false,
    "_updated_password": false,
    "_updated_server": false,
    "_updated_port": false,
    "_updated_url": false,
    "_updated_token": false,
    "_updated_user": false,
    "_updated_interval": true,
    "_updated_language": false,
    "_updated_working_mode": false
}
```

---

## 🚨 Comportamenti e Validazioni

### Modalità Tradizionale
- ✅ Tutti i campi devono essere presenti e validi
- ✅ Tutti i campi vengono aggiornati
- ❌ Se un campo manca o non è valido → configurazione rifiutata

### Modalità Aggiornamenti Parziali
- ✅ Solo i campi con flag `true` devono essere presenti e validi
- ✅ Solo i campi con flag `true` vengono aggiornati **e salvati in NVS**
- ✅ I campi con flag `false` vengono completamente ignorati (possono anche mancare o avere valori non validi)
- ✅ I campi esistenti non marcati per l'aggiornamento rimangono invariati
- ❌ Se nessun campo ha flag `true` → configurazione rifiutata
- ❌ Se un campo marcato per l'aggiornamento non è valido → configurazione rifiutata

---

## 📊 Log di Sistema

Il sistema fornisce log dettagliati per distinguere le modalità:

### Modalità Tradizionale
```
📋 Rilevato JSON tradizionale (configurazione completa)
✅ Configurazione aggiornata e salvata in NVS!
```

### Modalità Aggiornamenti Parziali
```
🔄 Rilevato JSON con aggiornamenti parziali (con flag)
✅ SSID aggiornato: NuovaRete
✅ Password WiFi aggiornata
✅ Intervallo API aggiornato: 60000 ms
✅ Configurazione parziale aggiornata e salvata in NVS!
```

### Errori Comuni
```
❌ Campo 'ssid' marcato per aggiornamento ma non valido
⚠️ Nessun campo marcato per l'aggiornamento. Configurazione non modificata.
```

---

## 🔧 Implementazione nell'App

Per utilizzare la nuova funzionalità nella tua app:

1. **Determina quali campi aggiornare**
2. **Imposta i flag corrispondenti a `true`**
3. **Imposta tutti gli altri flag a `false`**
4. **Includi i valori solo per i campi che stai aggiornando**
5. **Invia il JSON via BLE**

---

## 🔄 Retrocompatibilità

✅ **Garantita al 100%**

Le app esistenti che inviano JSON nel formato tradizionale continueranno a funzionare senza modifiche. Il nuovo sistema rileva automaticamente il formato e applica la logica appropriata.

---

## 💡 Vantaggi del Nuovo Sistema

- **🎯 Aggiornamenti mirati:** Modifica solo i parametri necessari
- **📱 App più efficienti:** Meno dati da trasmettere via BLE
- **🔒 Maggiore sicurezza:** Evita sovrascritture accidentali
- **🔄 Retrocompatibilità:** Nessuna interruzione per le app esistenti
- **📝 Log chiari:** Tracciamento dettagliato delle modifiche

---

## 🚀 Versione Firmware

Questa funzionalità è disponibile a partire da **Firminia 3.6.1**.

Per verificare la versione del firmware, controlla i log di avvio del dispositivo.
