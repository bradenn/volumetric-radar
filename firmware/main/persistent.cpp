//
// Created by Braden Nicholson on 2/1/23.
//

#include <esp_err.h>
#include <cstring>

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
        nvs_flash_init();
    }
    // Attempt to open the storage handle
    err = nvs_open("storage", NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
    }
}

// Write a string to non-volatile storage
void Persistent::writeString(const char *key, const char* value) const {
    ESP_ERROR_CHECK(nvs_set_str(handle, key, value));
}


// Read a string from non-volatile storage
void Persistent::readString(const char *key, char *dest) const {
    size_t stringSize = 0;
    // Find the length of the contained string if it exists
    esp_err_t err = nvs_get_str(handle, key, nullptr, &stringSize);
    if(stringSize <= 0 || err == ESP_ERR_NVS_NOT_FOUND) {
        strcpy(dest, "unset");
        return;
    }
    ESP_ERROR_CHECK(nvs_get_str(handle, key, dest, &stringSize));
}

Persistent &Persistent::instance() {
    static Persistent the_instance;
    return the_instance;
}

void Persistent::writeInt(const char *key, int32_t value) const {
    ESP_ERROR_CHECK(nvs_set_i32(handle, key, value));
}

void Persistent::readInt(const char *key, int32_t *dest, int32_t defaultValue) const {
    esp_err_t err = nvs_get_i32(handle, key, dest);
    if(err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_ERROR_CHECK(nvs_set_i32(handle, key, defaultValue));
    }

}
