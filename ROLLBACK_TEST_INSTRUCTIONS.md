# 🚨 **ISTRUZIONI PER TESTARE IL ROLLBACK**

## 🔧 **Problema Risolto**

Il sistema di rollback è stato corretto per gestire correttamente i crash del firmware. Ora il sistema dovrebbe:

1. ✅ Rilevare quando il firmware è stato riavviato a causa di un crash
2. ✅ Eseguire automaticamente il rollback alla partizione precedente
3. ✅ Evitare il loop infinito di riavvio

## 🧪 **Test da Eseguire**

### **Test 1: Test Sicuro (Raccomandato)**
```c
// In main/main_flow.c, modifica:
#define ENABLE_ROLLBACK_TESTS          1
#define TEST_PROBLEMATIC_FIRMWARE      1  // Test più sicuro
```

**Cosa fa**: Simula un firmware problematico che causa problemi ma non crash immediato
**Risultato atteso**: Sistema rileva problemi e fa rollback automatico

### **Test 2: Test Crash (Solo se Test 1 funziona)**
```c
// In main/main_flow.c, modifica:
#define ENABLE_ROLLBACK_TESTS          1
#define TEST_FIRMWARE_CORRUPTION       1  // Test crash
```

**Cosa fa**: Simula un crash immediato del firmware
**Risultato atteso**: Sistema crasha, rileva il crash al riavvio, fa rollback automatico

### **Test 3: Test Validazione (Sicuro)**
```c
// In main/main_flow.c, modifica:
#define ENABLE_ROLLBACK_TESTS          1
#define TEST_FIRMWARE_VALIDATION      1  // Test sicuro
```

**Cosa fa**: Testa solo la validazione del firmware
**Risultato atteso**: Warning nei log, ma NO rollback

## 🔄 **Come Testare**

### **Passo 1: Compila e Flash**
```bash
# Attiva l'ambiente ESP-IDF
C:\Users\biso\esp\v5.5\esp-idf\export.bat

# Compila
idf.py build

# Flash
idf.py flash

# Monitora
idf.py monitor
```

### **Passo 2: Osserva i Log**
Cerca questi messaggi chiave:

```
🔍 Reset reason: 4  # 4 = ESP_RST_PANIC (crash)
🚨 System crashed (reason: 4) - checking for rollback...
🚨 Attempting emergency rollback due to crash...
🔄 Emergency rollback to partition: ota_0 (offset: 0x00010000)
✅ Emergency rollback completed - rebooting in 3 seconds...
```

### **Passo 3: Verifica il Rollback**
Dopo il rollback, il sistema dovrebbe:
1. Riavviarsi con la partizione precedente
2. Funzionare normalmente
3. Non entrare più in loop di riavvio

## 🛠️ **Correzioni Implementate**

### **1. Rilevamento Crash Migliorato**
```c
// Rileva crash e forza rollback
if (reset_reason == ESP_RST_PANIC || reset_reason == ESP_RST_INT_WDT || 
    reset_reason == ESP_RST_TASK_WDT || reset_reason == ESP_RST_WDT) {
    // Esegue rollback di emergenza
}
```

### **2. Rollback di Emergenza**
```c
// Se il firmware è PENDING_VERIFY e c'è stato un crash
if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
    esp_ota_mark_app_invalid_rollback_and_reboot();
}
```

### **3. Rollback Manuale**
```c
// Rollback alla partizione opposta
const esp_partition_t* other_partition = esp_ota_get_next_update_partition(NULL);
esp_ota_set_boot_partition(other_partition);
```

## ⚠️ **Precauzioni**

1. **Sempre testare prima con TEST_PROBLEMATIC_FIRMWARE**
2. **Avere un firmware di backup pronto**
3. **Monitorare sempre i log durante i test**
4. **Disabilitare i test dopo aver verificato il funzionamento**

## 🎯 **Risultato Atteso**

Dopo aver implementato le correzioni, il sistema dovrebbe:

1. ✅ Rilevare automaticamente i crash del firmware
2. ✅ Eseguire rollback automatico alla partizione precedente
3. ✅ Evitare loop infiniti di riavvio
4. ✅ Ripristinare il funzionamento normale del dispositivo

## 📝 **Note di Debug**

Se il rollback non funziona ancora:

1. **Verifica le partizioni OTA**: `idf.py partition_table`
2. **Controlla la configurazione**: `idf.py menuconfig` → Application Level Tracing
3. **Monitora i log**: Cerca messaggi di errore specifici
4. **Testa manualmente**: Usa `esp_ota_set_boot_partition()` manualmente

Il sistema ora dovrebbe funzionare correttamente! 🚀
