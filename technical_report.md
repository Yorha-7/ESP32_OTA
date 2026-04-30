# Technical Report: ESP32-WROOM OTA Architecture

## 1. ESP32 Flash Memory Layout

### Flash Memory Map (4MB ESP32-WROOM)

```
┌─────────────────────────────────────────────────────────────────┐
│                    ESP32 FLASH (4MB)                         │
├────────────┬────────────┬────────────┬────────────┬────────────┤
│ Bootloader │  NVS      │  OTA      │  OTA      │  Scratch │
│           │          │  Data     │  Slot 0   │         │
│  Custom   │  Config  │  State    │ (Active)  │         │
│  Boot    │  Storage │          │  Firmware │         │
│  Sector  │          │          │  v1.0     │         │
│           │          │          ├───────────┤          │
│           │          │          │  Slot 1   │          │
│           │          │          │ (inactive)│          │
│           │          │          │  v2.0     │          │
│           │          │          │ (update)  │          │
└───────────┴──────────┴───────────┴───────────┴──────────┘
```

| Partition | Start Address | Size | Description |
|-----------|-------------|------|-------------|
| Bootloader | 0x1000 | 0xF000 | First stage bootloader |
| Partition Table | 0x8000 | 0x1000 | Partition definitions |
| NVS | 0x9000 | 0x6000 | Non-volatile storage |
| OTA_0 | 0x10000 | 0x1A0000 | Active firmware slot |
| OTA_1 | 0x1B0000 | 0x1A0000 | Inactive firmware slot |
| OTA Data | 0x350000 | 0x1000 | OTA state metadata |

---

## 2. Partition Definitions

### Partition Table Structure
```c
// In esp32.overlay or partitions.csv
# Name,   Type, SubType, Offset,   Size,     Flags
nvs,     data, nvs,     0x9000,  0x6000,
otadata, data, ota,     0xF000,  0x1000,
app0,    app,  ota_0,   0x10000, 0x1A0000,
app1,    app,  ota_1,   0x1B0000, 0x1A0000,
```

### Device Tree Overlay (Zephyr)
```dts
&flash0 {
    partitions {
        compatible = "fixed-partitions";
        #address-cells = <1>;
        #size-cells = <1>;

        slot0_partition: partition@10000 {
            label = "app_partition_0";
            reg = <0x10000 0x1A0000>;
        };

        slot1_partition: partition@1B0000 {
            label = "app_partition_1";
            reg = <0x1B0000 0x1A0000>;
        };

        otadata_partition: partition@350000 {
            label = "otadata_partition";
            reg = <0x350000 0x1000>;
        };
    };
};
```

---

## 3. MCUboot Integration

### Kconfig Options
```
# prj.conf - MCUboot configuration
CONFIG_BOOTLOADER_MCUBOOT=y
CONFIG_MCUBOOT_FLASH_LAYOUT_DEFAULT=y
CONFIG_MCUBOOT_SIGNATURE_KEY_FILE="root-rsa-2048.pem"
CONFIG_MCUBOOT_VALIDATE_SLOT0=y
CONFIG_MCUBOOT_PREFER_RAM_LOAD=y
```

### Build with MCUboot
```bash
# Build command
west build -b esp32_devkitc_wroom/esp32/procpu --sysbuild your_app

# This produces:
# - build/zephyr/bootloader.bin    (MCUboot)
# - build/zephyr/app.bin        (Your app)
```

---

## 4. OTA State Machine

### OTA Data Structure
```c
// Stored in OTA Data partition
struct ota_state {
    uint32_t magic;           // Magic number (0x12345678)
    uint8_t active_slot;     // 0 = slot0, 1 = slot1
    uint8_t pending_slot;    // 0 = none, 1 = slot1 pending
    uint8_t confirmed;      // 1 = confirmed working
    uint32_t version[2];   // Firmware version in each slot
};
```

### State Transitions
```
┌─────────┐     download     ┌─────────┐
│  Idle   │ ──────────────► │Downloading│
└─────────┘                └────┬────┘
     ▲                          │
     │                    write complete
     │                          ▼
┌─────────┐              ┌─────────┐
│  Boot  │ ◄──────────── │ Pending │
│  OK    │               │ Reboot  │
└─────────┘              └─────────┘
     ▲                         │
     │                    boot failure
     │                         │
     └─────────────────────────┘
                         (rollback)
```

---

## 5. HTTP Download Protocol

### Network Connection
```
┌─────────────────────────────────────────────────────────────┐
│                    ESP32 Wi-Fi Station                     │
│                  (Connects to local network)              │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  TCP Socket ───────────── HTTP Request                       │
│                                                             │
│  ┌─────────────────────────────────────────────────────┐   │
│  │ GET /firmware/v2.0.bin HTTP/1.1\r\n               │   │
│  │ Host: 192.168.1.100\r\n                          │   │
│  │ Range: bytes=0-\r\n                              │   │
│  │ \r\n                                             │   │
│  └─────────────────────────────────────────────────────┘   │
│                                                             │
│  ◄────────────── HTTP Response ───────────────           │
│                                                             │
│  ┌─────────────────────────────────────────────────────┐   │
│  │ HTTP/1.1 200 OK\r\n                                │   │
│  │ Content-Type: application/octet-stream\r\n        │   │
│  │ Content-Length: 1048576\r\n                       │   │
│  │ \r\n                                             │   │
│  │ <binary firmware data...>                          │   │
│  └─────────────────────────────────────────────────────┘   │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### Download Code (Simplified)
```c
// Pseudo-code for OTA download
int ota_download(const char *url) {
    // 1. Parse URL
    struct http_parser url;
    http_parse_url(url, "http://192.168.1.100/firmware/v2.0.bin");

    // 2. Connect TCP socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    connect(sock, hub_ip, hub_port);

    // 3. Send HTTP GET
    char request[256];
    snprintf(request, sizeof(request),
        "GET /firmware/v2.0.bin HTTP/1.1\r\n"
        "Host: %s\r\n"
        "\r\n", hub_ip);
    send(sock, request, strlen(request), 0);

    // 4. Receive response headers
    recv(sock, buffer, 1024, 0);
    // Parse Content-Length

    // 5. Download and write to flash
    int offset = 0;
    while (offset < firmware_size) {
        int chunk = recv(sock, buffer, CHUNK_SIZE, 0);
        // Write to inactive slot (slot 1)
        flash_write(SLOT1_BASE + offset, buffer, chunk);
        offset += chunk;
    }

    // 6. Verify checksum
    calculate_md5(SLOT1_BASE, firmware_size);
    compare_with_expected(checksum);

    return 0;  // Success
}
```

---

## 6. Flash Write Process

### Writing to Flash
```c
// Flash write parameters
#define SLOT1_BASE      0x1B0000
#define FLASH_PAGE_SIZE 4096
#define CHUNK_SIZE     4096

int write_firmware_chunk(uint32_t offset, const uint8_t *data, size_t len) {
    // Unlock flash
    esp_flash_unlock();

    // Erase sector (must erase before write)
    esp_flash_erase_sector(SLOT1_BASE + offset);

    // Write data
    esp_flash_write(SLOT1_BASE + offset, data, len);

    // Lock flash
    esp_flash_lock();

    return 0;
}
```

### Flash Timing
| Operation | Time |
|-----------|------|
| Erase 4KB sector | ~30ms |
| Write 4KB | ~50ms |
| Full 1MB erase | ~7.5s |
| Full 1MB write | ~12s |

---

## 7. Boot Process

### Power-On Sequence
```
┌─────────────────────────────────────────────────────────────┐
│                                                          │
│  1. ROM Bootloader (from mask ROM)                         │
│     │                                                     │
│     ▼                                                     │
│  2. Load MCUboot from flash (0x1000)                    │
│     │                                                     │
│     ▼                                                     │
│  3. MCUboot reads OTA data (0x350000)                   │
│     │                                                     │
│     ▼                                                     │
│  4. Validate active slot firmware                       │
│     │                                                     │
│     ├─ If valid: boot into slot                         │
│     │                                                     │
│     └─ If invalid: try other slot                      │
│                                                          │
│  5. Jump to firmware entry point                       │
│                                                          │
└─────────────────────────────────────────────────────────────┘
```

### Boot Decision Logic (MCUboot)
```c
// Pseudo-code from MCUboot
int boot_selector(void) {
    // Read OTA state
    struct ota_state state;
    flash_read(OTA_DATA_ADDR, &state, sizeof(state));

    // Check pending update
    if (state.pending_slot != 0) {
        // Try booting into pending slot
        if (validate_firmware(state.pending_slot)) {
            // Boot into new firmware
            boot_firmware(state.pending_slot);
            return 0;
        } else {
            // Validation failed, rollback
            state.active_slot = (state.active_slot == 0) ? 1 : 0;
            state.pending_slot = 0;
            flash_write(OTA_DATA_ADDR, &state, sizeof(state));
            boot_firmware(state.active_slot);
            return 0;
        }
    }

    // No pending update, boot current
    if (validate_firmware(state.active_slot)) {
        boot_firmware(state.active_slot);
        return 0;
    }

    // Both slots failed, panic
    PANIC("No valid firmware to boot!");
}
```

---

## 8. API Protocol

### Node → Hub Endpoints

#### Register/Check for Update
```
┌─────────────────────────────────────────────────────────┐
│ Request: GET /api/nodes?id=node-001&version=1.0       │
├─────────────────────────────────────────────────────────┤
│ Response (no update):                                  │
│ {                                                    │
│   "id": "node-001",                                  │
│   "update_available": false,                        │
│   "current_version": "1.0"                          │
│ }                                                    │
│                                                       │
│ Response (update available):                        │
│ {                                                    │
│   "id": "node-001",                                  │
│   "update_available": true,                          │
│   "firmware_url": "/firmware/v2.0.bin",              │
│   "firmware_version": "2.0",                        │
│   "firmware_size": 1048576                           │
│ }                                                    │
└─────────────────────────────────────────────────────────┘
```

#### Download Firmware
```
┌───────────��─────────────────────────────────────────────┐
│ Request: GET /firmware/v2.0.bin                       │
├─────────────────────────────────────────────────────────┤
│ Response:                                            │
│ HTTP/1.1 200 OK                                       │
│ Content-Type: application/octet-stream               │
│ Content-Length: 1048576                               │
│                                                       │
│ [binary firmware data...]                            │
└─────────────────────────────────────────────────────────┘
```

#### Report Status
```
┌─────────────────────────────────────────────────────────┐
│ Request: POST /api/status                            │
│ {                                                    │
│   "id": "node-001",                                  │
│   "version": "2.0",                                 │
│   "status": "running",                              │
│   "uptime_seconds": 300                             │
│ }                                                    │
├─────────────────────────────────────────────────────────┤
│ Response:                                            │
│ {                                                    │
│   "ok": true                                         │
│ }                                                    │
└─────────────────────────────────────────────────────────┘
```

---

## 9. Complete Update Flow

### Timeline
| Time | Event | Component |
|------|-------|------------|
| T0 | Node starts, connects Wi-Fi | ESP32 |
| T0+1s | Poll Hub for update | App |
| T0+1s | Response: no update | Hub |
| T0+2s | Start sensor code | App |
| T+5min | User uploads v2.0 via web UI | Hub |
| T+5min+1s | Node polls again | App |
| T+5min+1s | Response: update available | Hub |
| T+5min+2s | Download firmware to Slot 1 | App |
| T+5min+14s | Write complete | App |
| T+5min+15s | Verify checksum | App |
| T+5min+15s | Report "downloaded" to Hub | App |
| T+5min+16s | Set pending, reboot | App |
| T+5min+16s+1s | MCUboot activates Slot 1 | MCUboot |
| T+5min+16s+2s | Boot v2.0 | App |
| T+5min+16s+5s | v2.0 running successfully | App |
| T+5min+21s | Confirm update to Hub | App |
| T+5min+21s+1s | Hub marks v2.0 as active | Hub |

---

## 10. Error Handling

### Error Cases and Recovery
| Error | Detection | Recovery |
|-------|-----------|----------|
| Download timeout | TCP timeout | Retry download |
| Checksum mismatch | MD5 invalid | Discard, stay on old |
| Flash write fail | Write error | Retry erase+write |
| Boot failure | Panic/crash | Auto-rollback to old |
| Wi-Fi disconnect | Network error | Reconnect, retry |

---

## 11. Key Source Files

### ESP32 Node (Zephyr)
```
esp32_ota_node/
├── prj.conf              # Kconfig
├── esp32.overlay        # Partition overlay
├── src/
│   ├── main.c           # App entry + Wi-Fi
│   ├── network.c        # HTTP client
│   ├── ota.c           # OTA logic
│   └── ota.h
└── CMakeLists.txt
```

### Hub (Python)
```
pi_ota_hub/
├── server.py            # Flask app
├── firmware/           # .bin storage
├── templates/
│   └── index.html     # Web UI
└── requirements.txt
```

---

## 12. Summary

| Component | Responsibility |
|-----------|----------------|
| Slot 0 | Current running firmware |
| Slot 1 | New firmware download target |
| MCUboot | Decides which slot to boot |
| OTA Data | Tracks active/pending/confirmation |
| HTTP Client | Downloads new firmware |
| Flash Write | Writes to inactive slot |
| Hub Server | Stores firmware, responds to polls |

---

*Technical Report generated: April 2026*
*Project: ESP32-WROOM OTA Programming Hub*