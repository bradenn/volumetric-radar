//
// Created by Braden Nicholson on 2/6/23.
//


#include <string>
#include <sstream>
#include <esp_mac.h>
#include <lwip/sockets.h>
#include <iomanip>
#include <hal/gpio_types.h>
#include <driver/temperature_sensor.h>
#include <driver/gptimer.h>
#include <esp_adc/adc_oneshot.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"
#include "server.h"
#include "settings.h"
#include "sample.h"
#include "dac.h"
#include "gyro.h"
#include "runtime.h"

static RingbufHandle_t adc_buffer{};
static RingbufHandle_t dac_buffer{};
static RingbufHandle_t gyro_buffer{};


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
    cJSON_AddNumberToObject(obj, "base", CONFIG_vRADAR_BASE_FREQUENCY);
    cJSON_AddNumberToObject(obj, "xFov", CONFIG_vRADAR_FOV_X);
    cJSON_AddNumberToObject(obj, "yFov", CONFIG_vRADAR_FOV_Y);
    // Generate the ADC parameters
    auto adc = cJSON_CreateObject();
    auto settings = Settings::instance();

    auto system = settings.getSystem();
    cJSON_AddNumberToObject(obj, "enabled", system.enabled);
    cJSON_AddNumberToObject(obj, "audible", system.audible);
    cJSON_AddNumberToObject(obj, "gyro", system.gyro);

    cJSON *chirpObj = cJSON_CreateObject();
    cJSON_AddNumberToObject(chirpObj, "prf", system.chirp.prf);
    cJSON_AddNumberToObject(chirpObj, "duration", system.chirp.duration);
    cJSON_AddNumberToObject(chirpObj, "steps", system.chirp.steps);
    cJSON_AddNumberToObject(chirpObj, "padding", system.chirp.padding);
    cJSON_AddNumberToObject(chirpObj, "resolution", system.chirp.resolution);

    cJSON *samplingObj = cJSON_CreateObject();
    cJSON_AddNumberToObject(samplingObj, "frequency", system.sampling.frequency);
    cJSON_AddNumberToObject(samplingObj, "samples", system.sampling.samples);
    cJSON_AddNumberToObject(samplingObj, "attenuation", system.sampling.attenuation);
    cJSON_AddNumberToObject(obj, "updated", (double) esp_timer_get_time());

    cJSON_AddItemToObject(obj, "sampling", samplingObj);
    cJSON_AddItemToObject(obj, "chirp", chirpObj);

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

static esp_err_t systemConfigHandler(httpd_req_t *req) {
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

    auto s = &Settings::instance();
    s->systemFromJson(buf);
    s->push();

    auto md = generateMetadata();


    httpd_resp_send(req, md, HTTPD_RESP_USE_STRLEN);
    // Send the status OK message
    httpd_resp_set_status(req, HTTPD_200);
    // Return without error

    free(md);

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

void int64ToUint8Array(int64_t input, uint8_t *output) {
    output[0] = (input >> 56) & 0xFF;
    output[1] = (input >> 48) & 0xFF;
    output[2] = (input >> 40) & 0xFF;
    output[3] = (input >> 32) & 0xFF;
    output[4] = (input >> 24) & 0xFF;
    output[5] = (input >> 16) & 0xFF;
    output[6] = (input >> 8) & 0xFF;
    output[7] = input & 0xFF;
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
        auto buffer = (uint8_t *) calloc(1, ws_pkt.len + 1);
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

void uint16ToUint8Array(uint16_t input, uint8_t *output) {
    output[0] = (input >> 8) & 0xFF;
    output[1] = input & 0xFF;
}

SemaphoreHandle_t wsLock;

static void callback(SampleData *sd) {
    if (fd < 0) {
        return;
    }
    httpd_ws_frame_t out_packer;
    memset(&out_packer, 0, sizeof(httpd_ws_frame_t));

    size_t total_len = sd->size * 4 * sizeof(uint16_t) + sizeof(int64_t) * 2;
    // Allocate memory for the binary data buffer
    auto *binary_data = (uint8_t *) malloc(total_len);

    for (int i = 0; i < 4; i++) {
        // Convert the pointer array into a hex buffer for transport
        if (sd->data[i] == nullptr) {
            free(binary_data);
            return;
        }

        for (int j = 0; j < sd->size; ++j) {
            uint16ToUint8Array(sd->data[i][j], &binary_data[i * sd->size * sizeof(uint16_t) +
                                                            j * sizeof(uint16_t)]);
        }
    }

    int64ToUint8Array(sd->start, &binary_data[4 * sd->size * sizeof(uint16_t)]);
    int64ToUint8Array(sd->stop, &binary_data[4 * sd->size * sizeof(uint16_t) + sizeof(int64_t)]);
//    int64ToUint8Array(sd->chirpStart, &binary_data[4 * sd->size * sizeof(uint16_t) + sizeof(int64_t) * 2]);
//    int64ToUint8Array(sd->chirpStop, &binary_data[4 * sd->size * sizeof(uint16_t) + sizeof(int64_t) * 3]);

    out_packer.payload = (uint8_t *) binary_data;
    out_packer.len = total_len;
    out_packer.type = HTTPD_WS_TYPE_BINARY;

//    send_websocket_frame_using_queue(hd, fd, &out_packer);
    if (xSemaphoreTake(wsLock, portMAX_DELAY) != pdTRUE) {
        free(binary_data);
        return;
    }
    auto ret = httpd_ws_send_frame_async(hd, fd, &out_packer);
    if (ret != ESP_OK) {
        printf("Send to bridge failed!\n");
        fd = -1;
    }

    free(binary_data);
    xSemaphoreGive(wsLock);

}


static void sendMetadata(GyroData *gd, float temperature, int8_t rssi) {
    if (fd < 0) {
        vTaskDelay(1);
        return;
    }

    httpd_ws_frame_t out_packer;
    memset(&out_packer, 0, sizeof(httpd_ws_frame_t));
//    char over[1024]{};
//    vTaskGetRunTimeStats(over);

    auto obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(obj, "pitch", gd->pitch);
    cJSON_AddNumberToObject(obj, "roll", gd->roll);
    cJSON_AddNumberToObject(obj, "temperature", temperature);
    cJSON_AddNumberToObject(obj, "rssi", rssi);
//    cJSON_AddStringToObject(obj, "buffer", over);

    char *buf = (char *) malloc(sizeof(char) * 512);

    if (!cJSON_PrintPreallocated(obj, buf, 512, false)) {
        cJSON_Delete(obj);
        return;
    }

    out_packer.payload = (uint8_t *) buf;
    out_packer.len = strlen(buf);
    out_packer.type = HTTPD_WS_TYPE_TEXT;
    if (xSemaphoreTake(wsLock, pdMS_TO_TICKS(100)) != pdTRUE) {
        free(buf);
        return;
    }
//    send_websocket_frame_using_queue(hd, fd, &out_packer);
    auto ret = httpd_ws_send_frame_async(hd, fd, &out_packer);
    free(buf);
    if (ret != ESP_OK) {
        printf("Send to bridge failed!\n");
        fd = -1;
        xSemaphoreGive(wsLock);
        return;
    }
    cJSON_Delete(obj);
    xSemaphoreGive(wsLock);
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

static const httpd_uri_t systemConf = {
        .uri       = "/system",
        .method    = HTTP_POST,
        .handler   = systemConfigHandler,
        .is_websocket = false,
};


static const httpd_uri_t reboot = {
        .uri       = "/reboot",
        .method    = HTTP_POST,
        .handler   = restart,
        .is_websocket = false,
};


void gyroWatcher(void *arg) {
    temperature_sensor_handle_t temp_handle = nullptr;
    temperature_sensor_config_t temp_sensor = {
            .range_min = 10,
            .range_max = 65,
    };
    ESP_ERROR_CHECK(temperature_sensor_install(&temp_sensor, &temp_handle));


    while (1) {
        size_t size = -1;
        void *dat = xRingbufferReceive(gyro_buffer, &size, portMAX_DELAY);
        if (size <= 0 || dat == nullptr) {
//            vRingbufferReturnItem(gyro_buffer, dat);
            continue;
        }

        auto *data = (GyroData *) dat;
        // Enable temperature sensor
        ESP_ERROR_CHECK(temperature_sensor_enable(temp_handle));
        // Get converted sensor data
        float temp;
        ESP_ERROR_CHECK(temperature_sensor_get_celsius(temp_handle, &temp));
        // Disable the temperature sensor if it's not needed and save the power
        ESP_ERROR_CHECK(temperature_sensor_disable(temp_handle));
        wifi_ap_record_t ap_info;
        esp_err_t err = esp_wifi_sta_get_ap_info(&ap_info);
        int8_t rssi = -1;

        if (err == ESP_OK) {

            rssi = ap_info.rssi;
        } else {
            rssi = -1;
        }

        sendMetadata(data, temp, rssi);

        vRingbufferReturnItem(gyro_buffer, dat);

    }
}

void watcher(void *arg) {
    while (1) {

        size_t size = -1;
        void *dat = xRingbufferReceive(adc_buffer, &size, portMAX_DELAY);
        if (dat == nullptr || size <= 0) {
//            vRingbufferReturnItem(adc_buffer, dat);
            continue;
        }
        auto data = (SampleData *) dat;

//        callback(data);
        callback(data);
//        printf("Sending Data... (%lu)\n", esp_get_free_heap_size());

        for (int i = 0; i < 4; i++) {
            if (data->data[i] != nullptr) {
                heap_caps_free(data->data[i]);
            }
        }
        heap_caps_free(data->data);

        vRingbufferReturnItem(adc_buffer, dat);

    }
}

static int64_t last = 0;
static int64_t loop = 0;


void adcTask(void *arg) {
    auto s = new Sample();

    new DAC(xTaskGetCurrentTaskHandle());
    last = esp_timer_get_time();
    int samples = 256;
    while (true) {
        auto settings = Settings::instance();
        auto chirp = settings.getSystem().chirp;

        samples = (int) ceil(((double) chirp.prf / 1000.0) / (double) (1000.0 / settings.getSystem().sampling
                .frequency));
//        printf("Capturing: %d\n", samples);
        if (samples < 32) samples = 32;
        if (samples > 1024) samples = 1024;

//        auto itemSize = xRingbufferGetMaxItemSize(adc_buffer);
//        auto freeSize = xRingbufferGetCurFreeSize(adc_buffer);
//
//        if (freeSize < itemSize) {
//            size_t size = 0;
//            auto *item = xRingbufferReceive(adc_buffer, &size, 1);
//            if (item != nullptr) {
//                auto *data = (SampleData *) item;
//                if (data->data != nullptr) {
//                    for (int i = 0; i < 4; ++i) {
//                        if (data->data[i] != nullptr) heap_caps_free(data->data[i]);
//                    }
//                    heap_caps_free(data->data);
//                }
//                vRingbufferReturnItem(adc_buffer, item);
//            }
//        }

        auto **data = (uint16_t **) heap_caps_malloc(sizeof(uint16_t *) * 4, MALLOC_CAP_SPIRAM);
        if (data == nullptr) {
            printf("No Memory\n");
            vTaskDelay(pdMS_TO_TICKS(1));
            continue;
        }

        for (int i = 0; i < 4; ++i) {
            data[i] = (uint16_t *) heap_caps_malloc(sizeof(uint16_t) * samples, MALLOC_CAP_SPIRAM);
            if (data[i] == nullptr) {
                printf("No Memory\n");
                vTaskDelay(pdMS_TO_TICKS(1));
                continue;
            }
        }

        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        int64_t start = esp_timer_get_time();
        esp_err_t err = s->listen(samples, data);
        if (err != ESP_OK) {
            printf("ERROR: %s\n", esp_err_to_name(err));
            for (int i = 0; i < 4; ++i) {
                if (data[i] != nullptr) heap_caps_free(data[i]);
            }
            heap_caps_free(data);
            vTaskDelay(1);
            continue;
        }

        int64_t end = esp_timer_get_time();
        int64_t cStart = 0;
        int64_t cStop = 0;

        SampleData sd = {
                .data = data,
                .size = samples,
                .start = start,
                .stop = end,
                .chirpStart = cStart,
                .chirpStop = cStop,
        };


        if (xRingbufferSend(adc_buffer, (void *) &sd, sizeof(sd), pdMS_TO_TICKS(0.5)) != pdTRUE) {
            printf("Overflow :< \n");
            for (int i = 0; i < 4; ++i) {
                if (data[i] != nullptr) heap_caps_free(data[i]);
            }
            heap_caps_free(data);
        } else {
//            if (loop == 0) {
//                printf("Collected Sample: Duration=%lld, Interval=%lld Last: %d\n", end - start, esp_timer_get_time() -
//                last, sd.data[3][511]);
//            }
//            loop = (loop + 1) % 4;
//            last = esp_timer_get_time();
        }


    }
    delete s;
}

//void dacTask(void *arg) {
//    auto handle = (TaskHandle_t *) arg;
//
//    while (1) {
//        vTaskDelay(pdMS_TO_TICKS(1000));
//    }
//    delete dac;
//}

void gyroTask(void *arg) {
    while (1) {

    }
}


Server::Server() {
    wsLock = xSemaphoreCreateBinary();
    if (server != nullptr) {
        return;
    }
    xSemaphoreGive(wsLock);

    server = nullptr;
    httpd_config_t httpdConf = HTTPD_DEFAULT_CONFIG();

    esp_err_t ret = httpd_start(&server, &httpdConf);
    if (ESP_OK != ret) {
        return;
    }

    Settings::instance();
    httpd_register_uri_handler(server, &config);
    httpd_register_uri_handler(server, &reboot);
    httpd_register_uri_handler(server, &options);
    httpd_register_uri_handler(server, &socket_get);
    httpd_register_uri_handler(server, &systemConf);

    adc_buffer = xRingbufferCreate((sizeof(SampleData *)) * (256), RINGBUF_TYPE_NOSPLIT);
    if (adc_buffer == nullptr) {
        printf("Failed to create ring buffer\n");
    }

    dac_buffer = xRingbufferCreate((sizeof(SampleData *)) * 4, RINGBUF_TYPE_NOSPLIT);
    if (dac_buffer == nullptr) {
        printf("Failed to create ring buffer\n");
    }

    gyro_buffer = xRingbufferCreate(sizeof(GyroData) * 20, RINGBUF_TYPE_NOSPLIT);
    if (gyro_buffer == nullptr) {
        printf("Failed to create ring buffer\n");
    }

    printf("RingBuffers initialized...\n");

    new Gyro(gyro_buffer);

    TaskHandle_t adcTaskHandle{};
    xTaskCreatePinnedToCore(watcher, "watcherTask", 8192, nullptr, tskIDLE_PRIORITY + 5, nullptr, 1);
    xTaskCreate(gyroWatcher, "gyroWatcher", 8192, nullptr, tskIDLE_PRIORITY + 3, nullptr);
    xTaskCreatePinnedToCore(adcTask, "adcTask", 8192, nullptr, tskIDLE_PRIORITY + 6, &adcTaskHandle, 0);

//    xTaskCreate(dacTask, "dacTask", 24000, &adcTaskHandle, tskIDLE_PRIORITY + 3, nullptr);


}
