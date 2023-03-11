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

Server *srv;
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
    auto conf = srv->adc->getConfig();
    // Add compiled config metadata to the object
    const int sampleRate = 80000;
    cJSON_AddNumberToObject(obj, "base", CONFIG_vRADAR_BASE_FREQUENCY);
    cJSON_AddNumberToObject(obj, "xFov", CONFIG_vRADAR_FOV_X);
    cJSON_AddNumberToObject(obj, "yFov", CONFIG_vRADAR_FOV_Y);
    // Generate the ADC parameters
    auto adc = cJSON_CreateObject();
    cJSON_AddNumberToObject(adc, "samples", 2);
    cJSON_AddNumberToObject(adc, "window", BUFFER_SIZE);
    cJSON_AddNumberToObject(adc, "bits", ADC_BIT_WIDTH);
    cJSON_AddNumberToObject(adc, "frequency", 40000);
    cJSON_AddNumberToObject(adc, "pulse", 1000);
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

static void callback(int *arr[4], int opt[4], int n, int m) {
    if (fd < 0) return;
    httpd_ws_frame_t out_packer;
    memset(&out_packer, 0, sizeof(httpd_ws_frame_t));

    // Add a status key to the object
    auto lo = cJSON_CreateObject();
    for (int i = 0; i < 4; i++) {
        cJSON_AddItemToObject(lo, std::to_string(i).c_str(), cJSON_CreateIntArray(arr[i], BUFFER_SIZE));
    }

    cJSON_AddItemToObject(lo, "updates", cJSON_CreateNumber(m));

    auto val = cJSON_PrintUnformatted(lo);
    // Delete the  object
    cJSON_Delete(lo);

    out_packer.payload = (uint8_t *) val;
    out_packer.len = strlen(val);
    out_packer.type = HTTPD_WS_TYPE_TEXT;

    auto ret = httpd_ws_send_frame_async(hd, fd, &out_packer);
    if (ret != ESP_OK) {
        printf("Send to bridge failed!\n");
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





//        const int numBuffers = 4;
//        bool ready = true;
//        int *bufs[numBuffers];
//        int out[numBuffers];
//        for (int i = 0; i < numBuffers; ++i) {
//            Buffer *buf = adc->buffers[i];
//            if (buf != nullptr) {
//                bufs[i] = buf->frontBuffer();
//            } else {
//                ready = false;
//                break;
//            }
//
//        }
//
//        if (ready) {
//            callback(bufs, out, 4, 4);
//            for (int i = 0; i < numBuffers; ++i) {
//                Buffer *buf = adc->buffers[i];
//                buf->popBuffer();
//            }
//        } else {
//            vTaskDelay(1);
//        }


        int *ch0 = adc->buffers[0]->frontBuffer();
        int *ch1 = adc->buffers[1]->frontBuffer();
        int *ch2 = adc->buffers[2]->frontBuffer();
        int *ch3 = adc->buffers[3]->frontBuffer();
        if (ch0 != nullptr && ch1 != nullptr && ch2 != nullptr && ch3 != nullptr) {
            int *bufs[4] = {ch0, ch1, ch2, ch3};
            int *opt[4] = {nullptr};
            callback(bufs, reinterpret_cast<int *>(opt), 4, 4);
            adc->buffers[0]->popBuffer();
            adc->buffers[1]->popBuffer();
            adc->buffers[2]->popBuffer();
            adc->buffers[3]->popBuffer();


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

    AdcConfig conf = {
            .sampling{
                    .rate = 80000,
                    .subSamples = 4,
                    .attenuation = ADC_ATTEN_DB_0
            },
            .channels {
                    .count = 4,
                    .ports = {3, 4, 6, 5}
            }
    };
    adc = new Adc(conf);
    srv = this;
    new Controller(adc);

//    xTaskCreatePinnedToCore(fco, "fco", 4096 * 2, adc, 7, nullptr, 1);
//    xTaskCreatePinnedToCore(adcThread, "adc", 4096 * 2, adc, 5, nullptr, 1);
    xTaskCreatePinnedToCore(watcher, "adcWatch", 4096 * 4, adc, 4, nullptr, 1);
}
