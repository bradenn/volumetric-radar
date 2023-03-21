//
// Created by Braden Nicholson on 2/6/23.
//


#include <string>
#include <sstream>
#include <esp_mac.h>
#include <lwip/sockets.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"
#include "server.h"
#include "socket.h"
#include "controller.h"
#include "settings.h"

static RingbufHandle_t buf_handle;

static char *generateMetadata() {
    auto srv = &Settings::instance();

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
    // Add compiled config metadata to the object
    const int sampleRate = 80000;
    cJSON_AddNumberToObject(obj, "base", CONFIG_vRADAR_BASE_FREQUENCY);
    cJSON_AddNumberToObject(obj, "xFov", CONFIG_vRADAR_FOV_X);
    cJSON_AddNumberToObject(obj, "yFov", CONFIG_vRADAR_FOV_Y);
    // Generate the ADC parameters
    auto adc = cJSON_CreateObject();
    cJSON_AddNumberToObject(adc, "window", BUFFER_SIZE);
    cJSON_AddNumberToObject(adc, "bits", ADC_BIT_WIDTH);
    cJSON_AddNumberToObject(adc, "samples", srv->sampling.samples);
    cJSON_AddNumberToObject(adc, "frequency", srv->sampling.frequency);
    cJSON_AddNumberToObject(adc, "prf", srv->sampling.prf);
    cJSON_AddNumberToObject(adc, "pulse", srv->sampling.pulse);
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

static esp_err_t configHandler(httpd_req_t *req) {
    // Send payload type header
    httpd_resp_set_type(req, "application/json");
    // Allow Cross-Origin Access
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");


    const int size = 1024;
    char buf[size];
    int i = httpd_req_recv(req, buf, size);
    if (i <= 0) {
        httpd_resp_send(req, "Bad Request", HTTPD_RESP_USE_STRLEN);
        // Send the status OK message
        httpd_resp_set_status(req, HTTPD_400);
    }

    auto s = Settings::instance();
    s.fromJson(buf);
    s.push();


    httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
    // Send the status OK message
    httpd_resp_set_status(req, HTTPD_200);
    // Return without error

    return ESP_OK;
}

static esp_err_t restart(httpd_req_t *req) {
    // Send payload type header
    httpd_resp_set_type(req, "application/json");
    // Allow Cross-Origin Access
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    esp_restart();

    // Just here for the line count
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
            cJSON_free(metadata);
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

const char hexTable[] = "0123456789ABCDEF";

void intArrayToHexString(const int *arr, int arrSize, char *outStr) {
    // Allocate space for the hex string
    const int chars = 3;
    int hexStrSize = BUFFER_SIZE * chars + 1;
    char hexStr[BUFFER_SIZE * chars + 1] = {};
    hexStr[hexStrSize - 1] = '\0'; // Null terminate the string
    // Convert each int in the array to hex and concatenate it onto the hex string
    int i;
    for (i = 0; i < arrSize; i++) {
        hexStr[i * chars] = hexTable[(arr[i] >> 8) & 0xF];
        hexStr[i * chars + 1] = hexTable[(arr[i] >> 4) & 0xF];
        hexStr[i * chars + 2] = hexTable[arr[i] & 0xF];
    }

    // Copy the hex string to the output string
    strcpy(outStr, hexStr);
}

bool generatePacket(char *dst, int buffer, int **data, int m, int n) {
    auto obj = cJSON_CreateObject();
    // Create an array to hold the individual buffers
    auto results = cJSON_CreateArray();
    // Allocate memory on the stack to contain the converted hex strings
    char hex_str[BUFFER_SIZE * 3 + 1];
    // For each channel, add a string of hex to the results array
    for (int i = 0; i < m; i++) {
        // Convert the pointer array into a hex buffer for transport
        if (data[i] == nullptr) {
            continue;
        }
        intArrayToHexString(data[i], BUFFER_SIZE, hex_str);
        // Add the string to the results array
        cJSON_AddItemToArray(results, cJSON_CreateString(hex_str));
    }
    // Add the results array to the JSON object
    cJSON_AddItemToObject(obj, "results", results);
    // Add the current time since system boot to the JSON object
    cJSON_AddItemToObject(obj, "time", cJSON_CreateNumber((double) esp_timer_get_time()));
    // Print the JSON array out into the stack array
    auto val = cJSON_PrintPreallocated(obj, dst, buffer, false);
    // Free all memory used to create the JSON object
    cJSON_Delete(obj);
    // If the JSON export operation fails, cleanup and quietly move on
    if (val == 0) {
        // Print out a warning for context
        printf("A null string has been created...\n");
        // Proceed back to the beginning of the innermost while loop
        return false;
    }
    return true;
}


static void callback(int *arr[5]) {
    if (fd < 0) {
        vTaskDelay(1);
        return;
    }
    httpd_ws_frame_t out_packer;
    memset(&out_packer, 0, sizeof(httpd_ws_frame_t));

//    auto obj = cJSON_CreateObject();
//    auto results = cJSON_CreateArray();
//    char hex_str[BUFFER_SIZE * 2 + 1];
//    for (int i = 0; i < 4; i++) {
//        intArrayToHexString(arr[i], BUFFER_SIZE, hex_str);
//        cJSON_AddItemToArray(results, cJSON_CreateString(hex_str));
//    }
//
//    cJSON_AddItemToObject(obj, "results", results);
//
//    cJSON_AddItemToObject(obj, "time", cJSON_CreateNumber((double) esp_timer_get_time()));
//
//    const int len = 1500; // Size of buffers in json
//    char out[len];
//
//    auto val = cJSON_PrintPreallocated(obj, out, len, false);
//    if (val == 0) {
//        cJSON_Delete(obj);
//        printf("A null string has been created...\n");
//        return;
//    }
    const int len = 5 * (BUFFER_SIZE * 8) + 512; // Size of buffers in json
    char out[len];
    if (!generatePacket(out, len, arr, 5, BUFFER_SIZE)) {
        printf("Packet generation failed.\n");
    }
    out_packer.payload = (uint8_t *) out;
    out_packer.len = strlen(out);
    out_packer.type = HTTPD_WS_TYPE_TEXT;


    auto ret = httpd_ws_send_frame_async(hd, fd, &out_packer);
    if (ret != ESP_OK) {
        printf("Send to bridge failed!\n");
        fd = -1;
        return;
    }

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

static const httpd_uri_t config = {
        .uri       = "/config",
        .method    = HTTP_POST,
        .handler   = configHandler,
        .is_websocket = false,
};

static const httpd_uri_t reboot = {
        .uri       = "/reboot",
        .method    = HTTP_POST,
        .handler   = restart,
        .is_websocket = false,
};


void watcher(void *arg) {
    while (1) {

        size_t size = -1;

        void *dat = xRingbufferReceive(buf_handle, &size, portMAX_DELAY);
        if (size < 0) {
//            vRingbufferReturnItem(buf_handle, dat);
            continue;
        }

        int **data = (int **) dat;

        callback(data);

        for (int i = 0; i < 5; ++i) {
            free(data[i]);
        }

        vRingbufferReturnItem(buf_handle, dat);
    }
}

void runServer(void *params) {

    uint8_t rx[200];

    while (1) {
        struct sockaddr_in dest{
                .sin_family = AF_INET,
                .sin_port = htons(5545),
                .sin_addr {
                        .s_addr = htonl(INADDR_ANY),
                },
        };

        int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
        if (sock < 0) {
            printf("Socket creation failed...\n");
            vTaskDelay(pdMS_TO_TICKS(1));
            continue;
        }

        printf("Socket Initialized...\n");

        struct timeval timeout;
        timeout.tv_sec = 3;
        timeout.tv_usec = 0;
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof timeout);

        int err = bind(sock, (struct sockaddr *) &dest, sizeof(dest));
        if (err < 0) {
            printf("Socket bind failed... (%d)\n", err);
            vTaskDelay(pdMS_TO_TICKS(1));
            continue;
        }

        err = listen(sock, 1);
        if (err != 0) {
            printf("listen failed... (%d)\n", err);
            vTaskDelay(pdMS_TO_TICKS(1));
            continue;
        }


        printf("Socket Bound...\n");
        int keepAlive = 1;
        int keepIdle = 1;
        int keepInterval = 1;
        int keepCount = 1;
        int64_t begin = esp_timer_get_time();
        int64_t cycles = 0;
        while (1) {
            struct sockaddr_in sockAddr;
            socklen_t sockLen = sizeof(sockAddr);
            // Wait for a client to connect and send a message
            int local = accept(sock, (struct sockaddr *) &sockAddr, &sockLen);
            if (local < 0) {
                vTaskDelay(1);
                continue;
            }

            setsockopt(local, SOL_SOCKET, SO_KEEPALIVE, &keepAlive, sizeof(int));
            setsockopt(local, IPPROTO_TCP, TCP_KEEPIDLE, &keepIdle, sizeof(int));
            setsockopt(local, IPPROTO_TCP, TCP_KEEPINTVL, &keepInterval, sizeof(int));
            setsockopt(local, IPPROTO_TCP, TCP_KEEPCNT, &keepCount, sizeof(int));

            recv(sock, rx, sizeof(rx) - 1, 0);

            auto metadata = generateMetadata();
            err = send(local, metadata, strlen(metadata), MSG_WAITALL);
            // If there was an error sending the buffer, break out of this loop and force reconnect
            if (err < 0) {
                printf("Send failed. (%d) %d\n", err, errno);
                break;
            }
            cJSON_free(metadata);

            // Run indefinitely while the client is connected
            while (1) {
                size_t size;
                // Try to pull the next item from the ring, wait until something arrives
                void *dat = xRingbufferReceive(buf_handle, &size, portMAX_DELAY);
                // If there was an issue with the ring, wait a tick and try again
                if (dat == nullptr) {
//                    vTaskDelay(1);
                    continue;
                }
                // Cast the ring data to the double pointer array
                int **data = (int **) dat;
//                int start = esp_timer_get_time();
                // Create a new JSON object to contain the outgoing buffers
                const int len = 4 * (BUFFER_SIZE * 2) + 512; // Size of buffers in json
                char out[len];
                if (!generatePacket(out, len, data, 4, BUFFER_SIZE)) {
                    printf("Packet generation failed.\n");
                }
                // Free all the channel buffers
                for (int i = 0; i < 4; ++i) {
                    if (data[i] != nullptr) free(data[i]);
                }
                vRingbufferReturnItem(buf_handle, dat);
                // Try sending the JSON buffer to the client
                int rawLength = (int) strlen(out);
                int messageLength = rawLength + 1;
                out[rawLength] = '\0';
                int toWrite = messageLength;
                bool willExit = false;
                while (toWrite > 0) {
                    int n = send(local, out + (messageLength - toWrite), toWrite, 0);
                    if (n < 0) {
                        willExit = true;
                        break;
                    }
                    toWrite -= n;
                }
                if (willExit) {
                    break;
                }
                cycles++;
                int64_t now = esp_timer_get_time();
                if (now - begin > 1000 * 1000) {
                    printf("%.2f packets/s\n", (double) cycles / ((double) (now - begin) / 1000.0 / 1000.0));
                    begin = now;
                    cycles = 0;
                }
            }
            printf("TCP Socket lost...\n");
            close(local);

        }
        close(sock);

    }

}


Server::Server() {

    if (server != nullptr) {
        return;
    }

    server = nullptr;
    httpd_config_t httpdConf = HTTPD_DEFAULT_CONFIG();

    esp_err_t ret = httpd_start(&server, &httpdConf);
    if (ESP_OK != ret) {
        return;
    }

    Settings *s = &Settings::instance();
    httpd_register_uri_handler(server, &config);
    httpd_register_uri_handler(server, &reboot);
    httpd_register_uri_handler(server, &options);
    httpd_register_uri_handler(server, &socket_get);
    AdcConfig conf = {
            .sampling{
                    .rate = s->sampling.frequency,
                    .subSamples = s->sampling.samples,
                    .attenuation = ADC_ATTEN_DB_0
            },
            .channels {
                    .count = 4,
                    .ports = {3, 4, 5, 6}
            }
    };


    //Create ring buffer

    buf_handle = xRingbufferCreate((sizeof(int *) * 5) * 16, RINGBUF_TYPE_NOSPLIT);
    if (buf_handle == nullptr) {
        printf("Failed to create ring buffer\n");
    }

    adc = new Adc(conf, buf_handle);
    new Controller(adc, s->sampling.prf, s->sampling.pulse);
    xTaskCreatePinnedToCore(watcher, "adcWatch", 4096 * 4, nullptr, tskIDLE_PRIORITY + 4, nullptr, 0);

}
