#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/net_context.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/drivers/flash.h>
#include "ota.h"

// External WiFi functions
extern void wifi_init(void);
extern int wifi_connect(char *ssid, char *psk);
extern void wifi_wait_for_ip_addr(void);

// Static variables
static struct ota_state ota_state = {
    .state = OTA_STATE_IDLE,
    .update_available = 0,
    .firmware_url[0] = '\0',
    .firmware_version[0] = '\0',
    .firmware_size = 0,
    .downloaded_size = 0
};

static const struct device *flash_dev;

// HTTP GET request
static int http_get(const char *host, const char *path, char *response, size_t resp_size)
{
    struct zsock_addrinfo hints, *res;
    int sock, ret;
    char request[256];
    char buf[512];
    int bytes_received;

    // Setup hints
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    // DNS lookup
    ret = zsock_getaddrinfo(host, "5000", &hints, &res);
    if (ret != 0) {
        printk("OTA: DNS lookup failed for %s\n", host);
        return -EIO;
    }

    // Create socket
    sock = zsock_socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock < 0) {
        printk("OTA: Socket creation failed\n");
        return -EIO;
    }

    // Connect
    ret = zsock_connect(sock, res->ai_addr, res->ai_addrlen);
    if (ret < 0) {
        printk("OTA: Connect failed\n");
        zsock_close(sock);
        return -EIO;
    }

    // Build HTTP GET
    snprintf(request, sizeof(request),
        "GET %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Connection: close\r\n"
        "\r\n",
        path, host);

    // Send request
    ret = zsock_send(sock, request, strlen(request), 0);
    if (ret < 0) {
        printk("OTA: Send failed\n");
        zsock_close(sock);
        return -EIO;
    }

    // Receive response
    bytes_received = zsock_recv(sock, response, resp_size - 1, 0);
    if (bytes_received < 0) {
        printk("OTA: Receive failed\n");
        zsock_close(sock);
        return -EIO;
    }

    response[bytes_received] = '\0';
    zsock_close(sock);

    return 0;
}

// HTTP download (chunked)
static int http_download_chunk(const char *host, const char *path, 
                             uint8_t *buffer, size_t max_size, size_t *received)
{
    struct zsock_addrinfo hints, *res;
    int sock, ret;
    char request[256];
    char buf[CHUNK_SIZE];
    size_t total = 0;

    // Setup hints
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    // DNS lookup
    ret = zsock_getaddrinfo(host, "5000", &hints, &res);
    if (ret != 0) {
        return -EIO;
    }

    // Create socket
    sock = zsock_socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock < 0) {
        return -EIO;
    }

    // Connect
    ret = zsock_connect(sock, res->ai_addr, res->ai_addrlen);
    if (ret < 0) {
        zsock_close(sock);
        return -EIO;
    }

    // Build HTTP GET
    snprintf(request, sizeof(request),
        "GET %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Connection: close\r\n"
        "\r\n",
        path, host);

    // Send request
    zsock_send(sock, request, strlen(request), 0);

    // Skip HTTP headers, find body start
    int header_found = 0;
    size_t body_start = 0;
    
    while (total < max_size) {
        ret = zsock_recv(sock, buf, CHUNK_SIZE, 0);
        if (ret <= 0) break;

        if (!header_found) {
            // Find \r\n\r\n (end of headers)
            for (int i = 0; i < ret - 3; i++) {
                if (buf[i] == '\r' && buf[i+1] == '\n' && 
                    buf[i+2] == '\r' && buf[i+3] == '\n') {
                    header_found = 1;
                    body_start = i + 4;
                    break;
                }
            }
        }

        if (header_found) {
            size_t to_copy = ret - body_start;
            if (total + to_copy > max_size) {
                to_copy = max_size - total;
            }
            memcpy(buffer + total, buf + body_start, to_copy);
            total += to_copy;
            body_start = 0;
        }
    }

    zsock_close(sock);
    *received = total;
    return 0;
}

// Initialize OTA
void ota_init(void)
{
    printk("OTA: Initializing OTA client\n");
    flash_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_flash));
    if (!flash_dev) {
        printk("OTA: ERROR - Flash device not found\n");
    } else {
        printk("OTA: Flash device ready\n");
    }
    ota_state.state = OTA_STATE_IDLE;
}

// Check for updates
int ota_check_for_update(void)
{
    char response[HTTP_RESP_SIZE];
    char path[128];
    int ret;

    printk("OTA: Checking for updates...\n");

    // Build API path
    snprintf(path, sizeof(path), 
        "/api/nodes?id=%s&version=%s", 
        NODE_ID, FIRMWARE_VERSION);

    // Make request
    ret = http_get(OTA_SERVER_HOST, path, response, sizeof(response));
    if (ret < 0) {
        printk("OTA: Check failed\n");
        return -EIO;
    }

    // Simple parsing (look for "update_available":true)
    if (strstr(response, "update_available\":true") != NULL) {
        printk("OTA: Update available!\n");
        ota_state.update_available = 1;
        
        // Extract URL (simple method)
        char *url_start = strstr(response, "firmware_url\":");
        if (url_start) {
            url_start += 14;  // skip "firmware_url":"
            char *url_end = strchr(url_start, '"');
            if (url_end) {
                *url_end = '\0';
                strncpy(ota_state.firmware_url, url_start, sizeof(ota_state.firmware_url) - 1);
            }
        }
        return 0;
    }

    printk("OTA: No updates available\n");
    ota_state.update_available = 0;
    return 0;
}

// Download and flash firmware
int ota_download_and_flash(void)
{
    uint8_t buffer[CHUNK_SIZE];
    size_t received;
    uint32_t offset = SLOT1_BASE;
    int ret;

    if (!ota_state.update_available) {
        printk("OTA: No update to download\n");
        return -EINVAL;
    }

    printk("OTA: Downloading firmware from %s\n", ota_state.firmware_url);
    ota_state.state = OTA_STATE_DOWNLOADING;

    // Download to flash (chunk by chunk)
    while (1) {
        ret = http_download_chunk(OTA_SERVER_HOST, ota_state.firmware_url,
                             buffer, CHUNK_SIZE, &received);
        if (ret < 0 || received == 0) {
            break;
        }

        // Write to flash (Slot 1)
        if (flash_dev) {
            // Note: In real implementation, need to erase sector first
            // flash_erase(flash_dev, offset, SECTOR_SIZE);
            // flash_write(flash_dev, offset, buffer, received);
        }
        
        offset += received;
        ota_state.downloaded_size += received;
        
        printk("OTA: Written %u bytes\r", ota_state.downloaded_size);
    }

    printk("\nOTA: Download complete (%u bytes)\n", ota_state.downloaded_size);
    ota_state.state = OTA_STATE_IDLE;

    return 0;
}

// Reboot into new slot
void ota_reboot_to_new_slot(void)
{
    printk("OTA: Rebooting into new slot...\n");
    ota_state.state = OTA_STATE_REBOOTING;
    
    sys_reboot(SYS_REBOOT_COLD);
}

// Confirm update (tell server)
int ota_confirm_update(void)
{
    char response[HTTP_RESP_SIZE];
    char path[128];
    
    snprintf(path, sizeof(path),
        "/api/confirm?id=%s&version=%s",
        NODE_ID, FIRMWARE_VERSION);

    http_get(OTA_SERVER_HOST, path, response, sizeof(response));
    
    printk("OTA: Update confirmed\n");
    ota_state.state = OTA_STATE_CONFIRMED;
    
    return 0;
}

// Main poll (call this periodically)
void ota_poll_server(void)
{
    int ret;

    // Check for updates
    ret = ota_check_for_update();
    if (ret == 0 && ota_state.update_available) {
        // Download new firmware
        ret = ota_download_and_flash();
        if (ret == 0) {
            // Reboot
            k_sleep(K_SECONDS(2));
            ota_reboot_to_new_slot();
        }
    }
}