# 🔒 Firminia OTA Secure Boot Configuration Guide

## Overview

This guide explains how to configure ESP32 Secure Boot and signed firmware for Firminia's secure OTA system.

## 🔑 Prerequisites

- ESP-IDF v5.3.1 or later
- OpenSSL installed on development machine
- Production signing keys (keep secure!)

## 📋 Step-by-Step Configuration

### 1. Generate Signing Keys

```bash
# Generate RSA private key for firmware signing
openssl genrsa -out firminia_signing_key.pem 2048

# Extract public key
openssl rsa -in firminia_signing_key.pem -pubout -out firminia_public_key.pem

# Generate secure boot key (keep this VERY secure!)
espsecure.py generate_signing_key --version 2 secure_boot_signing_key.pem
```

### 2. Configure ESP-IDF for Secure Boot

Add to `sdkconfig`:

```ini
# Secure Boot Configuration
CONFIG_SECURE_BOOT=y
CONFIG_SECURE_BOOT_V2_ENABLED=y
CONFIG_SECURE_BOOT_SIGNING_KEY="secure_boot_signing_key.pem"
CONFIG_SECURE_BOOT_BUILD_SIGNED_BINARIES=y
CONFIG_SECURE_BOOT_INSECURE=n

# Flash Encryption (recommended with Secure Boot)
CONFIG_SECURE_FLASH_ENC_ENABLED=y
CONFIG_SECURE_FLASH_ENCRYPTION_MODE_DEVELOPMENT=n
CONFIG_SECURE_FLASH_ENCRYPTION_MODE_RELEASE=y

# OTA Configuration
CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y
CONFIG_BOOTLOADER_APP_ANTI_ROLLBACK=y
CONFIG_APP_ANTI_ROLLBACK=y
```

### 3. Update Partition Table for Secure Boot

The current `partitions.csv` is already configured correctly:

```csv
# Name,   Type, SubType, Offset,   Size
nvs,      data, nvs,      0x9000,   24K
phy_init, data, phy,      0xf000,   4K
factory,  app,  factory,  0x10000,  1536K
ota_0,    app,  ota_0,    0x190000, 1536K  
ota_1,    app,  ota_1,    0x310000, 1536K
ota_data, data, ota,      0x3f0000, 8K
```

### 4. Build Signed Firmware

```bash
# Clean build with secure boot
idf.py fullclean
idf.py build

# The build process will automatically create signed binaries:
# - build/bootloader/bootloader.bin (signed)
# - build/firminia3.bin (signed)
```

### 5. Flash Initial Firmware (Development)

⚠️ **WARNING**: This burns eFuses and enables secure boot permanently!

```bash
# Flash bootloader and enable secure boot (ONE TIME ONLY!)
idf.py bootloader-flash

# Flash the signed application
idf.py app-flash

# Or flash everything at once
idf.py flash
```

### 6. Production Firmware Signing Process

For production OTA updates:

```bash
# Sign firmware for OTA distribution
espsecure.py sign_data --keyfile firminia_signing_key.pem \
    --output firminia_v3.6.1_signed.bin \
    build/firminia3.bin

# Generate signature for verification
openssl dgst -sha256 -sign firminia_signing_key.pem \
    -out firminia_v3.6.1.sig \
    build/firminia3.bin
```

### 7. Update Public Key in Code

Replace the placeholder public key in `main/ota_manager.c`:

```c
// Replace this with your actual production public key
static const char* RSA_PUBLIC_KEY = 
"-----BEGIN PUBLIC KEY-----\n"
"[YOUR ACTUAL PUBLIC KEY HERE]\n"
"-----END PUBLIC KEY-----";
```

## 🔒 Security Best Practices

### Key Management

1. **Secure Boot Key**: Store in secure hardware (HSM) or encrypted storage
2. **Signing Key**: Use different keys for development and production
3. **Key Rotation**: Plan for key rotation strategy
4. **Backup**: Secure backup of all keys

### Development vs Production

| Environment | Secure Boot | Flash Encryption | Key Storage |
|-------------|-------------|------------------|-------------|
| Development | Optional | Development Mode | Local Files |
| Production | **Required** | Release Mode | HSM/Secure Storage |

### OTA Security Features

- ✅ **Signature Verification**: Every firmware is verified before installation
- ✅ **Rollback Protection**: Prevents downgrade attacks
- ✅ **Encrypted Transport**: HTTPS for firmware downloads
- ✅ **Integrity Checks**: SHA256 checksums
- ✅ **Anti-Rollback**: Version-based rollback protection

## 📊 Server-Side Requirements

### AskMeSign API Endpoints

Add these endpoints to your server:

```
GET /api/v2/firmware/check?device=firminia&current=3.6.1
Response: {
  "update_available": true,
  "version": "3.6.1",
  "download_url": "https://updates.askme.it/firminia/v3.6.1/firmware.bin",
  "signature_url": "https://updates.askme.it/firminia/v3.6.1/firmware.sig",
  "size": 1048576,
  "checksum": "sha256_hex_string"
}
```

### File Structure on Server

```
/updates/firminia/
├── v3.6.1/
│   ├── firmware.bin
│   ├── firmware.sig
│   └── manifest.json
├── v3.6.1/
│   ├── firmware.bin (signed)
│   ├── firmware.sig
│   └── manifest.json
└── latest.json
```

## 🚨 Important Warnings

1. **Secure Boot is PERMANENT**: Once enabled, it cannot be disabled
2. **Key Loss = Brick**: Losing the secure boot key makes the device unrecoverable
3. **Test Thoroughly**: Test the complete OTA flow in development first
4. **Flash Encryption**: Consider enabling flash encryption for additional security

## 🧪 Testing Procedure

1. **Development Testing**:
   ```bash
   # Build and flash development firmware
   idf.py build flash monitor
   
   # Test OTA update process
   # Verify signature verification
   # Test rollback functionality
   ```

2. **Production Testing**:
   - Test with production-signed firmware
   - Verify secure boot chain
   - Test OTA with invalid signatures (should fail)
   - Test rollback scenarios

## 📞 Support

For issues with secure boot configuration:
- ESP-IDF Documentation: [Secure Boot V2](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/security/secure-boot-v2.html)
- Espressif Support Forums
- ESP32 Security Guide

---

**⚠️ CRITICAL**: Always test secure boot configuration in development before applying to production devices!
