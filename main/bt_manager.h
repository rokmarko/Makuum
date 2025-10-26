#ifndef BT_MANAGER_H
#define BT_MANAGER_H

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_err.h"

// External event bits (defined in main.c)
#define TOOL_POWER_ON_BIT BIT0
#define BT_CONNECTED_BIT BIT1

// BLE Service and Characteristic UUIDs for Makita vacuum control
// #define MAKITA_SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
// #define MAKITA_RX_CHAR_UUID "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"  // Write to ESP32
// #define MAKITA_TX_CHAR_UUID "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"  // Read from ESP32

// Makita tool power keywords that trigger vacuum activation
// #define MAKITA_POWER_KEYWORD "MAKITA_POWER_ON"
// #define AWS_TOOL_KEYWORD "TOOL_ACTIVATED"
// #define POWER_ON_KEYWORD "POWER_ON"

/**
 * @brief Initialize Bluetooth manager
 * @param event_group Event group handle for communication with main app
 * @return ESP_OK on success
 */
esp_err_t bt_manager_init(EventGroupHandle_t event_group);

/**
 * @brief Manual control: Turn AWS tool ON
 * @return ESP_OK on success
 */
esp_err_t bt_aws_tool_on(void);

/**
 * @brief Print current AWS tool status to log
 */
void bt_aws_print_status(void);

#endif // BT_MANAGER_H