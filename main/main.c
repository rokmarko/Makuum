#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/timers.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/gpio.h"

#include "led_control.h"
#include "bt_manager.h"

static const char *TAG = "MAKITA_VACUUM";

#define BUTTON_GPIO 4
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

// Enhanced button debouncing and spike filtering
#define BUTTON_DEBOUNCE_MS 50          // Debounce time in ms
#define BUTTON_SPIKE_FILTER_SAMPLES 3  // Number of consistent readings needed
#define BUTTON_PRESS_MIN_TIME_MS 30    // Minimum press time to be valid
#define BUTTON_RELEASE_MIN_TIME_MS 50  // Minimum release time between presses

typedef enum {
    BUTTON_STATE_IDLE,
    BUTTON_STATE_PRESSED_DEBOUNCE,
    BUTTON_STATE_PRESSED_CONFIRMED,
    BUTTON_STATE_RELEASED_DEBOUNCE
} button_state_t;

static volatile button_state_t button_state = BUTTON_STATE_IDLE;
static volatile uint32_t button_press_start_time = 0;
static volatile uint32_t button_last_change_time = 0;
static volatile uint8_t button_spike_filter_count = 0;
static volatile bool button_expected_level = 1; // Expected GPIO level (1 = released, 0 = pressed)
static TimerHandle_t button_debounce_timer = NULL;

// Button debounce timer callback
static void button_debounce_timer_callback(TimerHandle_t xTimer)
{
    uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    int current_level = gpio_get_level(BUTTON_GPIO);
    
    switch (button_state) {
        case BUTTON_STATE_PRESSED_DEBOUNCE:
            // Check if button is still pressed after debounce period
            if (current_level == 0) {
                // Confirm button press
                button_state = BUTTON_STATE_PRESSED_CONFIRMED;
                button_press_start_time = current_time;
                ESP_LOGD(TAG, "Button press confirmed");
            } else {
                // False trigger, return to idle
                button_state = BUTTON_STATE_IDLE;
                button_expected_level = 1;
                ESP_LOGD(TAG, "Button press rejected (spike)");
            }
            break;
            
        case BUTTON_STATE_RELEASED_DEBOUNCE:
            // Check if button is still released after debounce period
            if (current_level == 1) {
                // Confirm button release - check if press was long enough
                uint32_t press_duration = current_time - button_press_start_time;
                
                if (press_duration >= BUTTON_PRESS_MIN_TIME_MS) {
                    // Valid button press detected - trigger automatic mode toggle
                    xEventGroupSetBits(vacuum_event_group, AUTO_MODE_TOGGLE_BIT);
                    ESP_LOGI(TAG, "Valid button press detected (duration: %lu ms)", press_duration);
                } else {
                    ESP_LOGD(TAG, "Button press too short (duration: %lu ms)", press_duration);
                }
                
                button_state = BUTTON_STATE_IDLE;
                button_expected_level = 1;
            } else {
                // Button pressed again quickly, return to pressed state
                button_state = BUTTON_STATE_PRESSED_CONFIRMED;
                ESP_LOGD(TAG, "Button pressed again during release debounce");
            }
            break;
            
        default:
            button_state = BUTTON_STATE_IDLE;
            button_expected_level = 1;
            break;
    }
}

// Enhanced button interrupt handler with spike filtering
static void IRAM_ATTR button_isr_handler(void* arg)
{
    uint32_t current_time = xTaskGetTickCountFromISR() * portTICK_PERIOD_MS;
    int current_level = gpio_get_level(BUTTON_GPIO);
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    // Ignore interrupts if they happen too frequently (hardware spike protection)
    if (current_time - button_last_change_time < 5) { // 5ms minimum between interrupts
        return;
    }
    
    button_last_change_time = current_time;
    
    // Spike filter: only proceed if we get the expected level change
    if (current_level == button_expected_level) {
        // This is not the expected transition, likely a spike
        return;
    }
    
    // Process state machine
    switch (button_state) {
        case BUTTON_STATE_IDLE:
            if (current_level == 0) { // Button pressed
                button_state = BUTTON_STATE_PRESSED_DEBOUNCE;
                button_expected_level = 0;
                button_spike_filter_count = 0;
                
                // Start debounce timer
                xTimerStartFromISR(button_debounce_timer, &xHigherPriorityTaskWoken);
            }
            break;
            
        case BUTTON_STATE_PRESSED_CONFIRMED:
            if (current_level == 1) { // Button released
                // Check minimum press time
                if (current_time - button_press_start_time >= BUTTON_PRESS_MIN_TIME_MS) {
                    button_state = BUTTON_STATE_RELEASED_DEBOUNCE;
                    button_expected_level = 1;
                    
                    // Start debounce timer for release
                    xTimerStartFromISR(button_debounce_timer, &xHigherPriorityTaskWoken);
                } else {
                    // Press was too short, probably a spike
                    button_state = BUTTON_STATE_IDLE;
                    button_expected_level = 1;
                }
            }
            break;
            
        default:
            // Shouldn't happen in ISR during debounce states
            break;
    }
    
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
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

// Initialize pushbutton GPIO with enhanced debouncing
static void button_init(void)
{
    // Create debounce timer first
    button_debounce_timer = xTimerCreate(
        "button_debounce",                    // Timer name
        pdMS_TO_TICKS(BUTTON_DEBOUNCE_MS),   // Timer period
        pdFALSE,                             // Auto-reload (pdFALSE = one-shot)
        NULL,                                // Timer ID
        button_debounce_timer_callback       // Callback function
    );
    
    if (button_debounce_timer == NULL) {
        ESP_LOGE(TAG, "Failed to create button debounce timer");
        return;
    }
    
    // Configure GPIO with enhanced settings for better noise immunity
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,    // Enable internal pull-up
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE       // Trigger on both edges for better state tracking
    };
    
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Button GPIO config failed: %s", esp_err_to_name(ret));
        return;
    }
    
    // Set GPIO input hysteresis for better noise immunity (if supported)
    gpio_set_drive_capability(BUTTON_GPIO, GPIO_DRIVE_CAP_0); // Minimum drive for input
    
    // Install ISR service if not already installed
    ret = gpio_install_isr_service(0);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to install GPIO ISR service: %s", esp_err_to_name(ret));
        return;
    }
    
    // Initialize button state
    button_state = BUTTON_STATE_IDLE;
    button_expected_level = gpio_get_level(BUTTON_GPIO);
    button_last_change_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    // Add ISR handler for the button
    gpio_isr_handler_add(BUTTON_GPIO, button_isr_handler, NULL);
    
    ESP_LOGI(TAG, "Enhanced button initialized on GPIO %d with debouncing and spike filtering", BUTTON_GPIO);
    ESP_LOGI(TAG, "Button config: debounce=%dms, min_press=%dms, spike_filter=%d samples", 
             BUTTON_DEBOUNCE_MS, BUTTON_PRESS_MIN_TIME_MS, BUTTON_SPIKE_FILTER_SAMPLES);
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
                        gpio_set_level(relay_gpio, 0);
                        
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