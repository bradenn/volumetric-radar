//
// Created by Braden Nicholson on 2/1/23.
//


#include "runtime.h"
#include "server.h"

#define NVS_KEY_SSID "ssid"
#define NVS_KEY_PASSWD "passwd"


Runtime::Runtime() {
    // Set the runtime state to initializing
    state = INITIALIZE;
    // Initialize the network interface
    ESP_ERROR_CHECK(esp_netif_init());
    // Initialize the event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    // Initialize the non-volatile storage manager
    persistent = Persistent();
    // Determine whether the system should enter softAP setup mode or station connection mode
    char *ssid = persistent.readString(NVS_KEY_SSID);
    char *passwd = persistent.readString(NVS_KEY_PASSWD);
    auto net = Network();
    esp_err_t err;
    if (strcmp("unset", ssid) == 0) {
        state = SETUP;
        err = net.startAP();
        if (err != ESP_OK) {
            printf("Network::AP initialization failed. (%s)\n", esp_err_to_name(err));
            return;
        }
    } else {
        state = CONNECTING;
        err = net.startSTA({.ssid = ssid, .passwd = passwd});
        if (err != ESP_OK) {
            printf("Network::AP initialization failed. (%s)\n", esp_err_to_name(err));
            return;
        }
    }


}

Runtime::~Runtime() {

}
