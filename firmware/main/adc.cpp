//
// Created by Braden Nicholson on 2/6/23.
//

#include <esp_timer.h>
#include <cstring>
#include <esp_adc/adc_cali_scheme.h>
#include "adc.h"

static TaskHandle_t s_task_handle;

uint64_t micros() {
    return esp_timer_get_time();
}


void Adc::restart() {
    stop();
    start();
}


void Adc::start() {
    if (this->running || !this->idle) return;
    this->idle = false;
    while (!this->running) {
        vTaskDelay(1);
    }
    printf("ADC Startup complete.\n");
}

void Adc::stop() {
    if (this->idle || !this->running) return;
    this->running = false;
    while (!this->idle) {
        vTaskDelay(1);
    }

    printf("ADC Shutdown complete.\n");

}

Adc::Adc(AdcConfig *conf) {

    this->conf = conf;
    for (int i = 0; i < this->conf->channels.count+1; ++i) {
        buffers[i] = new Buffer();
    }

    xTaskCreate(begin, "adcRunner", 4096 * 2, this, 5, nullptr);

}


Adc::Adc() : Adc(new AdcConfig) {

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
            .max_store_buf_size = static_cast<uint32_t>(conf->sampling.bufferSize),
            .conv_frame_size = static_cast<uint32_t>(conf->sampling.subSamples),
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
            .sample_freq_hz = static_cast<uint32_t>(conf->sampling.rate),
            .conv_mode = ADC_CONV_MODE,
            .format = ADC_OUTPUT_TYPE,
    };
    // Define pattern
    adc_digi_pattern_config_t adc_pattern[SOC_ADC_PATT_LEN_MAX] = {{0, 0, 0, 0}};
    dig_cfg.pattern_num = conf->channels.count;
    for (int i = 0; i < conf->channels.count; i++) {
        adc_pattern[i].atten = conf->sampling.attenuation;
        adc_pattern[i].channel = conf->channels.ports[i] & 0x7;
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
}

static bool IRAM_ATTR
s_conv_done_cb(adc_continuous_handle_t handle, const adc_continuous_evt_data_t *edata, void *user_data) {
    BaseType_t mustYield = pdFALSE;
    //Notify that ADC continuous driver has done enough number of conversions
    vTaskNotifyGiveFromISR(s_task_handle, &mustYield);

    return (mustYield == pdTRUE);
}


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
    while (1) {

        printf("ADC Startup initializing...\n");
        uint32_t read = 0;
        uint8_t result[ADC_CONV_FRAME] = {0};
        memset(result, 0xcc, ADC_CONV_FRAME);

        s_task_handle = xTaskGetCurrentTaskHandle();

        adc->handle = nullptr;
        adc->setup(&adc->handle);

        adc_continuous_evt_cbs_t cbs = {
                .on_conv_done = s_conv_done_cb
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

        uint64_t last = micros();
        uint64_t numCycles = 0;
        adc_cali_handle_t calHandle = 0;
        adc_cali_curve_fitting_config_t cali_config = {
                .unit_id = ADC_UNIT_1,
                .atten = adc->conf->sampling.attenuation,
                .bitwidth = ADC_BIT_WIDTH,
        };

        if (adc_cali_create_scheme_curve_fitting(&cali_config, &calHandle) != ESP_OK) {
            printf("ADC Calibration failed!\n");
        }
        adc->running = true;
        while (1) {
            if (adc->idle) {
                vTaskDelay(1);
                continue;
            }
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

            // For each channel that the ADC is configured to read
            while (adc->running) {
                int sumBuffer[ADC_MAX_CHANNELS] = {0};
                int sumSums[ADC_MAX_CHANNELS] = {0};

                err = adc_continuous_read(adc->handle, result, adc->conf->sampling.subSamples, &read, 0);

                if (err == ESP_ERR_TIMEOUT) {
                    break;
                } else if (err != ESP_OK) {
                    vTaskDelay(1);
                    continue;
                }

                for (int i = 0; i < read; i += SOC_ADC_DIGI_RESULT_BYTES) {
                    auto *out = (adc_digi_output_data_t *) &result[i];
                    uint32_t channel = out->type2.channel;
                    uint32_t value = out->type2.data;
                    for (int j = 0; j < adc->conf->channels.count; ++j) {
                        if (channel == adc->conf->channels.ports[j]) {
                            sumBuffer[j] += makeSigned(value);
                            sumSums[j]++;
                        }
                    }
                }
//                if(micros() > adc->cap) continue;
                for (int j = 0; j < adc->conf->channels.count; ++j) {
                    if (sumSums[j] <= 0) {
                        continue;
                    }
                    int v = sumBuffer[j] / sumSums[j];
                    int out = 0;
                    adc->buffers[j]->push(v);
                    adc->cycles++;
                    if (micros() - adc->duration >= 1000 * 1000) {
                        adc->cycles = 0;
                        adc->duration = micros();
                    }
                    adc->capPerSec = (double) ((double) adc->cycles /
                                               ((double) (micros() - adc->duration) / 1000.0 / 1000.0)) /
                                     adc->conf->channels.count;
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

        adc->idle = true;
    }
}

void Adc::capture(int us) {
    this->cap = micros() + us;
    while (this->cap > micros()) {

    }
    this->cap = 0;
}




