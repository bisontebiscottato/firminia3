# 🧪 **FIRMINIA ROLLBACK TEST SUITE**

## 📋 **Panoramica**

Questa suite di test verifica il funzionamento del sistema di rollback automatico implementato in Firminia 3.6.1. I test simulano diversi scenari di errore per verificare che il sistema protegga il dispositivo da firmware corrotti o problematici.

## 🎯 **Test Disponibili**

### **Test 1: Firmware Corrotto** 🔴
- **Obiettivo**: Verificare rollback automatico con firmware INVALID
- **Metodo**: Simula crash intenzionale del sistema
- **Risultato atteso**: Sistema crasha, rollback automatico, riavvio con firmware precedente
- **⚠️ ATTENZIONE**: Questo test causa un crash del sistema!

### **Test 2: Boot Watchdog** 🟡
- **Obiettivo**: Verificare rollback di emergenza su timeout boot
- **Metodo**: Simula boot che richiede più di 30 secondi
- **Risultato atteso**: Watchdog triggera dopo 30s, rollback automatico
- **⏱️ TEMPO**: Questo test richiede 30+ secondi per completarsi

### **Test 3: Validazione Firmware** 🟢
- **Obiettivo**: Verificare controlli di integrità
- **Metodo**: Testa validazione partizioni e metadati
- **Risultato atteso**: Warning nei log, ma NO rollback
- **✅ SICURO**: Questo test non causa crash

## 🚀 **Come Eseguire i Test**

### **Metodo 1: Script PowerShell (Raccomandato)**

```powershell
# Test singoli
.\test_rollback.ps1 -Test 1    # Test firmware corrotto
.\test_rollback.ps1 -Test 2    # Test boot watchdog
.\test_rollback.ps1 -Test 3    # Test validazione firmware

# Tutti i test
.\test_rollback.ps1 -All

# Con monitoraggio
.\test_rollback.ps1 -Test 1 -Monitor

# Mostra aiuto
.\test_rollback.ps1 -Help
```

### **Metodo 2: Configurazione Manuale**

1. **Modifica `main/main_flow.c`**:
   ```c
   #define ENABLE_ROLLBACK_TESTS          1
   #define TEST_FIRMWARE_CORRUPTION       1  // Per test 1
   #define TEST_BOOT_WATCHDOG             1  // Per test 2
   #define TEST_FIRMWARE_VALIDATION      1  // Per test 3
   ```

2. **Compila e flash**:
   ```bash
   idf.py build
   idf.py flash
   ```

3. **Monitora i log**:
   ```bash
   idf.py monitor
   ```

## 📊 **Cosa Monitorare**

### **Log Messages Chiave**
```
🧪 TEST 1: Simulating corrupted firmware...
🧪 TEST: Triggering intentional crash for rollback test
🚨 Current firmware is INVALID - performing rollback!
🔄 Rolling back to partition: ota_0 (offset: 0x10000)
✅ Rollback completed - rebooting in 3 seconds...
```

### **Timing Critici**
- **Test 1**: Crash dovrebbe avvenire entro 2 secondi
- **Test 2**: Watchdog dovrebbe triggerare dopo 30 secondi
- **Test 3**: Warning dovrebbero apparire immediatamente

### **Comportamenti Attesi**
- **Test 1**: Sistema crasha → Rollback → Riavvio automatico
- **Test 2**: Boot lento → Watchdog → Rollback → Riavvio automatico
- **Test 3**: Warning nei log → Sistema continua normalmente

## ⚠️ **Precauzioni di Sicurezza**

### **Prima di Eseguire i Test**
1. **Backup del firmware funzionante**
2. **Dispositivo di test dedicato** (non produzione)
3. **Accesso fisico al dispositivo** per recovery manuale
4. **Monitoraggio continuo** dei log

### **Durante i Test**
1. **Non interrompere** il processo di flash
2. **Monitorare i log** in tempo reale
3. **Aspettare** il completamento del rollback
4. **Verificare** che il sistema si riavvii correttamente

### **Dopo i Test**
1. **Verificare** che il sistema funzioni normalmente
2. **Controllare** che la configurazione sia preservata
3. **Disabilitare** i test per uso normale
4. **Documentare** i risultati

## 🔧 **Configurazione Avanzata**

### **Personalizzazione Test**
```c
// In main/main_flow.c
#define TEST_CRASH_DELAY_MS            2000    // Delay prima del crash
#define TEST_BOOT_SIMULATION_SEC       40      // Durata simulazione boot
#define TEST_WATCHDOG_TIMEOUT_SEC      30      // Timeout watchdog
```

### **Logging Dettagliato**
```c
// Abilita logging dettagliato
#define CONFIG_LOG_DEFAULT_LEVEL_DEBUG
#define CONFIG_LOG_MAXIMUM_LEVEL_DEBUG
```

## 📈 **Risultati Attesi**

### **Test 1 - Firmware Corrotto**
```
✅ SUCCESS: Sistema crasha come previsto
✅ SUCCESS: Rollback automatico eseguito
✅ SUCCESS: Riavvio con firmware precedente
✅ SUCCESS: Sistema funziona normalmente dopo rollback
```

### **Test 2 - Boot Watchdog**
```
✅ SUCCESS: Boot richiede >30 secondi
✅ SUCCESS: Watchdog triggera dopo 30s
✅ SUCCESS: Rollback di emergenza eseguito
✅ SUCCESS: Riavvio con firmware precedente
```

### **Test 3 - Validazione Firmware**
```
✅ SUCCESS: Warning appaiono nei log
✅ SUCCESS: Nessun rollback eseguito
✅ SUCCESS: Sistema continua normalmente
✅ SUCCESS: Funzionalità preservate
```

## 🐛 **Troubleshooting**

### **Problemi Comuni**
1. **Test non si attiva**: Verificare che `ENABLE_ROLLBACK_TESTS = 1`
2. **Crash non avviene**: Verificare che `TEST_FIRMWARE_CORRUPTION = 1`
3. **Watchdog non triggera**: Verificare che `TEST_BOOT_WATCHDOG = 1`
4. **Rollback non funziona**: Verificare configurazione partizioni OTA

### **Recovery Manuale**
```bash
# Se il sistema non si riavvia automaticamente
idf.py erase-flash
idf.py flash

# O flash di un firmware noto funzionante
idf.py app-flash --partition-table partitions.csv
```

## 📝 **Documentazione Risultati**

### **Template di Report**
```
Test Date: [DATA]
Test Version: Firminia 3.6.1
Hardware: [MODELO DISPOSITIVO]

Test 1 - Firmware Corrotto:
- Status: [PASS/FAIL]
- Crash Time: [TEMPO]
- Rollback Time: [TEMPO]
- Recovery Time: [TEMPO]

Test 2 - Boot Watchdog:
- Status: [PASS/FAIL]
- Watchdog Trigger: [TEMPO]
- Rollback Time: [TEMPO]
- Recovery Time: [TEMPO]

Test 3 - Validazione Firmware:
- Status: [PASS/FAIL]
- Warnings: [NUMERO]
- Rollback: [YES/NO]
- System Status: [NORMAL/ERROR]
```

## 🎉 **Conclusione**

Questa suite di test verifica che il sistema di rollback automatico di Firminia 3.6.1 funzioni correttamente e protegga il dispositivo da firmware problematici. I test coprono tutti gli scenari critici e garantiscono la robustezza del sistema.

**⚠️ IMPORTANTE**: Eseguire sempre i test su dispositivi di test, non in produzione!
