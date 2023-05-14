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

// Inspired by https://github.com/abobija/esp-dns-hijack-srv/blob/master/dns_hijack_srv.h

typedef struct __attribute__((packed)) DnsHeader {
    uint16_t ID;
    uint8_t RD: 1;
    uint8_t TC: 1;
    uint8_t AA: 1;
    uint8_t OPCODE: 4;
    uint8_t QR: 1;
    uint8_t RCODE: 4;
    uint8_t Z: 3;
    uint8_t RA: 1;
    uint16_t QDCOUNT;
    uint16_t ANCOUNT;
    uint16_t NSCOUNT;
    uint16_t ARCOUNT;
} DnsHeader;

typedef struct __attribute__((packed)) DnsResponse {
    uint16_t NAME;
    uint16_t TYPE;
    uint16_t CLASS;
    uint32_t TTL;
    uint16_t RDLENGTH;
    uint32_t RDATA;
} DnsResponse;

struct Credentials {
    char *ssid;
    char *passwd;
} typedef Credentials;

class Network {

public:

    static Network &instance();

    Network(const Network &) = default;

    Network &operator=(const Network &) = delete;

    esp_err_t startAP();

    esp_err_t startSTA(Credentials credentials);

    esp_err_t attemptSTA(Credentials credentials);

    wifi_ap_record_t scannedNetworks[AP_SCAN_MAX_AP]{};

    static esp_err_t scan();

    esp_err_t startPassiveAP();

private:

    int attemptNum = 0;




    Network();

    TaskHandle_t dnsTask{};


};


#endif //RADAR_NETWORK_H
