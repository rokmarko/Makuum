#ifndef LED_CONTROL_H
#define LED_CONTROL_H

#include "driver/gpio.h"
#include "esp_err.h"

// LED patterns
typedef enum {
    LED_PATTERN_OFF,
    LED_PATTERN_ON,
    LED_PATTERN_SLOW_BLINK,
    LED_PATTERN_FAST_BLINK,
    LED_PATTERN_PULSE
} led_pattern_t;

// Default GPIO pin for LED (can be overridden in sdkconfig)
#ifndef CONFIG_LED_GPIO
#define CONFIG_LED_GPIO GPIO_NUM_2
#endif

/**
 * @brief Initialize LED control
 * @return ESP_OK on success
 */
esp_err_t led_init(void);

/**
 * @brief Set LED pattern
 * @param pattern LED pattern to set
 * @return ESP_OK on success
 */
esp_err_t led_set_pattern(led_pattern_t pattern);

/**
 * @brief Turn LED on
 * @return ESP_OK on success
 */
esp_err_t led_on(void);

/**
 * @brief Turn LED off
 * @return ESP_OK on success
 */
esp_err_t led_off(void);

/**
 * @brief Toggle LED state
 * @return ESP_OK on success
 */
esp_err_t led_toggle(void);

/**
 * @brief Get current LED pattern
 * @return Current LED pattern
 */
led_pattern_t led_get_pattern(void);

#endif // LED_CONTROL_H