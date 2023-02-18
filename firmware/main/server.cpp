//
// Created by Braden Nicholson on 2/6/23.
//


#include <cmath>
#include <string>
#include <sstream>
#include "server.h"
#include "dsps_fft2r.h"
#include "dsps_wind_hann.h"
#include "socket.h"

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
            // Parse the text as JSON data
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

//#define N_SAMPLES BUFFER_SIZE
//int N = N_SAMPLES;
//// Input test array
//__attribute__((aligned(16)))
//float x1[N_SAMPLES];
//// Window coefficients
//__attribute__((aligned(16)))
//float wind[N_SAMPLES];
//// working complex array
//__attribute__((aligned(16)))
//float y_cf[N_SAMPLES * 2];
//// Pointers to result arrays
//__attribute__((aligned(16)))
//float *y1_cf = &y_cf[0];
//static Socket *socket = nullptr;

static void callback(int *arr, int *arr2, int n, int m) {
    if (fd < 0) return;
    httpd_ws_frame_t out_packer;
    memset(&out_packer, 0, sizeof(httpd_ws_frame_t));
//
//    esp_err_t err;
//    err = dsps_fft2r_init_fc32(nullptr, N);
//    if (err != ESP_OK) {
//        printf("Failed to init FFT!\n");
//        return;
//    }
//    // Generate Hann window
//    dsps_wind_hann_f32(wind, N);
//    // Convert two input vectors to one complex vector
//    for (int i = 0; i < N; i++) {
//        y_cf[i * 2 + 0] = (float) arr[i];
//        y_cf[i * 2 + 1] = 0;
//    }
//
//    dsps_fft2r_fc32(y_cf, N);
//    // Bit reverse
//    dsps_bit_rev_fc32(y_cf, N);
//    // Convert one complex vector to two complex vectors
//    dsps_cplx2reC_fc32(y_cf, N);
//
//    for (int i = 0 ; i < N/2 ; i++) {
//        y_cf[i] = 10 * log10f((y_cf[i * 2 + 0] * y_cf[i * 2 + 0] + y_cf[i * 2 + 1] * y_cf[i * 2 + 1])/(float)N);
//    }

    std::ostringstream oss("");
    for (int i = 0; i < BUFFER_SIZE; i += 2) {
        uint16_t v1 = arr[i];
        uint16_t v2 = arr[i + 1];
        uint32_t packed_values = ((v1 << 16) | v2);
        std::string hex_string = uint32_to_hex_string(packed_values);
        oss << hex_string;
    }

    // Add a status key to the object
    auto lo = cJSON_CreateObject();
//    if (socket != nullptr) {
//        socket->broadcast(oss.str().c_str(), 0);
//
//    }
    cJSON_AddItemToObject(lo, "payload", cJSON_CreateString(oss.str().c_str()));
    cJSON_AddItemToObject(lo, "waiting", cJSON_CreateNumber(m));

    auto val = cJSON_PrintUnformatted(lo);
    // Delete the  object
    cJSON_Delete(lo);
//
//    free(out);
    out_packer.payload = (uint8_t *) val;
    out_packer.len = strlen(val);
    out_packer.type = HTTPD_WS_TYPE_TEXT;
    auto ret = httpd_ws_send_frame_async(hd, fd, &out_packer);
    if (ret != ESP_OK) {
        printf("Frame send failed!\n");
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


void adcThread(void *arg) {
    Adc *adc = (Adc *) arg;
    adc->begin();
}

void watcher(void *arg) {
    Adc *adc = (Adc *) arg;
    while (1) {
        if (adc == nullptr) {
            vTaskDelay(1);
            continue;
        }
        int *ch0 = adc->buffers[0]->frontBuffer();

//        int *ch1 = adc->buffers[1]->frontBuffer();

        if (ch0 != nullptr) {
            callback(ch0, nullptr, BUFFER_SIZE, adc->buffers[0]->numBuffers());
            adc->buffers[0]->popBuffer();
//            adc->buffers[1]->popBuffer();
        }
        vTaskDelay(1);
    }
}
//
//void socketWatch(void *arg) {
//    Socket *sock = (Socket *) arg;
//    while(1) {
//    sock->configureUDP();
//
//    }
//}



Server::Server() {

    if (server != nullptr) {
        return;
    }

//    socket = new Socket();
//    xTaskCreate(socketWatch, "socketRun", 4096 * 2, socket, 2, nullptr);

    server = nullptr;
    config = HTTPD_DEFAULT_CONFIG();

    esp_err_t ret = httpd_start(&server, &config);
    if (ESP_OK != ret) {
        return;
    }


    httpd_register_uri_handler(server, &options);
    httpd_register_uri_handler(server, &socket_get);
    adc = new Adc();
    xTaskCreatePinnedToCore(adcThread, "adc", 4096 * 2, adc, 5, nullptr, 1);
    xTaskCreatePinnedToCore(watcher, "adcWatch", 4096 * 4, adc,2, nullptr, 1);


}
