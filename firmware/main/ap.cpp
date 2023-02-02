//
// Created by Braden Nicholson on 2/1/23.
//

#include <esp_wifi_default.h>
#include <esp_wifi.h>
#include <cstring>
#include "ap.h"

#define AP_SSID "VRadar"
#define AP_CHANNEL 11
#define AP_PASSWD "doppler"
#define AP_MAX_CONN 5


static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {

}

AccessPoint::AccessPoint() {
    // Create a soft access point
    esp_netif_create_default_wifi_ap();
    // Configure the system with the default modem settings
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    // Register the event handler function
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
            WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, this, nullptr));
    // Define the parameters for the access point
    wifi_config_t apConfig = {
            .ap {
                    .ssid = AP_SSID,
                    .password = AP_PASSWD,
                    .ssid_len = (uint8_t) strlen(AP_SSID),
                    .channel = AP_CHANNEL,
                    .authmode = WIFI_AUTH_OPEN,
                    .max_connection = AP_MAX_CONN,
            }
    };
    // Set the system Wi-Fi module to operate in access point mode
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    // Pass the configuration to the Wi-Fi subsystem
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &apConfig));
    // Open the access point
    ESP_ERROR_CHECK(esp_wifi_start());

}
