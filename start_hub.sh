#!/bin/bash

# OTA Hub Server Start Script
# Run this on the Raspberry Pi

echo "=========================================="
echo "  OTA Hub Server"
echo "=========================================="

# Check if we're in the right directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
HUB_DIR="$SCRIPT_DIR/hub"

if [ ! -f "$HUB_DIR/server.py" ]; then
    echo "Error: hub/server.py not found!"
    echo "Please run this script from the project root."
    exit 1
fi

# Check if Flask is installed
if ! python3 -c "import flask" 2>/dev/null; then
    echo "Installing Flask..."
    pip install -r "$HUB_DIR/requirements.txt"
fi

# Start the server
echo ""
echo "Starting OTA Hub Server..."
echo "Access at: http://raspberrypi.local:5000"
echo ""

cd "$HUB_DIR"
python3 server.py