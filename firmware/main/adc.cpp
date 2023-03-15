//
// Created by Braden Nicholson on 2/6/23.
//

#include <esp_timer.h>
#include <cstring>
#include <esp_adc/adc_cali_scheme.h>
#include "adc.h"

static TaskHandle_t s_task_handle;
SemaphoreHandle_t adcMutex;

uint64_t micros() {
    return esp_timer_get_time();
}

void Adc::adcTask(void *arg) {
    auto adc = (Adc *) arg;
    while (true) {
        xSemaphoreTake(adcMutex, portMAX_DELAY);
        // DO stuff
        int *mk[4] = {};
        bool all = true;
        for (int j = 0; j < adc->conf.channels.count; ++j) {
            int *front = adc->buffers[j]->front();
            if (front == nullptr && adc->buffers[j]->numBuffers() < 2) {
                all = false;
            } else {
                mk[j] = front;
            }
        }

        if (all) {
            if (xRingbufferSend(adc->rb, &mk, sizeof(mk), 0) == pdTRUE) {
                for (int j = 0; j < adc->conf.channels.count; ++j) {
                    adc->buffers[j]->pop();
                }
            }
        }
        xSemaphoreGive(adcMutex);
        vTaskDelay(1);
    }
}


Adc::Adc(AdcConfig conf, RingbufHandle_t rb) : conf(conf) {

    this->rb = rb;

    // Set the sample bytes to the number of samples per measurement * bytes per measurement * number of channels
    numSamples = conf.sampling.subSamples * SOC_ADC_DIGI_DATA_BYTES_PER_CONV * conf.channels.count;

    // Define the buffer size as being large enough to contain a sample set from all channels
    bufferSize = numSamples;
    adcMutex = xSemaphoreCreateMutex();

    xTaskCreatePinnedToCore(begin, "adcRunner", BUFFER_SIZE * BUFFER_COUNT * 4 + 4096, this, tskIDLE_PRIORITY + 2,
                            nullptr, 1);

//    esp_timer_handle_t watchTask;
//    esp_timer_create_args_t chirpArgs = {
//            .callback = reinterpret_cast<esp_timer_cb_t>(adcTask),
//            .arg = this,
//            .name = "adcTimerTask",
//    };
//    esp_timer_create(&chirpArgs, &watchTask);
//    esp_timer_start_periodic((esp_timer_handle_t) watchTask, 1000*50);
    xTaskCreatePinnedToCore(adcTask, "adcTimer", 4096, this, tskIDLE_PRIORITY + 1,
                            nullptr, 1);

}


/**
  * @brief Configure the analog to digital converter
  *
  * @return
  *    - ESP_OK: succeed
  */
void Adc::setup(adc_continuous_handle_t *out_handle) {
    // Initialize a local handle
    adc_continuous_handle_t localHandle = nullptr;
    // Define configuration for buffer size and samples per frame
    adc_continuous_handle_cfg_t adc_config = {
            .max_store_buf_size = static_cast<uint32_t>(bufferSize),
            .conv_frame_size = static_cast<uint32_t>(numSamples),
    };

    // Initialize handle
    esp_err_t err;
    err = adc_continuous_new_handle(&adc_config, &localHandle);
    if (err != ESP_OK) {
        printf("Add handle failed: %s\n", esp_err_to_name(err));
        return;
    }
    // Define parameters


    adc_continuous_config_t dig_cfg = {
            .sample_freq_hz = static_cast<uint32_t>(conf.sampling.rate),
            .conv_mode = ADC_CONV_SINGLE_UNIT_1,
            .format = ADC_OUTPUT_TYPE,
    };
    // Define pattern
    adc_digi_pattern_config_t adc_pattern[SOC_ADC_PATT_LEN_MAX] = {};
    dig_cfg.pattern_num = conf.channels.count;
    for (int i = 0; i < conf.channels.count; i++) {
        adc_pattern[i].atten = conf.sampling.attenuation;
        adc_pattern[i].channel = conf.channels.ports[i] & 0x7;
        adc_pattern[i].unit = ADC_UNIT;
        adc_pattern[i].bit_width = ADC_BIT_WIDTH;
    }
    dig_cfg.adc_pattern = adc_pattern;
    // Emplace patterns
    err = adc_continuous_config(localHandle, &dig_cfg);
    if (err != ESP_OK) {
        printf("ADC Register failed!\n");
        return;
    }
    // Set the local handle to the main filter
    *out_handle = localHandle;
    int samplesSec = conf.sampling.rate / conf.sampling.subSamples;
    int channels = conf.channels.count;
    printf("ADC Configured: %d samples/sec * %d channels (%d B/s)\n", samplesSec, channels, samplesSec * channels * 4);
}

static bool IRAM_ATTR
s_conv_done_cb(adc_continuous_handle_t handle, const adc_continuous_evt_data_t *edata, void *user_data) {
    BaseType_t mustYield = pdFALSE;
    //Notify that ADC continuous driver has done enough number of conversions
    vTaskNotifyGiveFromISR(s_task_handle, &mustYield);

    return (mustYield == pdTRUE);
}

#define CHANNEL_TO_INDEX(ch) (ch-3)

int makeSigned(unsigned x) {
    if (x <= (unsigned) INT_MAX) {
        return (int) x;
    }

    /* assume 2's complement */
    if (x >= (unsigned) INT_MIN) {
        return 0 - (int) (-x);
    }

    abort();
    return 0;
}

void Adc::begin(void *params) {
    auto adc = (Adc *) params;

    adc->buffers[0] = new Buffer();
    adc->buffers[1] = new Buffer();
    adc->buffers[2] = new Buffer();
    adc->buffers[3] = new Buffer();

    adc->running = true;


    uint32_t read = 0;
    uint8_t result[128] = {0};
    memset(result, 0xcc, 128);

    s_task_handle = xTaskGetCurrentTaskHandle();

    adc->handle = nullptr;
    adc->setup(&adc->handle);

    adc_continuous_evt_cbs_t cbs = {
            .on_conv_done = s_conv_done_cb,
    };

    esp_err_t err;
    err = adc_continuous_register_event_callbacks(adc->handle, &cbs, nullptr);
    if (err != ESP_OK) {
        printf("ADC Register failed!\n");
        return;
    }

    err = adc_continuous_start(adc->handle);
    if (err != ESP_OK) {
        printf("ADC Register failed!\n");
        return;
    }

    adc_cali_handle_t calHandle = nullptr;
    adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = ADC_UNIT_1,
            .atten = adc->conf.sampling.attenuation,
            .bitwidth = ADC_BIT_WIDTH,
    };

    if (adc_cali_create_scheme_curve_fitting(&cali_config, &calHandle) != ESP_OK) {
        printf("ADC Calibration failed!\n");
    }
    adc->running = true;
    int cycles = 0;
    int ejections = 0;
    int start = (int) micros();
    while (adc->running) {

        ulTaskNotifyTake(pdTRUE, 1);

        while (true) {
            int sumBuffer[ADC_MAX_CHANNELS] = {0};
            int sumSums[ADC_MAX_CHANNELS] = {0};

            err = adc_continuous_read(adc->handle, result, adc->bufferSize, &read, 0);
            if (err == ESP_ERR_INVALID_STATE) {
                printf("Bad state\n");
                break;
            } else if (err != ESP_OK) {
                break;
            }
            xSemaphoreTake(adcMutex, portMAX_DELAY);
            for (int i = 0; i < read; i += SOC_ADC_DIGI_RESULT_BYTES) {
                auto *out = (adc_digi_output_data_t *) &result[i];
                uint32_t channel = out->type2.channel;
                uint32_t value = out->type2.data;

                int j = CHANNEL_TO_INDEX(channel);

                if (j < 0 || j >= 4) {
                    continue;
                }
//                int v = 0;
//                adc_cali_raw_to_voltage(calHandle, makeSigned(value), &v);

                adc->buffers[j]->push(makeSigned(value));
//                sumBuffer[j] += makeSigned(value);
//                sumSums[j] += 1;
                ejections++;

            }
            xSemaphoreGive(adcMutex);
            cycles++;

//            int *mk[4] = {};
//            bool all = true;
//            xSemaphoreTake(adcMutex, portMAX_DELAY);
//            for (int j = 0; j < adc->conf.channels.count; ++j) {
//                if (sumSums[j] > 0) {
//                    int v = sumBuffer[j] / sumSums[j];
//                    adc_cali_raw_to_voltage(calHandle, v, &v);
////                    int *front = adc->buffers[j]->front();
////                    if (front == nullptr) {
////                        all = false;
////                    } else {
////                        mk[j] = front;
////                    }
//
//                    adc->buffers[j]->push(v);
//                }
//            }
//            xSemaphoreGive(adcMutex);
//            if (all) {
//                if (xRingbufferSend(adc->rb, &mk, sizeof(mk), 0) == pdTRUE) {
//                    for (int j = 0; j < adc->conf.channels.count; ++j) {
//                        adc->buffers[j]->pop();
//                    }
//                }
//            }

            if (micros() - start > 1000 * 1000) {
                double duration = ((double) (micros() - start) / 1000.0 / 1000.0);
                printf("%.2f cycle/s,  %.2f reading/s - %lu -> %lu %lu -> ",
                       (double) ((double) cycles / duration),
                       (double) ((double) ejections / duration), read, esp_get_minimum_free_heap_size(),
                       esp_get_free_heap_size());
                for (int j = 0; j < adc->conf.channels.count; ++j) {
                    printf("b[%d] = %d, ", j, adc->buffers[j]->numBuffers());
                }
                printf("\n");
                cycles = 0;
                ejections = 0;
                start = (int) micros();
            }

        }

    }
    printf("ADC Shutting down components...\n");
    err = adc_cali_delete_scheme_curve_fitting(calHandle);
    if (err != ESP_OK) {
        printf("ADC delete scheme failed!\n");
        return;
    }

    err = adc_continuous_stop(adc->handle);
    if (err != ESP_OK) {
        printf("ADC Register failed!\n");
        return;
    }

    err = adc_continuous_deinit(adc->handle);
    if (err != ESP_OK) {
        printf("ADC Register failed!\n");
        return;
    }

}

void Adc::capture(int us) {
    this->cap = micros() + us;
    while (this->cap > micros()) {

    }
    this->cap = 0;
}




