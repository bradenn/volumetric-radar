//
// Created by Braden Nicholson on 2/1/23.
//


#include <esp_mac.h>
#include <string>
#include <lwip/sockets.h>
#include <esp_http_server.h>
#include <sstream>
#include <sys/param.h>
#include <cJSON.h>
#include <vector>
#include <freertos/event_groups.h>
#include "network.h"
#include <lwip/netdb.h>
#include "runtime.h"

#define AP_HTML_SETUP_START "_binary_style_css_start"
#define AP_HTML_SETUP_END "_binary_style_css_end"
static EventGroupHandle_t s_wifi_event_group;
#define NETWORK_WIFI_RECONNECTS 10
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static bool Connected = false;


using std::string;


void useJSON(httpd_req_t *req) {
    // Send payload type header
    httpd_resp_set_type(req, "application/json");
    // Allow Cross-Origin Access
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
}

/**
  * @brief Fetch the device UUID
  *
  * @param     ref  An array to copy the device UUID string into
  *
  * @return
  *    - ESP_OK: succeed
  */
void generateApName(uint8_t ref[32]) {
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf((char *) ref, 12, "vRadar %02x%02x", mac[1], mac[4]);
}

/**
  * @brief Handle list endpoint, sends a JSON array of available Wi-Fi networks
  *
  * @return
  *    - ESP_OK: succeed
  */
static esp_err_t listJSON(httpd_req_t *req) {
    // Set the headers and expected content type to JSON
    useJSON(req);
    // Create a JSON structure to hold the data
    auto obj = cJSON_CreateObject();
    // Get a local instance of the network
    Network *n = &Network::instance();
    // Fetch the unique device name
    uint8_t ref[32];
    generateApName(ref);
    // Add the unique name to the JSON object
    cJSON_AddItemToObject(obj, "module", cJSON_CreateString((char *) &ref));
    // Initialize an array to contain the available networks
    auto arr = cJSON_CreateArray();
    for (const auto &item: n->scannedNetworks) {
        // Initialize an object for this item in the array
        auto local = cJSON_CreateObject();
        // Set the network name of the object
        cJSON_AddItemToObject(local, "ssid", cJSON_CreateString((const char *) item.ssid));
        // Set the signal strength of the network
        cJSON_AddItemToObject(local, "rssi", cJSON_CreateNumber(item.rssi));
        // Add the item to the array
        cJSON_AddItemToArray(arr, local);
    }
    // Add the array to the object
    cJSON_AddItemToObject(obj, "networks", arr);
    // Print the structure out to a string to be sent to the client
    char *json = cJSON_PrintUnformatted(obj);
    // Cleanup the resources used to create the JSON object
    cJSON_Delete(obj);
    // Attempt to send the data to the client
    esp_err_t err;
    err = httpd_resp_sendstr(req, json);
    if (err != ESP_OK) {
        // Emergency cleanup of the json string
        free(json);
        httpd_resp_set_status(req, HTTPD_500);
        return err;
    }
    // Send a status OK code if all is well
    err = httpd_resp_set_status(req, HTTPD_200);
    if (err != ESP_OK) {
        return err;
    }
    // Cleanup the json string from memory
    free(json);
    return ESP_OK;
}

/**
  * @brief Handle connect endpoint, attempts to connect to network provided
  *
  * @return
  *    - ESP_OK: succeed
  */
static esp_err_t connectJSON(httpd_req_t *req) {
    // Set the headers and expected content type to JSON
    useJSON(req);
    // Allocate memory to contain the incoming request buffer
    char *requestBuffer = (char *) calloc(req->content_len, sizeof(char));
    // Make sure the content_len represents the correct number of bytes received
    size_t requestSize = MIN(req->content_len, sizeof(requestBuffer));
    // Attempt to read those bytes into the buffer
    int ret = httpd_req_recv(req, requestBuffer, requestSize);
    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }
    // Parse the body as JSON
    cJSON *requestData = cJSON_Parse(requestBuffer);
    // Extract the desired SSID from the JSON structure
    char *ssid = cJSON_GetObjectItem(requestData, "ssid")->valuestring;
    // Extract the password from the structure
    char *passwd = cJSON_GetObjectItem(requestData, "passwd")->valuestring;

    auto obj = cJSON_CreateObject();
    Network *n = &Network::instance();
    bool success = true;
    esp_err_t err = n->attemptSTA({.ssid = ssid, .passwd = passwd});
    if (err != ESP_OK) {
        success = false;
        // Add the error message to the object
        cJSON_AddItemToObject(obj, "error", cJSON_CreateString(esp_err_to_name(err)));
    }
    // Add the success boolean to the out object
    cJSON_AddItemToObject(obj, "success", cJSON_CreateBool(success));

    // Clean up the request data before trying to send which may require premature exiting

    // Free the JSON structure to prevent leaks
    cJSON_Delete(requestData);
    // Free the requestBuffer memory to prevent leaks
    free(requestBuffer);
    // Marshal the json to a string
    char *json = cJSON_PrintUnformatted(obj);
    // Attempt to send the data to the client
    err = httpd_resp_sendstr(req, json);
    if (err != ESP_OK) {
        // Emergency cleanup of the json string
        free(json);
        httpd_resp_set_status(req, HTTPD_500);
        return err;
    }
    // Send a status OK code if all is well
    err = httpd_resp_set_status(req, HTTPD_200);
    if (err != ESP_OK) {
        return err;
    }

    return ESP_OK;
}

static esp_err_t connectIndex(httpd_req_t *req) {
    // Send payload type header
    httpd_resp_set_type(req, "text/html");
    // Allow Cross-Origin Access
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    httpd_resp_sendstr_chunk(req, AP_HTML_HEADER);
    size_t querySize = httpd_req_get_url_query_len(req);
    char *query = (char *) malloc(sizeof(char) * querySize + 1);
    httpd_req_get_url_query_str(req, query, querySize + 1);
    char out[33];
    httpd_query_key_value(query, "ssid", out, querySize);

    char pass[33];
    httpd_resp_sendstr_chunk(req, AP_HTML_HEADER);
    httpd_resp_send_chunk(req, "<style>", (ssize_t) strlen("<style>"));
    extern const unsigned char upload_script_start[] asm(AP_HTML_SETUP_START);
    extern const unsigned char upload_script_end[]   asm(AP_HTML_SETUP_END);
    const size_t upload_script_size = (upload_script_end - upload_script_start);
    httpd_resp_send_chunk(req, (const char *) upload_script_start, (ssize_t) upload_script_size);
    httpd_resp_send_chunk(req, "</style>", (ssize_t) strlen("</style>"));
    std::ostringstream oss("");
    oss << R"(<meta name="viewport" content="width=device-width, initial-scale=1.0"><div
                class="header"><h1>vRadar</h1>)";
    oss << "</div>";

    if (httpd_query_key_value(query, "passwd", pass, querySize) == ESP_ERR_NOT_FOUND) {
        oss << "<div class='title'>Enter the password for '" << out << "'</div>";
        oss << "<form action='/connect' method='get'> <input type='hidden' id='ssid' name='ssid' "
               "value='" << out << "'/><input type='password' id='passwd' name='passwd'></input>";
        oss << "<input type='submit' value='submit' id='submit' name='submit' value='Connect'/>";
        oss << "</form>";
    } else {
        Network *n = &Network::instance();
        esp_err_t e = n->attemptSTA({.ssid = out, .passwd = pass});
        if (e != ESP_OK) {
            oss << "<div class='title'>WRONG PASSWORD for '" << out << "'</div>";
        } else {
            oss << "<div class='title'>Connected to '" << out << "'</div>";
            oss << "<div>This network will now be disabled. Please continue from your selected network.</div>";
        }
    }

    oss << "<div class='center'>";
    oss << R"(<div class="subtext">vRadar )";
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char macOut[12];
    snprintf((char *) &macOut, 12, "%02x%02x", mac[1], mac[4]);
    oss << macOut;
    oss << "</div> Copyright &copy; Braden Nicholson 2023</div>";

    httpd_resp_sendstr_chunk(req, oss.str().c_str());
    httpd_resp_sendstr_chunk(req, AP_HTML_FOOTER);
    // Stop sending chunks
    httpd_resp_sendstr_chunk(req, nullptr);
    // Send the status OK code
    httpd_resp_set_status(req, HTTPD_200);
    free(query);
//    if(reboot) {
//        esp_restart();
//    }
    return ESP_OK;
}

static esp_err_t portalIndex(httpd_req_t *req) {
    // Send payload type header
    httpd_resp_set_type(req, "text/html");
    // Allow Cross-Origin Access
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    httpd_resp_sendstr_chunk(req, AP_HTML_HEADER);

    httpd_resp_send_chunk(req, "<style>", (ssize_t) strlen("<style>"));
    extern const unsigned char upload_script_start[] asm(AP_HTML_SETUP_START);
    extern const unsigned char upload_script_end[]   asm(AP_HTML_SETUP_END);
    const size_t upload_script_size = (upload_script_end - upload_script_start);
    /* Add file upload form and script which on execution sends a POST request to /upload */
    httpd_resp_send_chunk(req, (const char *) upload_script_start, (ssize_t) upload_script_size);
    httpd_resp_send_chunk(req, "</style>", (ssize_t) strlen("</style>"));

    std::ostringstream oss("");
    oss << R"(<meta name="viewport" content="width=device-width, initial-scale=1.0"><div
class="header"><h1>vRadar</h1>)";

    oss << "</div>";

    oss << "<div class='title'>Select A Wi-Fi Network</div>";
    oss << "<div class='menu'>";
    std::vector<uint8_t *> used;
    for (auto &item: wifi_records) {
        bool skip = false;
        for (auto ref: used) {
            if (strcmp((char *) (&ref), (char *) (&item.bssid)) == 0) {
                skip = true;
                break;
            }
        }
        if (skip) {
            continue;
        } else {
            used.push_back(item.bssid);
        }
        oss << R"(<div class="menu-bar"><div class="menu-option">)";
        oss << "<div>";
        oss << item.ssid;
        oss << "</div>";
        oss << "<div>";
        oss << (int) item.rssi;
        oss << " dB</div>";
        oss << "</div>";
        oss << "<a href='/connect?ssid=" << item.ssid << "'><div><i class=\"gg-chevron-right\" style='opacity:0.8;"
                                                         "'></i></div></a>";

        oss << "</div>";
    }
    oss << "</div> <div class='center'>";

    oss << R"(<div class="subtext">vRadar )";
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char macOut[12];
    snprintf((char *) &macOut, 12, "%02x%02x", mac[1], mac[4]);
    oss << macOut;
    oss << "</div> Copyright &copy; Braden Nicholson 2023</div>";

    httpd_resp_sendstr_chunk(req, oss.str().c_str());
    httpd_resp_sendstr_chunk(req, AP_HTML_FOOTER);
    // Stop sending chunks
    httpd_resp_sendstr_chunk(req, nullptr);
    // Send the status OK code
    httpd_resp_set_status(req, HTTPD_200);
    return ESP_OK;
}

static const httpd_uri_t jsonGetList = {
        .uri       = "/list",
        .method    = HTTP_GET,
        .handler   = listJSON,
        .user_ctx = nullptr,
};

static const httpd_uri_t jsonGetConnect = {
        .uri       = "/connect",
        .method    = HTTP_POST,
        .handler   = connectJSON,
        .user_ctx = nullptr,
};


static const httpd_uri_t portalGet = {
        .uri       = "/",
        .method    = HTTP_GET,
        .handler   = portalIndex,
        .user_ctx = nullptr,
};

static const httpd_uri_t connectGet = {
        .uri       = "/connect",
        .method    = HTTP_GET,
        .handler   = connectIndex,
        .user_ctx = nullptr,
};

static const httpd_uri_t hotspotGet = {
        .uri       = "/hotspot-detect.html",
        .method    = HTTP_GET,
        .handler   = portalIndex,
        .user_ctx = nullptr,
};


void httpServer(void *params) {


    httpd_handle_t server;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    esp_err_t ret = httpd_start(&server, &config);
    if (ESP_OK != ret) {
        printf("Failed to start server.");
        return;
    }

    httpd_register_uri_handler(server, &portalGet);
    httpd_register_uri_handler(server, &connectGet);
    httpd_register_uri_handler(server, &jsonGetConnect);
    httpd_register_uri_handler(server, &jsonGetList);
    httpd_register_uri_handler(server, &hotspotGet);

}

ip4_addr_t resolve_ip;

void dnsCleanup(int socket) {
    if (socket != -1) {
        shutdown(socket, 0);
        close(socket);
        socket = -1;
    }
}

#define SRV_HEADER_SIZE           12
#define SRV_QUESTION_MAX_LENGTH   50

void dnsServer(void *params) {

    uint8_t rx[128];

    while (1) {
        struct sockaddr_in dest{
                .sin_family = AF_INET,
                .sin_port = htons(53),
                .sin_addr {
                        .s_addr = htonl(INADDR_ANY),
                },
        };

        int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);

        if (sock < 0) {
            printf("Socket creation failed...\n");
            break;
        }
        int err = bind(sock, (struct sockaddr *) &dest, sizeof(dest));
        if (err < 0) {
            printf("Socket bind failed... (%d)\n", err);
            break;
        }

        while (1) {
            struct sockaddr_in sockAddr;
            socklen_t sockLen = sizeof(sockAddr);

            memset(rx, 0x00, sizeof(rx));

            int len = recvfrom(sock, rx, sizeof(rx), 0, (struct sockaddr *) &sockAddr, &sockLen);

            if (len < 0) {
                printf("Socket read failed.\n");
                break;
            }

            if (len > SRV_HEADER_SIZE + SRV_QUESTION_MAX_LENGTH) {
                printf("Read overflow\n");
                continue;
            }

            rx[sizeof(rx) - 1] = '\0';

            auto *t = (DnsHeader *) rx;

            t->QR = 1;
            t->OPCODE = 0;
            t->AA = 1;
            t->RCODE = 0;
            t->TC = 0;
            t->Z = 0;
            t->RA = 0;
            t->ANCOUNT = t->QDCOUNT;
            t->NSCOUNT = 0;
            t->ARCOUNT = 0;

            uint8_t *query = rx + sizeof(DnsHeader);

            // Skip QNAME
            while (*query++);

            // Skip QTYPE
            query += 2;

            // Skip QCLASS
            query += 2;

            auto *response = (DnsResponse *) query;

            response->NAME = __bswap16(0xC00C);
            response->TYPE = __bswap16(1);
            response->CLASS = __bswap16(1);
            response->TTL = 0;
            response->RDLENGTH = __bswap16(4);
            response->RDATA = resolve_ip.addr;

            query += sizeof(DnsResponse);
            err = sendto(sock, rx, query - rx, 0, (struct sockaddr *) &sockAddr, sizeof(sockAddr));
            if (err < 0) {
                printf("Send failed. (%d)\n", err);
                break;
            }
            taskYIELD();
        }
        dnsCleanup(sock);

    }

}

static int reconnectAttempts = 0;

#define MAC_STR_FMT "%02x:%02x:%02x:%02x:%02x:%02x"
#define IP_STR_FMT "%d.%d.%d.%d"

static void wifi_event_handler(void *arg, esp_event_base_t eventBase, int32_t eventId, void *event_data) {
    if (eventBase == WIFI_EVENT) {
        switch (eventId) {
            case WIFI_EVENT_STA_START: {
                esp_err_t err = esp_wifi_connect();
                if (err != ESP_OK) {
                    printf("Wi-Fi Connect failed! %s\n", esp_err_to_name(err));
                } else {
                    printf("Wi-Fi Station mode activated...\n");

                }
                break;
            }
            case WIFI_EVENT_STA_CONNECTED:
                printf("Connected to network!\n");
            case WIFI_EVENT_STA_DISCONNECTED:
                printf("Disconnected!\n");
                if (reconnectAttempts < NETWORK_WIFI_RECONNECTS) {
                    esp_wifi_connect();
                    reconnectAttempts++;
                    printf("Attempting to reconnect... (%d/%d)\n", reconnectAttempts, NETWORK_WIFI_RECONNECTS);
                } else {
                    xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
                }
                break;
            case WIFI_EVENT_AP_STACONNECTED: {
                auto *apEvent = (wifi_event_ap_staconnected_t *) event_data;
                printf("[AP] Device (" MAC_STR_FMT ") connected.\n", MAC2STR(apEvent->mac));
                break;
            }
            case WIFI_EVENT_AP_STADISCONNECTED: {
                auto *apEvent = (wifi_event_ap_stadisconnected_t *) event_data;
                printf("[AP] Device (" MAC_STR_FMT ") disconnected.\n", MAC2STR(apEvent->mac));
                break;
            }
            default:
                break;
        }
    } else if (eventBase == IP_EVENT) {
        switch (eventId) {
            case IP_EVENT_STA_LOST_IP: {
                printf("Unassigned IP...\n");
                break;
            }
            case IP_EVENT_STA_GOT_IP: {
                auto *ipEvent = (ip_event_got_ip_t *) event_data;
                printf("Assigned DHCP IP: (" IP_STR_FMT ") \n", IP2STR(&ipEvent->ip_info.ip));
                reconnectAttempts = 0;
                xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
                break;
            }
            default:
                break;
        }
    }
}

Network::Network() {
    // Create a soft access point
    esp_netif_create_default_wifi_ap();
    // Create station mode Wi-Fi connection
    esp_netif_create_default_wifi_sta();

    // Configure the system with the default modem settings
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    if (esp_wifi_init(&cfg) != ESP_OK) {
        printf("Wi-Fi initialization failed.\n");
        return;
    }
//    wifiEventGroup = xEventGroupCreate();
    printf("Network initialized...\n");

}

Network &Network::instance() {
    static Network the_instance{};
    return the_instance;
}

/**
  * @brief Scan for Wi-Fi networks which have WPA2_PSK
  *
  *
  * @return
  *    - ESP_OK: succeed
  *    - ESP_ERR_WIFI_NOT_INIT: WiFi is not initialized by esp_wifi_init
  *    - ESP_ERR_INVALID_ARG: invalid argument
  */
esp_err_t Network::scan() {
    // Register the event handler function
    wifi_scan_config_t scan_config = {
            .ssid = 0,
            .bssid = 0,
            .channel = 0,
            .show_hidden = false,
    };

    esp_err_t err;
    // Begin a blocking scan for Wi-Fi networks
    err = esp_wifi_scan_start(&scan_config, true);
    if (err != ESP_OK) {
        return err;
    }
    // Get the records from the internal memory and copy it to the Network class.
    uint16_t max_records = AP_SCAN_MAX_AP;
    err = esp_wifi_scan_get_ap_records(&max_records, wifi_records);
    if (err != ESP_OK) {
        return err;
    }


    return ESP_OK;
}

/**
  * @brief Attempt to connect to a Wi-Fi network
  *
  *
  * @return
  *    - ESP_OK: succeed
  *    - ESP_ERR_WIFI_NOT_INIT: WiFi is not initialized by esp_wifi_init
  *    - ESP_ERR_INVALID_ARG: invalid argument
  */
esp_err_t Network::attemptSTA(Credentials credentials) {
    wifi_config_t staConfig = {
            .sta = {
                    .threshold = {
                            .authmode = WIFI_AUTH_WPA2_PSK,
                    },
            },
    };
    strcpy((char *) staConfig.sta.ssid, credentials.ssid);
    strcpy((char *) staConfig.sta.password, credentials.passwd);
    esp_err_t err;
    err = esp_wifi_set_config(WIFI_IF_STA, &staConfig);
    if (err != ESP_OK) {
        return err;
    }
    // Attempt tp connect to target network
    err = esp_wifi_connect();
    if (err != ESP_OK) {
        return err;
    }
    printf("Connected to '%s'\n", credentials.ssid);
    auto p = &Persistent::instance();
    p->writeString("ssid", credentials.ssid);
    p->writeString("passwd", credentials.passwd);

    return ESP_OK;
}

/**
  * @brief Connect to a Wi-Fi network
  *
  *
  * @return
  *    - ESP_OK: succeed
  *    - ESP_ERR_WIFI_NOT_INIT: WiFi is not initialized by esp_wifi_init
  *    - ESP_ERR_INVALID_ARG: invalid argument
  */
esp_err_t Network::startSTA(Credentials credentials) {
    s_wifi_event_group = xEventGroupCreate();
    // Configure the wifi_event_handler function to handle WI-FI and IP events
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, nullptr, nullptr);
    esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, nullptr, nullptr);

    wifi_config_t staConfig = {
            .sta = {
                    .threshold = {
                            .authmode = WIFI_AUTH_WPA2_PSK,
                    },
            },
    };

    // Copy the password from the credential struct to the station mode config
    strcpy((char *) staConfig.sta.ssid, credentials.ssid);
    strcpy((char *) staConfig.sta.password, credentials.passwd);

    esp_err_t err;
    // Set the system Wi-Fi module to operate in access point mode
    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK) {
        return err;
    }
    err = esp_wifi_set_config(WIFI_IF_STA, &staConfig);
    if (err != ESP_OK) {
        return err;
    }
    // Open the access point if needed
    err = esp_wifi_start();
    if (err != ESP_OK) {
        return err;
    }
    // Wait for the
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        printf("Connected to '%s'\n", credentials.ssid);
        return ESP_OK;
    } else if (bits & WIFI_FAIL_BIT) {
        printf("Connection failed after %d attempts\n", NETWORK_WIFI_RECONNECTS);
    } else {
        printf("Unknown WIFI Event\n");
    }

    return ESP_OK;
}

/**
  * @brief Initialize & Start on-board access point
  *
  *
  * @return
  *    - ESP_OK: succeed
  *    - ESP_ERR_WIFI_NOT_INIT: WiFi is not initialized by esp_wifi_init
  *    - ESP_ERR_INVALID_ARG: invalid argument
  */
esp_err_t Network::startAP() {
    // Define the parameters for the access point
    wifi_config_t apConfig = {
            .ap = {
                    .password = AP_PASSWD,
                    .channel = AP_CHANNEL,
                    .authmode = WIFI_AUTH_OPEN,
                    .max_connection = AP_MAX_CONN,
            }
    };
    // Define the parameters for station mode
    wifi_config_t staConfig = {
            .sta = {
                    .scan_method = WIFI_FAST_SCAN,
                    // Sort the scanned networks by signal strength
                    .sort_method = WIFI_CONNECT_AP_BY_SIGNAL,
                    .threshold = {
                            // Allow very low strength networks to be found
                            .rssi = -127,
                            // Require searched networks have password protection
                            .authmode = WIFI_AUTH_WPA2_PSK,
                    }
            },
    };
    // Set the AP name to reflect the device's unique mac address
    generateApName(apConfig.ap.ssid);

    esp_err_t err;
    // Set the system Wi-Fi module to operate in access point mode
    err = esp_wifi_set_mode(WIFI_MODE_APSTA);
    if (err != ESP_OK) {
        return err;
    }
    // Pass the configuration to the Wi-Fi subsystems
    err = esp_wifi_set_config(WIFI_IF_AP, &apConfig);
    if (err != ESP_OK) {
        return err;
    }
    // Pass the configuration to the Wi-Fi subsystems
    err = esp_wifi_set_config(WIFI_IF_STA, &staConfig);
    if (err != ESP_OK) {
        return err;
    }
    // Open the access point if needed
    err = esp_wifi_start();
    if (err != ESP_OK) {
        return err;
    }
    // Scan for open networks
    err = scan();
    if (err != ESP_OK) {
        return err;
    }
    inet_pton(AF_INET, "192.168.4.1", &resolve_ip);

    httpServer(nullptr);
    xTaskCreate(dnsServer, "dnsServer", 4096, nullptr, 5, nullptr);

    return ESP_OK;
}
