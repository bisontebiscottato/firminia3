# 🔄 Firminia Automatic Rollback System Guide

## Overview

The Firminia 3.6.1 firmware includes a comprehensive automatic rollback system that protects against corrupted or invalid firmware installations. This system ensures device reliability and prevents bricking from bad OTA updates.

## 🛡️ Security Features Implemented

### 1. **Automatic Rollback Configuration**
- ✅ **Rollback Enabled**: `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y`
- ✅ **Anti-Rollback**: `CONFIG_APP_ANTI_ROLLBACK=y`
- ✅ **Partition Management**: Automatic partition switching

### 2. **Firmware Validation**
- ✅ **Health Checks**: Validates firmware integrity at boot
- ✅ **Project Verification**: Ensures firmware is for Firminia device
- ✅ **Version Validation**: Checks firmware version format
- ✅ **State Management**: Tracks firmware validity states

### 3. **Boot Watchdog**
- ✅ **Timeout Protection**: 30-second boot timeout
- ✅ **Health Monitoring**: Periodic system responsiveness checks
- ✅ **Emergency Rollback**: Automatic rollback on boot failure
- ✅ **Logging**: Detailed debug information

### 4. **Enhanced Logging**
- ✅ **Debug Information**: Comprehensive rollback logging
- ✅ **Partition States**: Detailed partition information
- ✅ **System Health**: Memory and reset reason tracking
- ✅ **Error Tracking**: Complete error history

## 🔧 How It Works

### Boot Sequence

1. **System Startup**
   ```
   ┌─ Boot Watchdog Started (30s timeout)
   ├─ Firmware Security Checks
   │  ├─ Rollback Check
   │  ├─ Health Validation
   │  └─ Mark as Valid
   ├─ Normal Boot Process
   └─ Watchdog Stopped
   ```

2. **Firmware State Management**
   ```
   ESP_OTA_IMG_PENDING_VERIFY → Mark as VALID
   ESP_OTA_IMG_INVALID → Trigger Rollback
   ESP_OTA_IMG_VALID → Continue Normal Boot
   ```

3. **Rollback Triggers**
   - Firmware marked as INVALID
   - Boot watchdog timeout (30 seconds)
   - System unresponsive during boot

### Rollback Process

1. **Detection**
   - System detects invalid firmware
   - Boot timeout exceeded
   - Health checks fail

2. **Recovery**
   - Find previous valid partition
   - Switch boot partition
   - Reboot system

3. **Verification**
   - New firmware boots successfully
   - Health checks pass
   - System operational

## 📊 Logging and Debugging

### Debug Information Logged

```c
📊 Rollback Debug Info - [Operation]:
  - Result: [ESP_OK/ESP_FAIL]
  - Running partition: [label] (0x[address], [size] bytes)
  - Update partition: [label] (0x[address], [size] bytes)
  - Running partition state: [VALID/INVALID/PENDING_VERIFY]
  - Reset reason: [POWERON/SW/PANIC/WDT/etc]
  - Free heap: [bytes]
  - Min free heap: [bytes]
```

### Key Log Messages

- `🔒 Performing firmware security checks...`
- `🔍 Step 1: Checking for rollback requirements...`
- `🔍 Step 2: Validating firmware health...`
- `🔍 Step 3: Marking firmware as valid...`
- `🚨 BOOT WATCHDOG TIMEOUT!`
- `🔄 Emergency rollback triggered by boot watchdog!`
- `✅ Rollback completed - rebooting...`

## ⚙️ Configuration

### SDKConfig Settings

```ini
# Application Rollback
CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y
CONFIG_APP_ANTI_ROLLBACK=y

# Bootloader Configuration
CONFIG_BOOTLOADER_WDT_ENABLE=y
CONFIG_BOOTLOADER_WDT_TIME_MS=18000
```

### Timeout Settings

```c
#define BOOT_WATCHDOG_TIMEOUT_MS       30000   // 30 seconds
#define BOOT_HEALTH_CHECK_INTERVAL_MS  5000    // 5 seconds
```

## 🧪 Testing the Rollback System

### Test Scenarios

1. **Invalid Firmware Test**
   - Flash corrupted firmware
   - System should detect and rollback
   - Check logs for rollback messages

2. **Boot Timeout Test**
   - Simulate boot hang
   - Watchdog should trigger rollback
   - Verify system recovers

3. **Health Check Test**
   - Flash wrong project firmware
   - System should detect mismatch
   - Log warnings but continue

### Manual Testing

```bash
# Flash invalid firmware
idf.py app-flash

# Monitor logs
idf.py monitor

# Expected behavior:
# 1. Boot starts
# 2. Health check fails
# 3. Rollback triggered
# 4. System reboots with previous firmware
```

## 🚨 Troubleshooting

### Common Issues

1. **Rollback Not Working**
   - Check `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y`
   - Verify partition table has OTA partitions
   - Check logs for error messages

2. **False Rollbacks**
   - Adjust `BOOT_WATCHDOG_TIMEOUT_MS`
   - Check system performance
   - Review health check logic

3. **Boot Loop**
   - Check if all partitions are invalid
   - Verify partition table integrity
   - Use factory reset if needed

### Recovery Procedures

1. **Manual Rollback**
   ```bash
   # Flash specific partition
   idf.py --partition-table partitions.csv app-flash
   ```

2. **Factory Reset**
   ```bash
   # Erase all partitions
   idf.py erase-flash
   idf.py flash
   ```

3. **Emergency Recovery**
   - Hold reset button during boot
   - Use serial bootloader
   - Flash via JTAG if available

## 📈 Performance Impact

### Boot Time
- **Additional Overhead**: ~100-200ms
- **Watchdog Overhead**: ~1ms per check
- **Logging Overhead**: ~10-50ms per operation

### Memory Usage
- **Additional RAM**: ~2KB for watchdog variables
- **Flash Usage**: ~5KB for rollback functions
- **Stack Usage**: Minimal impact

## 🔮 Future Enhancements

### Planned Features
- [ ] **Signature Verification**: Add RSA signature checking
- [ ] **Secure Boot**: Enable hardware-based security
- [ ] **Remote Rollback**: Server-triggered rollbacks
- [ ] **Metrics Collection**: Rollback statistics
- [ ] **A/B Testing**: Gradual firmware rollouts

### Advanced Rollback
- [ ] **Partial Rollbacks**: Rollback specific components
- [ ] **Conditional Rollbacks**: Rollback based on metrics
- [ ] **Rollback Scheduling**: Time-based rollback windows
- [ ] **User Confirmation**: Manual rollback approval

## 📞 Support

For issues with the rollback system:
- Check logs for detailed error information
- Verify configuration settings
- Test with known good firmware
- Contact development team with logs

---

**⚠️ IMPORTANT**: Always test rollback functionality in development before deploying to production devices!

**✅ BENEFITS**: This rollback system provides robust protection against firmware issues, ensuring high device reliability and preventing bricking from bad OTA updates.
