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

#define BUTTON_GPIO 0
static gpio_num_t relay_gpio = 16;  // Default Relay GPIO
#define DEVICE_NAME "Makita_Vacuum"

// Event group for synchronization
EventGroupHandle_t vacuum_event_group;
#define TOOL_POWER_ON_BIT BIT0
#define BT_CONNECTED_BIT BIT1
#define AUTO_MODE_TOGGLE_BIT BIT2

// Vacuum state
typedef enum {
    VACUUM_STATE_IDLE,
    VACUUM_STATE_STANDBY,
    VACUUM_STATE_ACTIVE
} vacuum_state_t;

static vacuum_state_t current_state = VACUUM_STATE_IDLE;

// Automatic mode state (disabled on startup)
static bool automatic_mode_enabled = false;

// Button debouncing variables
static uint32_t last_button_press_time = 0;
#define BUTTON_DEBOUNCE_MS 200

// Button interrupt handler
static void IRAM_ATTR button_isr_handler(void* arg)
{
    uint32_t current_time = xTaskGetTickCountFromISR() * portTICK_PERIOD_MS;
    
    // Simple debouncing
    if (current_time - last_button_press_time > BUTTON_DEBOUNCE_MS) {
        last_button_press_time = current_time;
        
        // Set the event bit to toggle automatic mode
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xEventGroupSetBitsFromISR(vacuum_event_group, AUTO_MODE_TOGGLE_BIT, &xHigherPriorityTaskWoken);
        
        if (xHigherPriorityTaskWoken) {
            portYIELD_FROM_ISR();
        }
    }
}

static void relay_init(void)
{  
    ESP_LOGI(TAG, "Initializing Relay GPIO %d", relay_gpio);

    // Configure GPIO
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << relay_gpio),
        .pull_down_en = 0,
        .pull_up_en = 0,
    };
    
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Relay GPIO config failed: %s", esp_err_to_name(ret));
        // return ret;
    }

    gpio_set_level(relay_gpio, 0);
}

// Initialize pushbutton GPIO
static void button_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE  // Trigger on falling edge (button press)
    };
    
    gpio_config(&io_conf);
    
    // Install ISR service if not already installed
    esp_err_t ret = gpio_install_isr_service(0);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to install GPIO ISR service: %s", esp_err_to_name(ret));
        return;
    }
    
    // Add ISR handler for the button
    gpio_isr_handler_add(BUTTON_GPIO, button_isr_handler, NULL);
    
    ESP_LOGI(TAG, "Button initialized on GPIO %d", BUTTON_GPIO);
}

static void vacuum_state_machine_task(void *pvParameters)
{
    EventBits_t bits;
    
    while (1) {
        // Wait for events with a timeout
        bits = xEventGroupWaitBits(vacuum_event_group,
                                   TOOL_POWER_ON_BIT | BT_CONNECTED_BIT | AUTO_MODE_TOGGLE_BIT,
                                   pdFALSE,  // Don't clear bits on exit
                                   pdFALSE,  // Wait for any bit
                                   pdMS_TO_TICKS(1000));

        // Check for automatic mode toggle
        if (bits & AUTO_MODE_TOGGLE_BIT) {
            automatic_mode_enabled = !automatic_mode_enabled;
            xEventGroupClearBits(vacuum_event_group, AUTO_MODE_TOGGLE_BIT);
            
            ESP_LOGI(TAG, "🔘 Automatic mode %s", 
                     automatic_mode_enabled ? "ENABLED" : "DISABLED");
            
            // Visual feedback: brief LED flash pattern
            if (automatic_mode_enabled) {
                gpio_set_level(relay_gpio, 1);

                // Flash LED quickly 3 times to indicate auto mode enabled
                led_set_pattern(LED_PATTERN_FAST_BLINK);
                vTaskDelay(pdMS_TO_TICKS(2000));
            } else {
                gpio_set_level(relay_gpio, 0); // Deactivate relay
                // Single long flash to indicate auto mode disabled
                led_set_pattern(LED_PATTERN_FAST_BLINK);
                vTaskDelay(pdMS_TO_TICKS(2000));
            }
            
            // Restore appropriate LED pattern based on current state
            // switch (current_state) {
            //     case VACUUM_STATE_IDLE:
            //         led_set_pattern(LED_PATTERN_PULSE);
            //         break;
            //     case VACUUM_STATE_STANDBY:
            //         led_set_pattern(LED_PATTERN_SLOW_BLINK);
            //         break;
            //     case VACUUM_STATE_ACTIVE:
            //         led_set_pattern(LED_PATTERN_ON);
            //         break;
            // }
        }

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
                    if (automatic_mode_enabled) {
                        ESP_LOGI(TAG, "Tool power detected - ACTIVATING vacuum!");
                        current_state = VACUUM_STATE_ACTIVE;
                        led_set_pattern(LED_PATTERN_ON);
                        
                        // Simulate vacuum activation
                        ESP_LOGI(TAG, "🌪️  VACUUM CLEANER ACTIVATED! 🌪️");
                        gpio_set_level(relay_gpio, 0); // Activate relay for indication
                        
                        // Clear the power on bit after activation
                        // xEventGroupClearBits(vacuum_event_group, TOOL_POWER_ON_BIT);

                        // Auto-deactivate after 10 seconds (simulate tool power off)
                        // vTaskDelay(pdMS_TO_TICKS(10000));
                        // ESP_LOGI(TAG, "Tool power timeout - deactivating vacuum");
                        // current_state = VACUUM_STATE_STANDBY;
                        // led_set_pattern(LED_PATTERN_SLOW_BLINK);
                    } else {
                        ESP_LOGI(TAG, "Tool power detected but automatic mode is DISABLED - ignoring");
                        // Clear the power on bit since we're ignoring it
                        // xEventGroupClearBits(vacuum_event_group, TOOL_POWER_ON_BIT);
                    }
                }
                break;
                
            case VACUUM_STATE_ACTIVE:
                if (!(bits & BT_CONNECTED_BIT)) {
                    ESP_LOGI(TAG, "Bluetooth disconnected during operation - emergency stop!");
                    current_state = VACUUM_STATE_IDLE;
                    led_set_pattern(LED_PATTERN_OFF);
                } else if (bits & TOOL_POWER_ON_BIT) {
                    // Reset timer if we get another power signal
                    // xEventGroupClearBits(vacuum_event_group, TOOL_POWER_ON_BIT);
                }
                else {
                    ESP_LOGI(TAG, "Tool power OFF detected - returning to STANDBY");
                    current_state = VACUUM_STATE_STANDBY;
                    led_set_pattern(LED_PATTERN_SLOW_BLINK);
                    gpio_set_level(relay_gpio, 1); // Deactivate relay for indication

                    // Simulate vacuum deactivation
                    ESP_LOGI(TAG, "🛑 VACUUM CLEANER DEACTIVATED! 🛑");
                }
                break;
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

static void print_status_task(void *pvParameters)
{
    while (1) {
        ESP_LOGI(TAG, "Status - State: %s, BT: %s, Auto Mode: %s", 
                 current_state == VACUUM_STATE_IDLE ? "IDLE" :
                 current_state == VACUUM_STATE_STANDBY ? "STANDBY" : "ACTIVE",
                 (xEventGroupGetBits(vacuum_event_group) & BT_CONNECTED_BIT) ? "Connected" : "Disconnected",
                 automatic_mode_enabled ? "ENABLED" : "DISABLED");

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
    led_set_pattern(LED_PATTERN_FAST_BLINK);
    
    // Initialize pushbutton for automatic mode toggle
    ESP_LOGI(TAG, "Initializing pushbutton control...");
    button_init();
    
    relay_init();
    
    // Initialize Bluetooth
    ESP_LOGI(TAG, "Initializing Bluetooth...");
    bt_manager_init(vacuum_event_group);
    
    // Start tasks
    xTaskCreate(vacuum_state_machine_task, "vacuum_sm", 4096, NULL, 5, NULL);
    xTaskCreate(print_status_task, "status", 2048, NULL, 3, NULL);
    
    ESP_LOGI(TAG, "✅ Makita Vacuum Cleaner Ready!");
    ESP_LOGI(TAG, "📱 Automatic mode: DISABLED (press button on GPIO%d to toggle)", BUTTON_GPIO);
    ESP_LOGI(TAG, "🔗 Waiting for Bluetooth connection...");
    
    // Main loop - could be used for additional monitoring
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}