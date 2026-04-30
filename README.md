# ESP32 OTA Programming Hub

A system to update ESP32-WROOM firmware over Wi-Fi from a Raspberry Pi hub.

## Architecture

```
┌──────────────┐    Wi-Fi     ┌──────────────┐
│  Raspberry  │◄───────────►│   ESP32     │
│    Pi      │             │  WROOM     │
│  (Hub)    │             │  (Node)    │
│           │             │           │
│ - Web UI  │   OTA      │ - OTA     │
│ - Server │  Updates   │ - Sensor │
└──────────┘             └──────────┘
```

## Quick Start

### 1. Build Docker (First Time Only)

```bash
docker build -t env-zephyr-espressif -f Dockerfile.espressif .
```

### 2. Run Container

```bash
docker run --rm -it -v "$(pwd)"/workspace:/workspace -w /workspace env-zephyr-espressif
```

### 3. Build ESP32 OTA Node (Inside Container)

```bash
west build -b my_board/procpu apps/ota_node
```

### 4. Flash ESP32

```bash
west flash
```

### 5. Start Hub Server (On Raspberry Pi)

```bash
cd hub
pip install -r requirements.txt
python3 server.py
```

The server runs at `http://raspberrypi.local:5000`

## Using the Scripts

### Build & Flash (Inside Docker Container)

```bash
# Build ESP32 firmware
./build.sh build

# Flash to ESP32
./build.sh flash
```

### Start Hub (On Raspberry Pi)

```bash
# Start the hub server
./build.sh hub

# Or directly
./start_hub.sh
```

## File Structure

```
├── README.md                    # This file
├── build.sh                    # Main build script
├── workspace/
│   └── apps/
│       └── ota_node/          # ESP32 OTA Node app
│           ├── prj.conf      # Kconfig (WiFi, MCUboot)
│           ├── CMakeLists.txt
│           └── src/
│               ├── main.c   # Main app entry
│               ├── wifi.c  # WiFi connection
│               ├── wifi.h
│               ├── ota.c   # OTA client logic
│               └── ota.h
│
└── hub/                       # Raspberry Pi Hub
    ├── server.py              # Flask server
    ├── requirements.txt      # flask>=2.0
    ├── start_hub.sh         # Hub start script
    ├── templates/
    │   └── index.html     # Web dashboard
    └── firmware/           # .bin files stored here
```

## WiFi Configuration

Edit in `ota_node/src/main.c`:

```c
#define WIFI_SSID     "Zero"
#define WIFI_PSK     "abcdefgh"
```

## Hub Configuration

Edit in `ota_node/src/ota.h`:

```c
#define OTA_SERVER_HOST   "raspberrypi.local"
#define OTA_SERVER_PORT  5000
```

## Node ID

In `ota_node/src/ota.h`:

```c
#define NODE_ID        "node-001"
```

## How It Works

1. ESP32 connects to WiFi
2. Polls Hub every 5 minutes: `/api/nodes?id=node-001&version=1.0.0`
3. Hub responds with `{update_available: true/false}`
4. If update available, ESP32 downloads firmware
5. ESP32 writes to flash (Slot 1)
6. ESP32 reboots into new firmware
7. ESP32 confirms update to Hub

## Web Dashboard

Access at `http://raspberrypi.local:5000`

### Features

- View all connected nodes
- See firmware version
- Upload new firmware (.bin file)
- Schedule update for specific node
- View node status (online/offline)

## API Endpoints

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/` | GET | Web dashboard |
| `/api/nodes` | GET | Node checks for updates |
| `/api/confirm` | GET | Node confirms update |
| `/upload` | POST | Upload firmware |
| `/firmware/<id>.bin` | GET | Download firmware |

## Troubleshooting

### ESP32 not connecting

1. Check WiFi credentials in `ota_node/src/main.c`
2. Verify Hub is running
3. Check network connectivity

### Flash write fails

Make sure MCUboot is enabled in `ota_node/prj.conf`:

```
CONFIG_BOOTLOADER_MCUBOOT=y
```

### Build errors

Make sure you're inside the Docker container and ESP32 support is installed:

```bash
west manifest --resolve
west blobs fetch hal_espressif
```

## Build Output

After build, firmware is at:
```
build/zephyr/app.bin
```

Flash size varies based on your code (typically 200KB-1MB)

## License

IIT Dholakpur

## Problems

here are some problems that i listed by reading the ota code:

1. the OTA will update only when firmware updates from v0.1 to v0.2, it has no clue what comes after that, also it will not load older versions if we try that.
2. its using a meta data that will remain intact even after the new flash, that is used as pointer, thats too noob. but also i dont know myself how to do it better.
3. by now my room must have been a trash can (prototypes lying on floor) but its clean. yes that is a problem.
