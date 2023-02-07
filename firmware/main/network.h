//
// Created by Braden Nicholson on 2/1/23.
//

#ifndef RADAR_NETWORK_H
#define RADAR_NETWORK_H

#include <esp_wifi.h>
#include <cstring>

#define AP_SSID "VRadar"
#define AP_CHANNEL 11
#define AP_PASSWD "doppler"
#define AP_MAX_CONN 5

#define AP_IP "10.0.1.1"
#define AP_GATEWAY AP_IP
#define AP_NETMASK "255.255.255.0"

#define AP_DNS_PORT htons(53)
#define AP_SCAN_MAX_AP 20

#define AP_HTML_HEADER "<!DOCTYPE html><html>"
#define AP_HTML_FOOTER "</html>"

static wifi_ap_record_t wifi_records[AP_SCAN_MAX_AP];

struct Credentials {
    char *ssid;
    char *passwd;
} typedef Credentials;

class Network {

public:

    static Network &instance();

    Network(const Network &) = default;

    Network();

    Network &operator=(const Network &) = delete;

    esp_err_t startAP();

    esp_err_t startSTA(Credentials credentials);

    esp_err_t attemptSTA(Credentials credentials);

    wifi_ap_record_t scannedNetworks[AP_SCAN_MAX_AP]{};

    esp_err_t scan();

private:

    int attemptNum = 0;


    TaskHandle_t dnsTask{};


};


#endif //RADAR_NETWORK_H
