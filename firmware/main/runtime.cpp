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
    Indicator::instance().setIndicator(POWER, true);
    // Initialize the network interface
    ESP_ERROR_CHECK(esp_netif_init());
    // Initialize the event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    // Initialize the non-volatile storage manager
    Persistent p = Persistent::instance();
    // Determine whether the system should enter softAP setup mode or station connection mode
    char ssid[33];
    p.readString(NVS_KEY_SSID, ssid);
    char passwd[33];
    p.readString(NVS_KEY_PASSWD, passwd);

    if (strcmp("unset", ssid) == 0) {
        printf("Using DEBUG Wi-Fi credentials. Remove definitions to resume normal operation.\n");
        strcpy(ssid, CONFIG_vRADAR_WIFI_SSID);
        strcpy(passwd, CONFIG_vRADAR_WIFI_PASS);
    }

    auto net = Network::instance();
    esp_err_t err;
    Indicator::instance().setPulsing(LINK, true);
    printf("Connecting: SSID: %s PASS: %s\n", ssid, passwd);
    if (strcmp("unset", ssid) == 0) {
        state = SETUP;
        err = net.startAP();
        if (err != ESP_OK) {
            printf("Network::AP initialization failed. (%s)\n", esp_err_to_name(err));
            Indicator::instance().setIndicator(FAULT, true);
            return;
        }
    } else {
        state = CONNECTING;
        err = net.startSTA({.ssid = ssid, .passwd = passwd});
        if (err != ESP_OK) {
            printf("Network::STA initialization failed. (%s)\n", esp_err_to_name(err));
            Indicator::instance().setIndicator(FAULT, true);
            return;
        }
        printf("Connected\n");
        Indicator::instance().setIndicator(LINK, true);
        Server();
    }

}

Runtime::~Runtime() {

}
