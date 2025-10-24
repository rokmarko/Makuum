# Real BLE Implementation Guide

This file explains how to replace the simulation with real BLE functionality when needed.

## Current Status

The project currently uses **simulation mode** for BLE connectivity, which allows you to:
- ✅ Test the complete vacuum state machine
- ✅ Verify LED patterns work correctly  
- ✅ Build and flash without BLE configuration issues
- ✅ Understand the project structure and logic

## Why Simulation Mode?

ESP-IDF v5.5.1 has significant changes to the BLE API, and proper BLE configuration requires:
1. **Menuconfig setup** - Enabling BLE components correctly
2. **Header compatibility** - Using the right API calls for this ESP-IDF version  
3. **Memory management** - Proper Bluetooth controller initialization
4. **GATT service setup** - Complex characteristic and service registration

## When to Implement Real BLE

Implement real BLE when you need:
- **Actual device communication** with AWS tools or Makita devices
- **Production deployment** for real vacuum control
- **Mobile app integration** for remote control

## Implementation Steps

### Step 1: Enable BLE in Configuration

```bash
cd /home/rok/src/Makuum
idf.py menuconfig
```

Navigate to:
- `Component config` → `Bluetooth` → Enable Bluetooth
- `Bluetooth` → `Bluedroid Options` → Enable BLE
- `Bluetooth` → `Controller Options` → Configure as needed

### Step 2: Use the Pre-built BLE Implementation

The project includes a complete BLE GATT server implementation:

```bash
# Replace simulation with real BLE
cd /home/rok/src/Makuum/main
mv bt_manager.c bt_manager_simulation.c
mv bt_manager_complex_ble.c bt_manager.c
```

### Step 3: Update Configuration Files

Update `sdkconfig.defaults`:
```bash
# Enable BLE components
CONFIG_BT_ENABLED=y
CONFIG_BT_BLUEDROID_ENABLED=y
CONFIG_BT_BLE_ENABLED=y
CONFIG_BT_GATTS_ENABLE=y
```

### Step 4: Test BLE Functionality

1. **Build and flash**:
   ```bash
   idf.py build flash monitor
   ```

2. **Connect with BLE app**:
   - Use nRF Connect (mobile)
   - Look for "Makita_Vacuum" device
   - Connect to GATT service 0x00FF
   - Write to characteristic 0xFF01

3. **Send test commands**:
   - Write "POWER_ON" to activate vacuum
   - Write "POWER_OFF" to deactivate
   - Write "MAKITA_POWER_ON" for specific trigger

## BLE Service Details

The real BLE implementation provides:

### GATT Service
- **Service UUID**: `0x00FF`
- **Characteristic UUID**: `0xFF01`
- **Properties**: Read, Write, Notify

### Supported Commands
```
Activation:          Deactivation:
- POWER_ON          - POWER_OFF  
- MAKITA_POWER_ON   - OFF
- TOOL_ACTIVATED    - STOP
- ACTIVATED         - DEACTIVATED
- START
```

### Response Messages
- **"VACUUM_ACTIVATED"** - Sent when vacuum turns on
- **"VACUUM_DEACTIVATED"** - Sent when vacuum turns off

## Testing with Mobile Apps

### nRF Connect (Recommended)
1. Install nRF Connect app
2. Scan for "Makita_Vacuum"  
3. Connect and explore services
4. Find characteristic 0xFF01
5. Write commands and read responses

### BLE Scanner
1. Scan for ESP32 device
2. Connect to GATT services
3. Send commands via characteristic write

## Troubleshooting Real BLE

### Build Errors
```bash
# Clean and reconfigure
idf.py fullclean
idf.py menuconfig  # Enable BLE components
idf.py build
```

### Connection Issues
- Check device is advertising correctly
- Verify GATT service is registered
- Monitor serial output for BLE events
- Ensure mobile device BLE is enabled

### Memory Issues
- Reduce log levels in menuconfig
- Disable unused components (WiFi, etc.)
- Use `esp_bt_controller_mem_release()` for unused modes

## Reverting to Simulation

If you encounter issues with real BLE:

```bash
cd /home/rok/src/Makuum/main
mv bt_manager.c bt_manager_ble_backup.c  
mv bt_manager_simulation.c bt_manager.c
idf.py build flash
```

## Integration with AWS/Makita Tools

For production use:
1. **Pair with AWS device** using standard BLE pairing
2. **Configure tool communication** via GATT characteristics  
3. **Implement security** (encryption, authentication) as needed
4. **Add error handling** for connection drops and retries
5. **Optimize power consumption** with connection intervals

## Next Steps

1. **Start with simulation** - Verify everything works
2. **Enable real BLE** when you need device communication
3. **Test thoroughly** with mobile apps first
4. **Integrate with tools** after BLE communication is stable

The simulation mode provides a solid foundation for development, and real BLE can be added when hardware communication is required.