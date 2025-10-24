#!/bin/bash

# BLE Test Script for Makita Vacuum
# Tests BLE communication using bluetoothctl or similar tools

echo "🔵 Makita Vacuum BLE Test 🔵"
echo "=========================="

echo "This script helps test BLE communication with your Makita Vacuum ESP32"
echo ""

# Check if tools are available
if command -v bluetoothctl &> /dev/null; then
    echo "✅ bluetoothctl found"
    BT_TOOL="bluetoothctl"
elif command -v hcitool &> /dev/null; then
    echo "✅ hcitool found"
    BT_TOOL="hcitool"
else
    echo "❌ No Bluetooth tools found. Install bluez-utils:"
    echo "   sudo apt install bluez-utils"
    exit 1
fi

echo ""
echo "Instructions for testing:"
echo "1. Flash and run your ESP32 with the Makita Vacuum code"
echo "2. The device should advertise as 'Makita_Vacuum'"
echo "3. Use a BLE app or the commands below to connect"
echo ""

if [ "$BT_TOOL" = "bluetoothctl" ]; then
    echo "Manual BLE scanning with bluetoothctl:"
    echo "  sudo bluetoothctl"
    echo "  power on"
    echo "  scan on"
    echo "  # Look for 'Makita_Vacuum' in the scan results"
    echo "  connect XX:XX:XX:XX:XX:XX  # Replace with your ESP32's MAC"
    echo ""
    echo "To send commands, use a BLE app like:"
    echo "- nRF Connect (mobile)"
    echo "- BLE Scanner (mobile)"  
    echo "- gatttool (command line)"
fi

echo ""
echo "Test commands to send via BLE write:"
echo "- POWER_ON      (activates vacuum)"
echo "- MAKITA_POWER_ON"
echo "- TOOL_ACTIVATED"
echo "- POWER_OFF     (deactivates vacuum)"
echo "- STOP"
echo ""

echo "Expected behavior:"
echo "📍 ESP32 LED off → No BLE connection"
echo "📍 ESP32 LED slow blink → BLE connected, standby"  
echo "📍 ESP32 LED solid on → Vacuum activated"
echo ""

echo "Service UUID: 00FF"
echo "Characteristic UUID: FF01"
echo "Device name: Makita_Vacuum"