#include <stdio.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "ota.h"

// WiFi credentials
#define WIFI_SSID     "Zero"
#define WIFI_PSK      "abcdefgh"

// ======================================================
// USER SENSOR CODE SECTION
// ======================================================
// Add your sensor code here!
// 
// Step 1: Initialize your sensor in sensor_init()
// Step 2: Read sensor data in sensor_read()
// Step 3: Process data in the main loop
// 
// Example sensor functions:
// 
// void sensor_init(void) {
//     printk("Initializing sensor...\n");
//     // Your sensor initialization code
//     my_sensor_setup();
// }
// 
// void sensor_read(void) {
//     // Read sensor data
//     float value = my_sensor_read_value();
//     printk("Sensor value: %.2f\n", value);
//     // Send to Hub or process
// }
// 
// ======================================================

// Placeholder for sensor initialization
// Replace this with your sensor init code
void sensor_init(void)
{
    printk("[SENSOR] Placeholder: sensor_init() called\n");
    // TODO: Add your sensor initialization here
    // Examples:
    // - I2C sensor: i2c_init();
    // - ADC: adc_channel_setup();
    // - GPIO: gpio_pin_configure();
}

// Placeholder for reading sensor data
// Replace this with your sensor reading code
void sensor_read(void)
{
    printk("[SENSOR] Placeholder: sensor_read() called\n");
    // TODO: Add your sensor reading code here
    // Examples:
    // - Read I2C: i2c_reg_read(SENSOR_ADDR, REG_DATA);
    // - Read ADC: adc_read(ADC_CHANNEL);
    // - Read GPIO: gpio_pin_get();
}

// Process sensor data (optional)
// Add data processing, filtering, or sending to Hub here
void sensor_process(void)
{
    // TODO: Add your processing code here
    // Examples:
    // - Apply calibration
    // - Filter data
    // - Send to Hub via network
}

// ======================================================
// MAIN APPLICATION
// ======================================================

void main(void)
{
    printk("===========================================\n");
    printk("  ESP32 OTA Node starting...\n");
    printk("  Node ID: %s\n", NODE_ID);
    printk("  Firmware: %s\n", FIRMWARE_VERSION);
    printk("===========================================\n\n");

    // Initialize WiFi
    printk("Initializing WiFi...\n");
    wifi_init();

    // Connect to WiFi
    printk("Connecting to WiFi '%s'...\n", WIFI_SSID);
    int ret = wifi_connect(WIFI_SSID, WIFI_PSK);
    if (ret < 0) {
        printk("ERROR: WiFi connection failed (%d)\n", ret);
        return;
    }

    // Wait for IP address
    printk("Waiting for IP address...\n");
    wifi_wait_for_ip_addr();
    printk("WiFi connected!\n\n");

    // Initialize OTA
    printk("Initializing OTA client...\n");
    ota_init();

    // Initialize USER sensor
    printk("Initializing user sensor...\n");
    sensor_init();

    // Initial check
    printk("\n--- Checking for firmware updates ---\n");
    ota_poll_server();

    printk("\n--- OTA check complete ---\n\n");

    // Main loop - runs sensor + checks for OTA updates
    while (1) {
        // Read sensor data
        sensor_read();
        
        // Optional: process sensor data
        sensor_process();
        
        // Sleep interval (adjust for your sensor needs)
        // Default: 10 seconds. Change to K_MINUTES(X) for minutes
        printk("Sleeping for 10 seconds...\n");
        k_sleep(K_SECONDS(10));
        
        // Periodically check for OTA updates (every 5 minutes)
        static int loop_count = 0;
        loop_count++;
        if (loop_count >= 30) {  // 30 * 10 seconds = 5 minutes
            loop_count = 0;
            printk("Checking for OTA updates...\n");
            ota_poll_server();
        }
    }
}
