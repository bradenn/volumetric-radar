//
// Created by Braden Nicholson on 2/1/23.
//

#include <esp_err.h>

#include "persistent.h"

// Persistent constructor initializes the non-volatile storage subsystem
Persistent::Persistent() {
    // Attempt to initialize the non-volition storage system
    esp_err_t err = nvs_flash_init();
    // Check if the nvs was corrupted or truncated, if so, restore it
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // Clear the flash records
        ESP_ERROR_CHECK(nvs_flash_erase());
        // Reinitialize the nvs
        ESP_ERROR_CHECK(nvs_flash_init());
    }
    // Attempt to open the storage handle
    err = nvs_open("storage", NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
    }
}

// Write a string to non-volatile storage
void Persistent::writeString(char *key, char* value) const {
    ESP_ERROR_CHECK(nvs_set_str(handle, key, value));
}

// Read a string from non-volatile storage
char * Persistent::readString(char *key) const {
    size_t stringSize = 0;
    // Find the length of the contained string if it exists
    esp_err_t err = nvs_get_str(handle, key, nullptr, &stringSize);
    if(stringSize <= 0 || err == ESP_ERR_NVS_NOT_FOUND) {
        return "unset";
    }
    // Allocate memory to contain the target string
    char *out = static_cast<char *>(malloc(sizeof(char) * stringSize));
    // Read the string from nvs into the out variable
    ESP_ERROR_CHECK(nvs_get_str(handle, key, out, &stringSize));
    // Return the string pointer
    return out;
}


