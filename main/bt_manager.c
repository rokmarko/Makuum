#include "bt_manager.h"
#include <string.h>
#include <stdio.h>
#include "esp_log.h"
// #include "esp_bt.h"
#include "esp_log_level.h"
// #include "nvs_flash.h"

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_gap.h"

static const char *TAG = "AWS_BLE_MANAGER";

static EventGroupHandle_t app_event_group = NULL;
// static bool aws_tool_detected = false;
static bool ble_scanning = false;
static bool nimble_synced = false;

// Makita AWS Protocol Constants
// #define AWS_SERVICE_UUID_128        {0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0xf0, 0xff, 0x00, 0x00}
// #define AWS_CHARACTERISTIC_UUID_128 {0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0xf1, 0xff, 0x00, 0x00}

// AWS Protocol Data
// #define AWS_TOOL_ACTIVE_DATA    0x01
// #define AWS_TOOL_IDLE_DATA      0x00

// Scan parameters for AWS tool detection - More aggressive scanning
static struct ble_gap_disc_params ble_scan_params = {
    .itvl = 0x30,    // 30ms (in 0.625ms units) - Faster interval
    .window = 0x30,  // 30ms (in 0.625ms units) - 100% duty cycle
    .filter_policy = 0, // Accept all
    .limited = 0,    // General discovery
    .passive = 0,    // Active scanning to get more data
    .filter_duplicates = 0  // Don't filter duplicates - see all advertisements
};

static void aws_tool_power_off_timer_cb(void *arg)
{
    ESP_LOGI(TAG, "⏰ AWS tool power-off delay expired - deactivating vacuum");
    
    if (app_event_group) {
        xEventGroupClearBits(app_event_group, TOOL_POWER_ON_BIT);
    }
}

static esp_timer_handle_t power_off_timer = NULL;

static void start_power_off_timer(void)
{
    // Stop existing timer if running
    if (power_off_timer) {
        esp_timer_stop(power_off_timer);
    }

    // Start 1-second delay timer (as per AWS protocol)
    esp_timer_start_once(power_off_timer, 1000000); // 1 second in microseconds
}

static void process_aws_advertisement(const struct ble_gap_disc_desc *disc)
{
    const uint8_t *adv_data = disc->data;
    uint16_t adv_len = disc->length_data;
    
    bool aws_tool_active = false;
    bool potential_aws_device = false;
    
    // Log raw advertisement data for debugging
    ESP_LOGD(TAG, "📡 Processing advertisement, length: %d", adv_len);
    
    // Parse advertisement data for AWS service UUID and device name
    for (int i = 0; i < adv_len; ) {
        if (i >= adv_len) break;
        
        uint8_t length = adv_data[i];
        if (length == 0 || i + length >= adv_len) break;
        
        uint8_t type = adv_data[i + 1];
        const uint8_t *data = &adv_data[i + 2];
        
        ESP_LOGV(TAG, "AD type: 0x%02X, length: %d", type, length - 1);
        
        // Check for complete/incomplete local name (type 0x08, 0x09)
        // if (type == 0x08 || type == 0x09) {
        //     char name[32] = {0};
        //     int name_len = (length - 1) < 31 ? (length - 1) : 31;
        //     memcpy(name, data, name_len);
        //     ESP_LOGD(TAG, "📱 Device name: %s", name);
            
        //     // Check for AWS/Makita device names
        //     if (strstr(name, "AWS") || strstr(name, "MAKITA") || 
        //         strstr(name, "aws") || strstr(name, "makita") ||
        //         strstr(name, "TOOL")) {
        //         potential_aws_device = true;
        //         ESP_LOGI(TAG, "🎯 Potential AWS device name found: %s", name);
        //     }
        // }
        
        // Check for 16-bit service UUIDs (type 0x02, 0x03)
        // else if ((type == 0x02 || type == 0x03) && (length - 1) >= 2) {
        //     for (int j = 0; j < (length - 1); j += 2) {
        //         uint16_t uuid = (data[j + 1] << 8) | data[j];
        //         ESP_LOGV(TAG, "16-bit Service UUID: 0x%04X", uuid);
        //         if (uuid == 0xFFF0) {  // AWS service UUID (16-bit version)
        //             aws_service_found = true;
        //             ESP_LOGI(TAG, "🔍 Found AWS 16-bit service UUID: 0xFFF0");
        //         }
        //     }
        // }
        
        // Check for 128-bit service UUID (type 0x06 or 0x07)
        // else if ((type == 0x06 || type == 0x07) && (length - 1) == 16) {
        //     // Compare with AWS service UUID
        //     uint8_t aws_uuid[] = AWS_SERVICE_UUID_128;
        //     if (memcmp(data, aws_uuid, 16) == 0) {
        //         aws_service_found = true;
        //         ESP_LOGI(TAG, "🔍 Found AWS 128-bit service UUID in advertisement");
        //     }
            
        //     // Log UUID for debugging
        //     ESP_LOGD(TAG, "128-bit UUID: %02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        //             data[15], data[14], data[13], data[12],
        //             data[11], data[10], data[9], data[8],
        //             data[7], data[6], data[5], data[4],
        //             data[3], data[2], data[1], data[0]);
        // }
        
        // Check for manufacturer data that might contain tool status
        if (type == 0xFF && (length - 1) >= 4) {
            ESP_LOGD(TAG, "📦 Manufacturer data found, length: %d, %x,%x", length - 1, data[0], data[1]);
            
            // Check for AWS tool active pattern
            if (data[2] == 3 && data[3] == 6) {
                potential_aws_device = true;
                ESP_LOGD(TAG, "🔋 AWS tool detected");
                if (data[0] == 0xfd && data[1] == 0xaa) {
                    ESP_LOGD(TAG, "🔋 AWS tool ACTIVE signal detected in manufacturer data");
                    aws_tool_active = true;
                }
            }
        }
        
        // Check for service data
        // else if (type == 0x16 && (length - 1) >= 2) {
        //     uint16_t service_uuid = (data[1] << 8) | data[0];
        //     ESP_LOGV(TAG, "Service data for UUID: 0x%04X", service_uuid);
        //     if (service_uuid == 0xFFF0) {
        //         aws_service_found = true;
        //         ESP_LOGI(TAG, "🔍 Found AWS service data");
        //     }
        // }
        
        i += length + 1;
    }
    
    // Detect AWS tool based on service UUID OR potential device name
    if (potential_aws_device && app_event_group) {
        xEventGroupSetBits(app_event_group, BT_CONNECTED_BIT);

        if(aws_tool_active) {
            // Reset the power-off timer since we're still seeing the tool
            // ESP_LOGI(TAG, "🔌 AWS tool ACTIVATED");
            xEventGroupSetBits(app_event_group, TOOL_POWER_ON_BIT);
            start_power_off_timer();
        }
    }
}

static int gap_event_handler(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {
        case BLE_GAP_EVENT_DISC:
            // Discovery event - advertisement received
            {
                char addr_str[18];
                snprintf(addr_str, sizeof(addr_str), "%02x:%02x:%02x:%02x:%02x:%02x",
                         event->disc.addr.val[0], event->disc.addr.val[1], event->disc.addr.val[2],
                         event->disc.addr.val[3], event->disc.addr.val[4], event->disc.addr.val[5]);
                
                // Log all devices with decent signal strength
                if (event->disc.rssi > -70) {
                    ESP_LOGD(TAG, "📱 BLE device: %s, RSSI: %d dBm, AD len: %d", 
                            addr_str, event->disc.rssi, event->disc.length_data);
                } else {
                    ESP_LOGV(TAG, "📱 BLE device: %s, RSSI: %d dBm (weak)", addr_str, event->disc.rssi);
                }
                
                // Process advertisement for AWS protocol
                process_aws_advertisement(&event->disc);
            }
            break;
            
        case BLE_GAP_EVENT_DISC_COMPLETE:
            ESP_LOGI(TAG, "BLE scan complete, restarting...");
            ble_gap_disc(BLE_OWN_ADDR_PUBLIC, BLE_HS_FOREVER, &ble_scan_params, 
                        gap_event_handler, NULL);
            break;
            
        default:
            ESP_LOGV(TAG, "GAP BLE event: %d", event->type);
            break;
    }
    return 0;
}

static void on_nimble_reset(int reason)
{
    ESP_LOGE(TAG, "NimBLE reset, reason=%d", reason);
    nimble_synced = false;
}

static void on_nimble_sync(void)
{
    ESP_LOGI(TAG, "NimBLE sync completed");
    nimble_synced = true;
    
    ESP_LOGI(TAG, "✅ NimBLE ready for scanning");
    
    // Set more aggressive scan parameters for better detection
    ble_scan_params.itvl = 0x20;    // 20ms - Very fast scanning
    ble_scan_params.window = 0x20;  // 20ms - 100% duty cycle
    
    // Automatically start scanning when sync is complete
    ESP_LOGI(TAG, "🔍 Starting aggressive AWS tool detection...");
    ESP_LOGI(TAG, "📋 Scan params: interval=20ms, window=20ms (100% duty cycle)");
    
    int rc = ble_gap_disc(BLE_OWN_ADDR_PUBLIC, BLE_HS_FOREVER, &ble_scan_params, 
                         gap_event_handler, NULL);
    if (rc == 0) {
        ble_scanning = true;
        ESP_LOGI(TAG, "🔍 BLE scanning started - looking for AWS tools");
        // ESP_LOGI(TAG, "📋 Listening for Makita AWS devices (AWS_XXXX, AWSTOOL, MAKITA)");
        // ESP_LOGI(TAG, "📋 Monitoring service UUIDs: 0xFFF0, 0000fff0-0000-1000-8000-00805f9b34fb");
    } else {
        ESP_LOGE(TAG, "Failed to start scanning: %d", rc);
    }
}

void ble_host_task(void *param) {
    nimble_port_run();                // Runs host event loop
    nimble_port_freertos_deinit();    // Clean up when stopped
}

esp_err_t bt_manager_init(EventGroupHandle_t event_group)
{
    app_event_group = event_group;
    esp_log_level_set(TAG, ESP_LOG_DEBUG);  // only this tag logs at DEBUG or higher
    ESP_LOGI(TAG, "🔧 Initializing Makita AWS BLE Scanner...");

    // Initialize NimBLE host
    nimble_port_init();

    // Configure host callbacks BEFORE starting host task
    ble_hs_cfg.reset_cb = on_nimble_reset;
    ble_hs_cfg.sync_cb = on_nimble_sync;

    // Start NimBLE host task
    nimble_port_freertos_init(ble_host_task);

    ESP_LOGI(TAG, "✅ NimBLE initialized successfully");
    
    // Create power-off delay timer
    esp_timer_create_args_t timer_args = {
        .callback = aws_tool_power_off_timer_cb,
        .name = "aws_power_off_timer"
    };
    
    esp_err_t ret = esp_timer_create(&timer_args, &power_off_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create power-off timer: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGD(TAG, "✅ Makita AWS BLE Scanner initialized successfully");
    ESP_LOGI(TAG, "🔍 Ready to detect AWS tool power events");
    
    return ESP_OK;
}

esp_err_t bt_aws_tool_on(void)
{
    ESP_LOGI(TAG, "🔌 Manual AWS tool ON");
    
    if (app_event_group) {
        xEventGroupSetBits(app_event_group, TOOL_POWER_ON_BIT);
        xEventGroupSetBits(app_event_group, BT_CONNECTED_BIT);
    }
    
    start_power_off_timer();
    return ESP_OK;
}

void bt_aws_print_status(void)
{
    ESP_LOGI(TAG, "📊 AWS Tool Status:");
    ESP_LOGI(TAG, "   BLE scanning: %s", ble_scanning ? "YES" : "NO");
    ESP_LOGI(TAG, "   Timer active: %s", power_off_timer ? "YES" : "NO");
}