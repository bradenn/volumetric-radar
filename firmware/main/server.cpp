//
// Created by Braden Nicholson on 2/6/23.
//


#include <string>
#include <sstream>
#include <esp_mac.h>
#include <driver/gpio.h>
#include "server.h"
#include "socket.h"
#include "controller.h"

#define VCO_FREQUENCY (1000*2)
#define VCO_DURATION (1000*2)

static char *generateMetadata() {
    // Initialize a json object
    auto obj = cJSON_CreateObject();
    // Get the system mac address
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    // Generate the module human-readable name
    char name[32];
    snprintf((char *) name, 12, "vRadar %02x%02x", mac[1], mac[4]);
    // Generate the mac address string
    char macAddr[32];
    snprintf((char *) macAddr, 18, "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    // Add generated variables to the object
    cJSON_AddStringToObject(obj, "name", name);
    cJSON_AddStringToObject(obj, "mac", macAddr);
    // Add compiled config metadata to the object
    cJSON_AddNumberToObject(obj, "frequency", CONFIG_vRADAR_BASE_FREQUENCY);
    cJSON_AddNumberToObject(obj, "xFov", CONFIG_vRADAR_FOV_X);
    cJSON_AddNumberToObject(obj, "yFov", CONFIG_vRADAR_FOV_Y);
    // Generate the ADC parameters
    auto adc = cJSON_CreateObject();
    cJSON_AddNumberToObject(adc, "samples", ADC_CONV_FRAME);
    cJSON_AddNumberToObject(adc, "bits", ADC_BIT_WIDTH);
    cJSON_AddNumberToObject(adc, "frequency", ADC_FREQUENCY);
    cJSON_AddNumberToObject(adc, "vcoMs", VCO_FREQUENCY);
    // Add the adc object to the root objects
    cJSON_AddItemToObject(obj, "adc", adc);
    // Marshal the object to json as a string
    char *out = cJSON_PrintUnformatted(obj);
    // Free the memory used to create the object
    cJSON_Delete(obj);
    return out;
}

static esp_err_t options_handler(httpd_req_t *req) {
    // Send payload type header
    httpd_resp_set_type(req, "application/json");
    // Allow Cross-Origin Access
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
    // Send the status OK message
    httpd_resp_set_status(req, HTTPD_200);
    // Return without error
    return ESP_OK;
}

// Socket handler is the http method handler for requests made to the /ws endpoint
static esp_err_t socket_get_handler(httpd_req_t *req) {
    // If the connection is a http GET request, initialize a new connection
    if (req->method == HTTP_GET) {
        // Log the connection
        printf("Socket connected.\n");
        // Return from the handler
        return ESP_OK;
    }
    // Instantiate a websocket frame
    httpd_ws_frame_t ws_pkt;
    // Clear the packet
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    // Set the type to websocket text
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    // Check to make sure the frame is ready by receiving zero bytes
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
        return ret;
    }
    // If the frame is not empty
    if (ws_pkt.len) {
        // Allocate memory for the incoming buffer
        auto buffer = (uint8_t *) calloc(1, ws_pkt.len + 1);;
        // Set the packet payload pointer to the buffer
        ws_pkt.payload = buffer;
        // Receive the rest of the data
        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        if (ret != ESP_OK) {
            free(buffer);
            return ret;
        }
        // If the packet type remains text, parse it.
        if (ws_pkt.type == HTTPD_WS_TYPE_TEXT) {
            // Generate the metadata json payload
            char *metadata = generateMetadata();
            // Initialize a websocket frame
            httpd_ws_frame_t out_packer;
            memset(&out_packer, 0, sizeof(httpd_ws_frame_t));
            // Configure the packet as an outgoing text message
            out_packer.payload = (uint8_t *) metadata;
            out_packer.len = strlen(metadata);
            out_packer.type = HTTPD_WS_TYPE_TEXT;
            // Send the packet
            ret = httpd_ws_send_frame(req, &out_packer);
            if (ret != ESP_OK) {
                printf("Frame send failed!\n");
            }
            // Free the metadata string
            free(metadata);
            // Configure the session
            setSession(req);
            printf("Session started.\n");
        }
        // Free the buffer allocated earlier
        free(buffer);
    }
    // Return normally
    return 0;
}

std::string uint32_to_hex_string(uint32_t num) {
    char hex_string[9];
    snprintf(hex_string, 9, "%08lx", num);
    return std::string{hex_string};
}

static void callback(int *arr, int *arr2, int *arr3, int *arr4, int *arr5, int n, int m) {
    if (fd < 0) return;
    httpd_ws_frame_t out_packer;
    memset(&out_packer, 0, sizeof(httpd_ws_frame_t));

    std::ostringstream oss("");
    std::ostringstream oss1("");
    std::ostringstream oss2("");
    std::ostringstream oss3("");
    std::ostringstream oss4("");
    for (int i = 0; i < BUFFER_SIZE; i += 2) {
        uint16_t v1 = arr[i];
        uint16_t v2 = arr[i + 1];
        uint32_t packed_values = ((v1 << 16) | v2);
        std::string hex_string = uint32_to_hex_string(packed_values);
        oss << hex_string;
    }

    for (int i = 0; i < BUFFER_SIZE; i += 2) {
        uint16_t v1 = arr2[i];
        uint16_t v2 = arr2[i + 1];
        uint32_t packed_values = ((v1 << 16) | v2);
        std::string hex_string = uint32_to_hex_string(packed_values);
        oss1 << hex_string;
    }

    for (int i = 0; i < BUFFER_SIZE; i += 2) {
        uint16_t v1 = arr3[i];
        uint16_t v2 = arr3[i + 1];
        uint32_t packed_values = ((v1 << 16) | v2);
        std::string hex_string = uint32_to_hex_string(packed_values);
        oss2 << hex_string;
    }


    for (int i = 0; i < BUFFER_SIZE; i += 2) {
        uint16_t v1 = arr4[i];
        uint16_t v2 = arr4[i + 1];
        uint32_t packed_values = ((v1 << 16) | v2);
        std::string hex_string = uint32_to_hex_string(packed_values);
        oss3 << hex_string;
    }

    for (int i = 0; i < BUFFER_SIZE; i += 2) {
        uint16_t v1 = arr5[i];
        uint16_t v2 = arr5[i + 1];
        uint32_t packed_values = ((v1 << 16) | v2);
        std::string hex_string = uint32_to_hex_string(packed_values);
        oss4 << hex_string;
    }


    // Add a status key to the object
    auto lo = cJSON_CreateObject();

    cJSON_AddItemToObject(lo, "ch0", cJSON_CreateString(oss.str().c_str()));
    cJSON_AddItemToObject(lo, "ch1", cJSON_CreateString(oss1.str().c_str()));
    cJSON_AddItemToObject(lo, "ch2", cJSON_CreateString(oss2.str().c_str()));
    cJSON_AddItemToObject(lo, "ch3", cJSON_CreateString(oss3.str().c_str()));
    cJSON_AddItemToObject(lo, "ch4", cJSON_CreateString(oss4.str().c_str()));
    cJSON_AddItemToObject(lo, "updates", cJSON_CreateNumber(m));

    auto val = cJSON_PrintUnformatted(lo);
    // Delete the  object
    cJSON_Delete(lo);

    out_packer.payload = (uint8_t *) val;
    out_packer.len = strlen(val);
    out_packer.type = HTTPD_WS_TYPE_TEXT;

    auto ret = httpd_ws_send_frame_async(hd, fd, &out_packer);
    if (ret != ESP_OK) {
        printf("Frame send failed... Connection terminated!\n");
        free(val);
        fd = -1;
        return;
    }

    free(val);
}


static const httpd_uri_t socket_get = {
        .uri       = "/ws",
        .method    = HTTP_GET,
        .handler   = socket_get_handler,
        .is_websocket = true,
};

static const httpd_uri_t options = {
        .uri       = "/ws",
        .method    = HTTP_OPTIONS,
        .handler   = options_handler,
        .is_websocket = false,
};


void fco(void *arg) {


}


void watcher(void *arg) {
    Adc *adc = (Adc *) arg;
    while (1) {

        if (adc == nullptr) {
            vTaskDelay(1);
            continue;
        }

        int *ch0 = adc->buffers[0]->frontBuffer();
        int *ch1 = adc->buffers[1]->frontBuffer();
        int *ch2 = adc->buffers[2]->frontBuffer();
        int *ch3 = adc->buffers[3]->frontBuffer();
        int *ch4 = adc->buffers[4]->frontBuffer();

        if (ch0 != nullptr && ch1 != nullptr && ch2 != nullptr && ch3 != nullptr) {
            if (ch4 != nullptr) {
                callback(ch0, ch1, ch2, ch3, ch4, BUFFER_SIZE, (int) adc->getUpdatesPerSecond());
            } else {
                int dummy[10] = {};
                callback(ch0, ch1, ch2, ch3, dummy, BUFFER_SIZE, (int) adc->getUpdatesPerSecond());
            }
            adc->buffers[0]->popBuffer();
            adc->buffers[1]->popBuffer();
            adc->buffers[2]->popBuffer();
            adc->buffers[3]->popBuffer();
            if (ch4 != nullptr) {
                adc->buffers[4]->popBuffer();
            }
        } else {
            vTaskDelay(1);

        }

    }
}

Server::Server() {

    if (server != nullptr) {
        return;
    }

    server = nullptr;
    config = HTTPD_DEFAULT_CONFIG();

    esp_err_t ret = httpd_start(&server, &config);
    if (ESP_OK != ret) {
        return;
    }

    httpd_register_uri_handler(server, &options);
    httpd_register_uri_handler(server, &socket_get);

//    adc = new Adc();
    Controller cont = Controller();
    adc = cont.adc;
//    xTaskCreatePinnedToCore(fco, "fco", 4096 * 2, adc, 7, nullptr, 1);
//    xTaskCreatePinnedToCore(adcThread, "adc", 4096 * 2, adc, 5, nullptr, 1);
    xTaskCreate(watcher, "adcWatch", 4096 * 4, adc, 4, nullptr);
}
