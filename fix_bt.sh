#!/bin/bash

# Makita Vacuum ESP32 Fix Script
# Fixes Bluetooth initialization issues

set -e

PROJECT_DIR="/home/rok/src/Makuum"
PORT="/dev/ttyUSB0"

echo "🔧 Fixing Makita Vacuum ESP32 Bluetooth Issues 🔧"
echo "================================================="

cd "$PROJECT_DIR"

# Check if ESP-IDF is sourced
if [ -z "$IDF_PATH" ]; then
    echo "❌ ESP-IDF not found. Please run: . \$HOME/esp/esp-idf/export.sh"
    exit 1
fi

echo "📁 Working in: $(pwd)"
echo "🔍 ESP-IDF Version: $(idf.py --version)"

# Clean everything
echo "🧹 Cleaning project completely..."
idf.py fullclean

# Remove any existing sdkconfig to force regeneration
echo "🗑️  Removing old sdkconfig..."
rm -f sdkconfig sdkconfig.old

# Rebuild with fresh configuration
echo "🔨 Building with fresh configuration..."
idf.py build

echo ""
echo "✅ Build complete! Ready to flash."
echo ""
echo "To flash and monitor:"
echo "  idf.py -p $PORT flash monitor"
echo ""
echo "To just flash:"
echo "  idf.py -p $PORT flash"
echo ""
echo "Changes made:"
echo "- Switched to BLE-only mode (removed Classic Bluetooth)"
echo "- Released Classic BT memory to save space"
echo "- Implemented GATT server for tool communication"
echo "- Removed SPP and Classic BT dependencies"
echo "- Fresh sdkconfig generation for BLE"