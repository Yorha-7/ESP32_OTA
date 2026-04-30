#!/bin/bash

# ESP32 OTA Node Build Script
# Run this inside the Docker container

echo "=========================================="
echo "  ESP32 OTA Node Build Script"
echo "=========================================="

# Change to workspace directory
cd /home/pi/TEST/workspace

# Build OTA node for my_board
echo ""
echo "Building OTA node for ESP32-WROOM..."
echo ""

west build -b my_board/procpu apps/ota_node

echo ""
echo "=========================================="
echo "  Build Complete!"
echo "=========================================="
echo ""
echo "Firmware location: build/zephyr/app.bin"
echo ""
echo "To flash: west flash"
echo ""