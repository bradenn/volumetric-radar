//
// Created by Braden Nicholson on 2/6/23.
//


#include "server.h"

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

static void callback(int m, int n, int **arr) {
    if (fd < 0) return;
    httpd_ws_frame_t out_packer;
    memset(&out_packer, 0, sizeof(httpd_ws_frame_t));
    // Instantiate an object
    auto obj = cJSON_CreateArray();
    // Add a status key to the object
    for (int i = 0; i < m; ++i) {
        auto lo= cJSON_CreateObject();
        cJSON_AddItemToObject(lo, "values", cJSON_CreateIntArray(arr[i], n));
        cJSON_AddItemToArray(obj, lo);
    }
    auto val = cJSON_PrintUnformatted(obj);
    // Delete the  object
    cJSON_Delete(obj);

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


static void adcThread(void *arg) {
    auto adc = (Adc*) arg;
    adc->begin();
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

    auto adc = &Adc::instance();
    adc->hook(callback);
    xTaskCreate(adcThread, "ADC", 4096*2, adc, 2, nullptr);


}
