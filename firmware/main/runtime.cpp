//
// Created by Braden Nicholson on 2/1/23.
//

#include <cstring>
#include <esp_netif.h>
#include <esp_event.h>
#include "runtime.h"

#define NVS_KEY_SSID "networkSSID"
#define NVS_KEY_PASSWD "networkPass"

Runtime::Runtime() {
    // Set the runtime state to initializing
    state = INITIALIZE;
    // Initialize the network interface
    ESP_ERROR_CHECK(esp_netif_init());
    // Initialize the event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    // Initialize the non-volatile storage manager
    persistent = new Persistent();
    // Determine whether the system should enter softAP setup mode or station connection mode
    if(isNetworkConfigured()) {
        state = CONNECTING;
    }else{
        state = SETUP;
    }

}

Runtime::~Runtime() {

    delete persistent;

}


bool Runtime::isNetworkConfigured() {
    string ssid = persistent->readString(NVS_KEY_SSID);
    if(ssid == "unset") {
        return false;
    }
    return true;
}
