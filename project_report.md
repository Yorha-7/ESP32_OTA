# Project Report: ESP32-WROOM OTA Programming Hub

## 1. Project Scope

### Objective
Give ESP32-WROOM nodes the ability to receive firmware updates over Wi-Fi (OTA) from a Raspberry Pi hub. User will code their own sensor application - we're building the OTA infrastructure.

### Hardware
- **Hub:** Raspberry Pi (existing)
- **Node:** ESP32-WROOM module
- **Count:** 1 node for testing, up to 5-6 total

### Scope (What We're Building)
| Component | What It Does |
|-----------|-------------|
| ESP32 Node Base | Zephyr + Wi-Fi (Station) + OTA client |
| Pi Hub | Web server + firmware storage + push mechanism |

### Out of Scope
- Sensor driver code (user codes this)
- Cloud integration (local network only)

---

## 2. Architecture

```
┌─────────────────────────────────────────────────────────────┐
│              Raspberry Pi (Hub)                       │
│                                                     │
│   ┌─────────────────────────────────────────────┐   │
│   │           Web Dashboard                      │   │
│   │  - Node status list                         │   │
│   │  - Firmware upload (.bin)                  │   │
│   │  - Trigger update                         │   │
│   └─────────────────────────────────────────────┘   │
│                       │                            │
│   ┌─────────────────────────────────────────────┐   │
│   │           HTTP Server                       │   │
│   │  GET /api/nodes      - node registration   │   │
│   │  GET /firmware/:id  - download firmware │   │
│   │  POST /api/update  - trigger update     │   │
│   └─────────────────────────────────────────────┘   │
└───────────────────────┬─────────────────────────────┘
                        │ Wi-Fi (local network)
                        │ HTTP
        ┌───────────────┼───────────────┐
        │               │               │
   ┌────▼────┐   ┌────▼────┐   ┌───┴────┐
   │ Node 1  │   │ Node 2  │   │ Node N │
   │         │   │         │   │       │
   │  Wi-Fi  │   │  Wi-Fi  │   │ Wi-Fi │
   │ Station │   │ Station │   │Station│
   └────────┘   └────────┘   └───────┘
```

### Update Flow
1. Node boots → connects Wi-Fi → registers with Hub (GET /api/nodes)
2. User → opens Hub web UI → uploads .bin file
3. User → selects node → clicks "Update"
4. Hub → stores firmware, tells Node to update
5. Node → downloads firmware from Hub (GET /firmware/:id)
6. Node → writes to flash → reboots
7. Node → on new firmware

---

## 3. OTA Approach

**Custom HTTP-based OTA** - simple and works with Zephyr.

### Why Custom?
- Simpler than Mender (no external server needed)
- Full control
- Works locally without internet

### Requirements
- MCUboot bootloader for A/B partition updates
- HTTP client on ESP32 to download firmware
- Simple REST API on Hub

---

## 4. Implementation Plan

### Phase 1: ESP32 Base (Zephyr + Wi-Fi)
| Task | Description |
|------|-------------|
| 1.1 | Set up Zephyr build env for ESP32-WROOM |
| 1.2 | Configure Wi-Fi station mode |
| 1.3 | Add networking (HTTP client) |
| 1.4 | Build and test connection to Hub |

### Phase 2: OTA Client
| Task | Description |
|------|-------------|
| 2.1 | Enable MCUboot bootloader |
| 2.2 | Configure flash partitions (A/B) |
| 2.3 | Implement HTTP download |
| 2.4 | Implement flash write + reboot |
| 2.5 | Test full update flow |

### Phase 3: Hub Server
| Task | Description |
|------|-------------|
| 3.1 | Python Flask server |
| 3.2 | Node registration endpoint |
| 3.3 | Firmware upload endpoint |
| 3.4 | GET /firmware/:id endpoint |
| 3.5 | Web dashboard HTML |

### Phase 4: Integration
| Task | Description |
|------|-------------|
| 4.1 | Node connects to Hub automatically |
| 4.2 | User uploads .bin via web UI |
| 4.3 | Trigger update to node |
| 4.4 | Verify reboot with new firmware |
| 4.5 | Test with 2+ nodes |

---

## 5. File Structure

### ESP32 Node (Zephyr App)
```
esp32_ota_node/
├── prj.conf              # Kconfig (Wi-Fi, networking, MCUboot)
├── esp32.overlay         # Flash partition map
├── src/
│   ├── main.c           # App entry + Wi-Fi connect
│   ├── ota_client.c     # OTA download logic
│   └── ota_client.h
└── CMakeLists.txt
```

### Hub (Raspberry Pi)
```
pi_ota_hub/
├── server.py             # Flask server
├── firmware/           # Store .bin files
│   └── .gitkeep
├── templates/
│   └── index.html     # Dashboard UI
├── models.py          # Node database
├── requirements.txt  # Python deps
└── README.md
```

---

## 6. Hardware Connections

### ESP32-WROOM Pinout (for reference)
| Pin | Function |
|-----|----------|
| 3V3 | VCC |
| GND | GND |
| GPIO21 | I2C SDA (sensor) |
| GPIO22 | I2C SCL (sensor) |
| GPIO1 | UART TX (debug) |
| GPIO3 | UART RX (debug) |

### Initial Test (Wi-Fi only, no sensor)
- Just need: 3V3, GND, Wi-Fi built-in
- Sensor added later by user

---

## 7. Dependencies

### ESP32 Node
- Zephyr RTOS
- ESP32 Wi-Fi driver
- MCUboot (Zephyr downstream)
- HTTP client (builtins)

### Hub
- Python 3
- Flask
- SQLite (built-in)

---

## 8. Configuration

### Wi-Fi
- Network: Hub's local Wi-Fi (2.4GHz)
- ESP32 connects as station

### Addresses
- Hub: `http://raspberrypi.local:5000` (or IP)
- Node IP: Assigned by DHCP

---

## 9. API Endpoints

### Node → Hub
| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/nodes` | Register node, get pending update |
| GET | `/firmware/<id>` | Download firmware binary |
| POST | `/api/status` | Report node status |

### Hub → Node
| Endpoint | Description |
|----------|-------------|
| `/api/nodes` response | Contains `update_available` flag + firmware URL |

---

## 10. Next Steps

| Step | Task | Status |
|------|------|--------|
| 1 | Set up Zephyr build env | Pending |
| 2 | Build ESP32 base with Wi-Fi | Pending |
| 3 | Implement OTA client | Pending |
| 4 | Build Hub server | Pending |
| 5 | Integration test | Pending |

---

*Report generated: April 2026*
*Project: ESP32-WROOM OTA Programming Hub*