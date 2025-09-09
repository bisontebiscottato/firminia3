# Guida alla Conversione dell'Immagine QR Code

## 📱 Conversione dell'Immagine QR per LVGL

### 🔧 Strumenti Necessari

Per convertire l'immagine QR code allegata in un formato utilizzabile da LVGL, puoi utilizzare:

1. **LVGL Image Converter Online**: https://lvgl.io/tools/imageconverter
2. **LVGL Image Converter Tool**: Disponibile nel repository LVGL

### 📋 Procedura di Conversione

#### **Passo 1: Preparazione dell'Immagine**
1. Assicurati che l'immagine QR sia **100x100 pixel**
2. Formato preferito: **PNG** o **BMP**
3. **Monocromatica** (bianco e nero) per dimensioni ottimali

#### **Passo 2: Conversione Online**
1. Vai su https://lvgl.io/tools/imageconverter
2. Carica la tua immagine QR code
3. Imposta i seguenti parametri:
   - **Color format**: `Indexed 1 bit (I1)` per immagini monocromatiche
   - **Output format**: `C array`
   - **Name**: `qr_code_img`
   - **LVGL version**: `9.x` (importante per la struttura corretta)

#### **Passo 3: Integrazione nel Codice**
1. Copia l'array C generato
2. Sostituisci il contenuto del file `main/qr_image.c`
3. Aggiorna i dati nell'array `qr_code_data[]`

### 📝 Esempio di Struttura Dati

```c
// Esempio per LVGL 9.x
static const uint8_t qr_code_data[] = {
    // Dati dell'immagine convertita (esempio)
    0xFF, 0x81, 0xBD, 0xA1, 0xBD, 0x81, 0xFF, 0x00,
    0x80, 0x00, 0x80, 0x80, 0x80, 0x00, 0x80, 0x00,
    // ... continua con tutti i dati dell'immagine
};

const lv_image_dsc_t qr_code_img = {
    .header.magic = LV_IMAGE_HEADER_MAGIC,
    .header.cf = LV_COLOR_FORMAT_I1,
    .header.flags = 0,
    .header.w = 100,
    .header.h = 100,
    .header.stride = 13,  // (100 + 7) / 8 = 13 bytes per row
    .data_size = sizeof(qr_code_data),
    .data = qr_code_data,
};
```

### ⚡ Conversione Manuale Alternativa

Se preferisci una conversione manuale:

```bash
# Usando ImageMagick per convertire in bitmap
convert qr_code.png -resize 100x100 -monochrome qr_code.bmp

# Poi usa uno script Python per convertire in array C
python3 img_to_c_array.py qr_code.bmp
```

### 🔧 Attivazione QR Code Reale

Per passare dal placeholder al QR code reale:

1. **Converti l'immagine** seguendo i passi sopra
2. **Sostituisci i dati** in `main/qr_image.c`  
3. **Attiva la modalità QR** in `main/display_manager.c`:
   ```c
   // Cambia da:
   #define QR_DISPLAY_MODE 0  // placeholder
   
   // A:
   #define QR_DISPLAY_MODE 1  // QR reale
   ```
4. **Ricompila** il progetto

### 🎯 Risultato Finale

Dopo la configurazione, il display mostrerà:
- **Modalità 0 (Placeholder)**: Rettangolo bianco con testo "QR CODE"
- **Modalità 1 (QR Reale)**: La tua immagine QR convertita
- **Alternanza ogni 3 secondi** con il testo BLE
- **Centrato sullo schermo** (100x100 pixel)
- **Arco blu rotante** sempre visibile in background

### 🔍 Verifica

Per verificare che l'immagine sia stata convertita correttamente:
1. Compila il progetto: `idf.py build`
2. Flash sul dispositivo: `idf.py flash`
3. Entra in modalità BLE e osserva l'alternanza

### 📱 Note Importanti

- L'immagine deve essere **esattamente 100x100 pixel**
- Il formato `Indexed 1 bit` è ottimale per QR code monocromatici
- L'alternanza inizia automaticamente quando si entra in modalità BLE
- Il timer si ferma automaticamente quando si esce dalla modalità BLE

### 🛠️ Troubleshooting

**Problema**: Immagine non visualizzata
- **Soluzione**: Verifica che l'array sia correttamente formattato e che le dimensioni siano 100x100

**Problema**: Immagine distorta
- **Soluzione**: Assicurati che il formato colore sia `Indexed 1 bit` per immagini monocromatiche

**Problema**: Errori di compilazione
- **Soluzione**: Verifica che `qr_image.c` sia incluso nel `CMakeLists.txt`
