#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/gpio.h"

#include "led_control.h"
#include "bt_manager.h"

static const char *TAG = "MAKITA_VACUUM";

#define LED_GPIO CONFIG_LED_GPIO
#define DEVICE_NAME "Makita_Vacuum"

// Event group for synchronization
EventGroupHandle_t vacuum_event_group;
#define TOOL_POWER_ON_BIT BIT0
#define BT_CONNECTED_BIT BIT1

// Vacuum state
typedef enum {
    VACUUM_STATE_IDLE,
    VACUUM_STATE_STANDBY,
    VACUUM_STATE_ACTIVE
} vacuum_state_t;

static vacuum_state_t current_state = VACUUM_STATE_IDLE;

static void vacuum_state_machine_task(void *pvParameters)
{
    EventBits_t bits;
    
    while (1) {
        // Wait for events with a timeout
        bits = xEventGroupWaitBits(vacuum_event_group,
                                   TOOL_POWER_ON_BIT | BT_CONNECTED_BIT,
                                   pdFALSE,  // Don't clear bits on exit
                                   pdFALSE,  // Wait for any bit
                                   pdMS_TO_TICKS(1000));

        switch (current_state) {
            case VACUUM_STATE_IDLE:
                if (bits & BT_CONNECTED_BIT) {
                    ESP_LOGI(TAG, "Bluetooth connected - entering STANDBY mode");
                    current_state = VACUUM_STATE_STANDBY;
                    led_set_pattern(LED_PATTERN_SLOW_BLINK);
                }
                break;
                
            case VACUUM_STATE_STANDBY:
                if (!(bits & BT_CONNECTED_BIT)) {
                    ESP_LOGI(TAG, "Bluetooth disconnected - returning to IDLE");
                    current_state = VACUUM_STATE_IDLE;
                    led_set_pattern(LED_PATTERN_OFF);
                } else if (bits & TOOL_POWER_ON_BIT) {
                    ESP_LOGI(TAG, "Tool power detected - ACTIVATING vacuum!");
                    current_state = VACUUM_STATE_ACTIVE;
                    led_set_pattern(LED_PATTERN_ON);
                    
                    // Simulate vacuum activation
                    ESP_LOGI(TAG, "🌪️  VACUUM CLEANER ACTIVATED! 🌪️");
                    
                    // Clear the power on bit after activation
                    xEventGroupClearBits(vacuum_event_group, TOOL_POWER_ON_BIT);

                    // Auto-deactivate after 10 seconds (simulate tool power off)
                    vTaskDelay(pdMS_TO_TICKS(10000));
                    ESP_LOGI(TAG, "Tool power timeout - deactivating vacuum");
                    current_state = VACUUM_STATE_STANDBY;
                    led_set_pattern(LED_PATTERN_SLOW_BLINK);
                }
                break;
                
            case VACUUM_STATE_ACTIVE:
                if (!(bits & BT_CONNECTED_BIT)) {
                    ESP_LOGI(TAG, "Bluetooth disconnected during operation - emergency stop!");
                    current_state = VACUUM_STATE_IDLE;
                    led_set_pattern(LED_PATTERN_OFF);
                } else if (bits & TOOL_POWER_ON_BIT) {
                    // Reset timer if we get another power signal
                    xEventGroupClearBits(vacuum_event_group, TOOL_POWER_ON_BIT);
                }
                break;
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

static void print_status_task(void *pvParameters)
{
    while (1) {
        ESP_LOGI(TAG, "Status - State: %s, BT: %s", 
                 current_state == VACUUM_STATE_IDLE ? "IDLE" :
                 current_state == VACUUM_STATE_STANDBY ? "STANDBY" : "ACTIVE",
                 (xEventGroupGetBits(vacuum_event_group) & BT_CONNECTED_BIT) ? "Connected" : "Disconnected");

        vTaskDelay(pdMS_TO_TICKS(10000)); // Print status every 10 seconds
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "🔧 Makita Vacuum Cleaner Starting... 🔧");
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Create event group
    vacuum_event_group = xEventGroupCreate();
    if (vacuum_event_group == NULL) {
        ESP_LOGE(TAG, "Failed to create event group");
        return;
    }
    
    // Initialize LED control
    ESP_LOGI(TAG, "Initializing LED control...");
    led_init();
    led_set_pattern(LED_PATTERN_OFF);
    
    // Initialize Bluetooth
    ESP_LOGI(TAG, "Initializing Bluetooth...");
    bt_manager_init(vacuum_event_group);
    
    // Start tasks
    xTaskCreate(vacuum_state_machine_task, "vacuum_sm", 4096, NULL, 5, NULL);
    xTaskCreate(print_status_task, "status", 2048, NULL, 3, NULL);
    
    ESP_LOGI(TAG, "✅ Makita Vacuum Cleaner Ready! Waiting for Bluetooth connection...");
    
    // Main loop - could be used for additional monitoring
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}