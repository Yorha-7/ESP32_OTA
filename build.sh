#!/bin/bash

# Main Build Script
# Builds OTA node for ESP32 and starts Hub server

echo "=========================================="
echo "  ESP32 OTA Project - Build All"
echo "=========================================="

# Get the project root
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$PROJECT_ROOT"

# Check which action
ACTION="${1:-build}"

case "$ACTION" in
    build)
        echo ""
        echo "Building ESP32 OTA Node..."
        echo "=========================================="
        
        cd workspace
        
        if ! command -v west &> /dev/null; then
            echo "Error: west not found!"
            echo "Make sure you're in the Docker container."
            exit 1
        fi
        
        west build -b my_board/procpu apps/ota_node
        
        echo ""
        echo "=========================================="
        echo "  Build Complete!"
        echo "=========================================="
        echo ""
        echo "Firmware: build/zephyr/app.bin"
        echo "To flash: west flash"
        ;;
        
    hub)
        echo ""
        echo "Starting OTA Hub Server..."
        echo "=========================================="
        
        cd hub
        
        # Install Flask if needed
        if ! python3 -c "import flask" 2>/dev/null; then
            echo "Installing Flask..."
            pip install -r requirements.txt
        fi
        
        echo ""
        echo "Server running at http://raspberrypi.local:5000"
        echo ""
        
        python3 server.py
        
        ;;
        
    flash)
        echo "Flashing ESP32..."
        west flash
        
        ;;
        
    *)
        echo "Usage: $0 {build|hub|flash}"
        echo ""
        echo "  build  - Build ESP32 OTA node firmware"
        echo "  hub    - Start OTA Hub server"
        echo "  flash - Flash firmware to ESP32"
        exit 1
        ;;
esac