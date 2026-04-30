#ifndef OTA_H
#define OTA_H

#include <zephyr/kernel.h>

// OTA Server Configuration
#define OTA_SERVER_HOST     "raspberrypi.local"
#define OTA_SERVER_PORT     5000
#define OTA_POLL_INTERVAL  300000  // 5 minutes in ms

// Node Configuration
#define NODE_ID            "node-001"
#define FIRMWARE_VERSION   "1.0.0"

// Flash partitions (must match overlay)
#define SLOT0_BASE        0x10000
#define SLOT1_BASE        0x1B0000
#define SLOT_SIZE         0x1A0000

// HTTP buffers
#define HTTP_RESP_SIZE   4096
#define CHUNK_SIZE       1024

// OTA States
#define OTA_STATE_IDLE        0
#define OTA_STATE_CHECKING    1
#define OTA_STATE_DOWNLOADING  2
#define OTA_STATE_WRITING    3
#define OTA_STATE_REBOOTING  4
#define OTA_STATE_CONFIRMED  5

// Structures
struct ota_state {
    uint8_t state;
    uint8_t update_available;
    char firmware_url[128];
    char firmware_version[32];
    uint32_t firmware_size;
    uint32_t downloaded_size;
};

// Function prototypes
void ota_init(void);
void ota_poll_server(void);
int ota_check_for_update(void);
int ota_download_and_flash(void);
void ota_reboot_to_new_slot(void);
int ota_confirm_update(void);

// WiFi functions (from wifi.c)
void wifi_init(void);
int wifi_connect(char *ssid, char *psk);
void wifi_wait_for_ip_addr(void);

// HTTP client functions
int http_get(const char *host, const char *path, char *response, size_t resp_size);
int http_download(const char *host, const char *path, uint8_t *buffer, size_t size);

#endif  // OTA_H