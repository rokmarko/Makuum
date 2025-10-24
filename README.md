# Makita Vacuum Cleaner ESP32 Project

🔧 **ESP32-based Makita Vacuum Cleaner Controller** 🔧

This project transforms an ESP32-WROOM module into a smart vacuum cleaner controller that responds to Bluetooth signals from AWS-capable devices or Makita tools.

## Features

- 🔵 **BLE Simulation Mode** (ready for real BLE upgrade - see BLE_IMPLEMENTATION.md)
- 💡 **LED status indicator** with multiple patterns (off, on, slow blink, fast blink, pulse)
- 🔌 **Tool power detection** simulation and real BLE communication support
- 🌪️ **Automatic vacuum activation** when tool power is detected
- ⏰ **Configurable timeout** for vacuum operation
- � **Ready-to-test** without complex BLE configuration

## Hardware Requirements

- ESP32-WROOM development board
- LED (built-in LED on GPIO2 or external LED)
- USB cable for programming
- AWS-capable device or Makita tool with Bluetooth capability

## Pin Configuration

| Component | GPIO | Notes |
|-----------|------|-------|
| Status LED | GPIO2 | Configurable via menuconfig |

## Current Mode: BLE Simulation

🚀 **This project is currently in simulation mode** for easy testing and development!

### Why Simulation Mode?
- ✅ **Instant testing** - Build and flash without BLE configuration hassles
- ✅ **Complete functionality** - Test all vacuum logic and LED patterns
- ✅ **No dependencies** - Works out of the box with ESP-IDF v5.5.1
- ✅ **Perfect for learning** - Understand the project before adding complexity

### Simulation Behavior
- **10 seconds** - Simulates BLE device connection (LED starts blinking)
- **30 seconds** - Simulates "MAKITA_POWER_ON" command (LED solid on)
- **60 seconds** - Simulates power off and cycle repeats

### Ready for Real BLE?
When you need actual device communication, see **[BLE_IMPLEMENTATION.md](BLE_IMPLEMENTATION.md)** for the complete upgrade guide!

## Project Structure

```
├── main/
│   ├── main.c                    # Main application logic and state machine
│   ├── bt_manager.c/.h           # BLE simulation (real BLE ready)
│   ├── bt_manager_complex_ble.c  # Real BLE implementation (ready to use)
│   ├── led_control.c/.h          # LED control and pattern management
│   └── CMakeLists.txt            # Component build configuration
├── CMakeLists.txt                # Main project build configuration  
├── sdkconfig.defaults            # Default ESP-IDF configuration
├── BLE_IMPLEMENTATION.md         # Guide for real BLE upgrade
└── README.md                     # This file
```

## Software Features

### State Machine
The vacuum operates in three states:
1. **IDLE** - Waiting for Bluetooth connection
2. **STANDBY** - Connected, waiting for tool power signal
3. **ACTIVE** - Vacuum running (tool detected)

### LED Patterns
- **OFF** - No Bluetooth connection
- **SLOW BLINK** - Connected, standby mode
- **SOLID ON** - Vacuum active

### Bluetooth Communication
- Listens for keywords: `MAKITA_POWER_ON`, `TOOL_ACTIVATED`, `POWER_ON`, `POWER`, `ON`, `ACTIVATED`, `START`
- Responds with acknowledgment messages
- Supports device discovery for AWS and Makita devices

## Setup Instructions

### Prerequisites

1. **Install ESP-IDF**: Follow the [ESP-IDF Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/)
2. **Set up development environment**:
   ```bash
   # Install ESP-IDF (if not already installed)
   git clone --recursive https://github.com/espressif/esp-idf.git
   cd esp-idf
   ./install.sh
   . ./export.sh
   ```

### Building and Flashing

1. **Clone and navigate to project**:
   ```bash
   cd /home/rok/src/Makuum
   ```

2. **Configure the project** (optional):
   ```bash
   idf.py menuconfig
   ```
   Navigate to "Makita Vacuum Configuration" to customize:
   - LED GPIO pin
   - Bluetooth device name
   - Vacuum activation timeout
   - Debug mode

3. **Build the project**:
   ```bash
   idf.py build
   ```

4. **Flash to ESP32**:
   ```bash
   idf.py -p /dev/ttyUSB0 flash
   ```
   Replace `/dev/ttyUSB0` with your ESP32's port.

5. **Monitor output**:
   ```bash
   idf.py -p /dev/ttyUSB0 monitor
   ```

### Quick Start Commands

```bash
# Full build and flash
idf.py build flash monitor

# Clean build
idf.py fullclean build flash monitor

# Just monitor (after flashing)
idf.py monitor
```

## Configuration Options

Access via `idf.py menuconfig` → "Makita Vacuum Configuration":

- **LED_GPIO**: GPIO pin for status LED (default: 2)
- **BT_DEVICE_NAME**: Bluetooth device name (default: "Makita_Vacuum_Cleaner")
- **VACUUM_ACTIVATION_TIMEOUT**: Auto-off timeout in seconds (default: 30)
- **DEBUG_MODE**: Enable verbose logging (default: enabled)

## Usage

### Basic Operation

1. **Power on** the ESP32
2. **LED will be off** - waiting for BLE connection
3. **Connect from BLE device** (appears as "Makita_Vacuum")
4. **LED starts slow blinking** - ready for tool signals
5. **Write power command** to BLE characteristic (keywords listed above)
6. **LED turns solid on** - vacuum activated!
7. **Vacuum auto-deactivates** after timeout or power-off command

### BLE Communication

**Service UUID**: `00FF` (or use standard Nordic UART Service)
**Characteristic UUID**: `FF01`

Send these strings via BLE write to control the vacuum:

**Activation commands:**
- `MAKITA_POWER_ON`
- `TOOL_ACTIVATED` 
- `POWER_ON`
- `POWER`
- `ON`
- `ACTIVATED`
- `START`

**Deactivation commands:**
- `POWER_OFF`
- `OFF`
- `STOP`
- `DEACTIVATED`

### Troubleshooting

**LED not working:**
- Check GPIO configuration in menuconfig
- Verify LED connection and polarity
- Check serial output for GPIO initialization errors

**Bluetooth not connecting:**
- Ensure Bluetooth Classic is enabled in sdkconfig
- Check device compatibility (Bluetooth Classic, not BLE)
- Monitor serial output for Bluetooth initialization errors
- Try restarting both devices

**Build errors:**
- Ensure ESP-IDF is properly installed and sourced
- Try `idf.py fullclean` before building
- Check ESP-IDF version compatibility

**No response to commands:**
- Verify command format matches expected keywords
- Check Bluetooth connection status in serial output
- Ensure SPP (Serial Port Profile) connection is established

## Development

### Adding New Features

1. **New LED patterns**: Modify `led_control.c` and add to `led_pattern_t` enum
2. **Custom Bluetooth commands**: Update `parse_received_data()` in `bt_manager.c`
3. **Additional sensors**: Add new component in `components/` directory
4. **Configuration options**: Add to `Kconfig.projbuild`

### Serial Output Example

```
I (1234) MAKITA_VACUUM: 🔧 Makita Vacuum Cleaner Starting... 🔧
I (1245) LED_CONTROL: Initializing LED control on GPIO 2
I (1256) BT_MANAGER: Initializing Bluetooth Classic...
I (1267) MAKITA_VACUUM: ✅ Makita Vacuum Cleaner Ready! Waiting for Bluetooth connection...
I (1278) BT_MANAGER: Bluetooth discovery started. Device is discoverable as 'Makita_Vacuum_Cleaner'
I (5000) MAKITA_VACUUM: Status - State: STANDBY, BT: Connected
I (6234) BT_MANAGER: 🔌 Tool power ON detected! Activating vacuum...
I (6245) MAKITA_VACUUM: Tool power detected - ACTIVATING vacuum!
I (6256) MAKITA_VACUUM: 🌪️  VACUUM CLEANER ACTIVATED! 🌪️
```

## License

This project is open source. Feel free to modify and distribute according to your needs.

## Support

For issues and questions:
1. Check the serial monitor output for error messages
2. Verify hardware connections
3. Ensure ESP-IDF version compatibility
4. Check Bluetooth device compatibility

---

**Happy vacuuming! 🌪️🔧**