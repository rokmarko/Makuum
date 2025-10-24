#include "led_control.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include <stdbool.h>

static const char *TAG = "LED_CONTROL";

static gpio_num_t led_gpio = CONFIG_LED_GPIO;
static led_pattern_t current_pattern = LED_PATTERN_OFF;
static bool led_state = false;
static TaskHandle_t led_task_handle = NULL;
static bool led_task_running = false;

// LED control task
static void led_task(void *pvParameters)
{
    led_pattern_t pattern;
    TickType_t delay;
    bool blink_state = false;
    int pulse_direction = 1;
    int pulse_intensity = 0;
    
    while (led_task_running) {
        pattern = current_pattern;
        
        switch (pattern) {
            case LED_PATTERN_OFF:
                gpio_set_level(led_gpio, 0);
                led_state = false;
                delay = pdMS_TO_TICKS(1000); // Check every second
                break;
                
            case LED_PATTERN_ON:
                gpio_set_level(led_gpio, 1);
                led_state = true;
                delay = pdMS_TO_TICKS(1000); // Check every second
                break;
                
            case LED_PATTERN_SLOW_BLINK:
                blink_state = !blink_state;
                gpio_set_level(led_gpio, blink_state ? 1 : 0);
                led_state = blink_state;
                delay = pdMS_TO_TICKS(1000); // 1 second on, 1 second off
                break;
                
            case LED_PATTERN_FAST_BLINK:
                blink_state = !blink_state;
                gpio_set_level(led_gpio, blink_state ? 1 : 0);
                led_state = blink_state;
                delay = pdMS_TO_TICKS(250); // 250ms on, 250ms off
                break;
                
            case LED_PATTERN_PULSE:
                // Simple pulse simulation with on/off
                pulse_intensity += pulse_direction * 10;
                if (pulse_intensity >= 100) {
                    pulse_intensity = 100;
                    pulse_direction = -1;
                } else if (pulse_intensity <= 0) {
                    pulse_intensity = 0;
                    pulse_direction = 1;
                }
                
                // Simple approximation: LED on if intensity > 50%
                bool pulse_on = pulse_intensity > 50;
                gpio_set_level(led_gpio, pulse_on ? 1 : 0);
                led_state = pulse_on;
                delay = pdMS_TO_TICKS(50); // Update every 50ms for smooth pulse
                break;
                
            default:
                delay = pdMS_TO_TICKS(1000);
                break;
        }
        
        vTaskDelay(delay);
    }
    
    // Clean up
    led_task_handle = NULL;
    vTaskDelete(NULL);
}

esp_err_t led_init(void)
{
    ESP_LOGI(TAG, "Initializing LED control on GPIO %d", led_gpio);
    
    // Configure GPIO
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << led_gpio),
        .pull_down_en = 0,
        .pull_up_en = 0,
    };
    
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "GPIO config failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Initialize LED as off
    gpio_set_level(led_gpio, 0);
    led_state = false;
    
    // Start LED control task
    led_task_running = true;
    BaseType_t task_created = xTaskCreate(led_task, "led_task", 2048, NULL, 3, &led_task_handle);
    if (task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create LED task");
        led_task_running = false;
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "LED control initialized successfully");
    return ESP_OK;
}

esp_err_t led_set_pattern(led_pattern_t pattern)
{
    ESP_LOGI(TAG, "Setting LED pattern to: %d", pattern);
    current_pattern = pattern;
    return ESP_OK;
}

esp_err_t led_on(void)
{
    return led_set_pattern(LED_PATTERN_ON);
}

esp_err_t led_off(void)
{
    return led_set_pattern(LED_PATTERN_OFF);
}

esp_err_t led_toggle(void)
{
    if (current_pattern == LED_PATTERN_ON) {
        return led_set_pattern(LED_PATTERN_OFF);
    } else {
        return led_set_pattern(LED_PATTERN_ON);
    }
}

led_pattern_t led_get_pattern(void)
{
    return current_pattern;
}