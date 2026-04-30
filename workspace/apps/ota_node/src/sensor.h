#ifndef SENSOR_H
#define SENSOR_H

#include <zephyr/kernel.h>

// ======================================================
// SENSOR CONFIGURATION
// ======================================================
// Configure your sensor settings here
// ======================================================

// Sensor reading interval (in seconds)
#define SENSOR_READ_INTERVAL  10

// Whether to send sensor data to Hub
#define SENSOR_SEND_TO_HUB  1

// ======================================================
// SENSOR FUNCTIONS
// ======================================================

// Initialize sensor hardware
// Call this once at startup
void sensor_init(void);

// Read current sensor value(s)
// Call this periodically in main loop
void sensor_read(void);

// Process and handle sensor data
// Optional: filtering, calibration, sending to Hub
void sensor_process(void);

// ======================================================
// CUSTOM SENSOR IMPLEMENTATION
// ======================================================
// Add your sensor-specific functions below:
//
// Example for I2C sensor:
// -------------------
// #include <zephyr/drivers/i2c.h>
//
// #define SENSOR_I2C_ADDR  0x68
//
// static const struct device *i2c_dev;
//
// void sensor_init(void) {
//     i2c_dev = device_get_binding("I2C_0");
//     // Configure sensor
// }
//
// float sensor_read_temperature(void) {
//     uint8_t reg = TEMP_REG;
//     uint8_t data[2];
//     i2c_reg_read(i2c_dev, SENSOR_I2C_ADDR, reg, data, 2);
//     return convert_to_celsius(data);
// }
// -------------------
//
// Example for ADC:
// -------------------
// #include <zephyr/drivers/adc.h>
//
// #define ADC_CHANNEL  0
//
// void sensor_read_adc(void) {
//     int16_t raw;
//     adc_read(ADC_CHANNEL, &raw);
//     float voltage = raw * 3.3 / 4096;
// }
// -------------------
//
// Example for GPIO input:
// -------------------
// #include <zephyr/drivers/gpio.h>
//
// void sensor_read_button(void) {
//     int value;
//     gpio_pin_get(button_dev, BUTTON_PIN, &value);
// }
// -------------------

#endif  // SENSOR_H