#!/bin/bash

# Quick build script that ensures ESP-IDF is sourced
set -e

PROJECT_DIR="/home/rok/src/Makuum"

echo "🔧 Quick Build - Makita Vacuum BLE 🔧"
echo "=================================="

cd "$PROJECT_DIR"

# Check if ESP-IDF is already sourced
if [ -z "$IDF_PATH" ]; then
    echo "📁 Sourcing ESP-IDF from /home/rok/esp/v5.3.4/esp-idf..."
    export IDF_PATH="/home/rok/esp/v5.3.4/esp-idf"
    source "$IDF_PATH/export.sh"
fi

echo "✅ ESP-IDF Path: $IDF_PATH"
echo "📁 Working directory: $(pwd)"

echo ""
echo "🔨 Building project..."
idf.py build

echo ""
echo "✅ Build complete!"
echo ""
echo "To flash: idf.py -p /dev/ttyUSB0 flash monitor"