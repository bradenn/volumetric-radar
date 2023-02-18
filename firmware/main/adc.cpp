//
// Created by Braden Nicholson on 2/6/23.
//

#include <esp_timer.h>
#include <cstring>
#include <driver/adc.h>
#include "adc.h"

#define INDEX_FROM_CHANNEL(channel) (channel-4)
static TaskHandle_t s_task_handle;
uint64_t micros() {
    return esp_timer_get_time();
}

static adc_channel_t channels[ADC_NUM_CHANNELS] = {ADC_CHANNEL_6};

/**
  * @brief Configure the analog to digital converter
  *
  * @return
  *    - ESP_OK: succeed
  */
void Adc::setup(adc_continuous_handle_t *out_handle) {

    adc_continuous_handle_t localHandle = nullptr;

    adc_continuous_handle_cfg_t adc_config = {
            .max_store_buf_size = ADC_CONV_BUFFER,
            .conv_frame_size = ADC_CONV_FRAME,
    };

    esp_err_t err;
    err = adc_continuous_new_handle(&adc_config, &localHandle);
    if (err != ESP_OK) {
        printf("Add handle failed: %s\n", esp_err_to_name(err));
        return;
    }

    adc_continuous_config_t dig_cfg = {
            .sample_freq_hz = ADC_FREQUENCY,
            .conv_mode = ADC_CONV_MODE,
            .format = ADC_OUTPUT_TYPE,
    };

    adc_digi_pattern_config_t adc_pattern[SOC_ADC_PATT_LEN_MAX] = {{0, 0, 0, 0}};
    dig_cfg.pattern_num = ADC_NUM_CHANNELS;
    for (int i = 0; i < ADC_NUM_CHANNELS; i++) {
        adc_pattern[i].atten = ADC_ATTENUATION;
        adc_pattern[i].channel = channels[i] & 0x7;
        adc_pattern[i].unit = ADC_UNIT;
        adc_pattern[i].bit_width = ADC_BIT_WIDTH;
    }
    dig_cfg.adc_pattern = adc_pattern;
    err = adc_continuous_config(localHandle, &dig_cfg);
    if(err != ESP_OK){
        printf("ADC Register failed!\n");
        return;
    }

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

void Adc::begin() {
    printf("Starting ADC...\n");
    uint32_t read = 0;
    uint8_t result[ADC_CONV_FRAME] = {0};
    memset(result, 0xcc, ADC_CONV_FRAME);

    s_task_handle = xTaskGetCurrentTaskHandle();

    handle = nullptr;
    setup(&handle);

    adc_continuous_evt_cbs_t cbs = {
            .on_conv_done = s_conv_done_cb
    };

    esp_err_t err;
    adc1_ulp_enable();
    err = adc_continuous_register_event_callbacks(handle, &cbs, nullptr);
    if(err != ESP_OK){
        printf("ADC Register failed!\n");
        return;
    }

    err = adc_continuous_start(handle);
    if(err != ESP_OK){
        printf("ADC Register failed!\n");
        return;
    }

    uint64_t last = micros();
    uint64_t numCycles = micros();

    printf("ADC Running.\n");
    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        // For each channel that the ADC is configured to read
        while (1) {
            int sumBuffer[ADC_NUM_CHANNELS] = {0};
            int sumSums[ADC_NUM_CHANNELS] = {0};
            err = adc_continuous_read(handle, result, ADC_CONV_FRAME, &read, 0);
            if (err == ESP_ERR_TIMEOUT){
                break;
            }else if(err != ESP_OK) {
                continue;
            }

            for (int i = 0; i < read; i += SOC_ADC_DIGI_RESULT_BYTES) {
                auto *out = (adc_digi_output_data_t *) &result[i];
                uint32_t channel = out->type1.channel;
                uint32_t value = out->type1.data;
                if (channel == ADC_CHANNEL_6) {
                    sumBuffer[0] += makeSigned(value);
                    sumSums[0]++;
//                    buffers[0]->push(makeSigned(value));
                } else if (channel == ADC_CHANNEL_7) {
//                    buffers[1]->push(makeSigned(value));
//                    sumBuffer[1] += makeSigned(value);
//                    sumSums[1]++;
                }
            }
            if(sumSums[0] <= 0){
                continue;
            }
            buffers[0]->push(sumBuffer[0] / sumSums[0]);
//            buffers[1]->push(sumBuffer[0] / sumSums[0]);
//            vTaskDelay(1);
            numCycles++;
//        if(micros()-last > 1000*1000) {
//            printf("%llu Hz\n", numCycles);
//            numCycles = 0;
//            last = micros();
//        }

        }
    }

    err = adc_continuous_stop(handle);
    if(err != ESP_OK){
        printf("ADC Register failed!\n");
        return;
    }

    err = adc_continuous_deinit(handle);
    if(err != ESP_OK){
        printf("ADC Register failed!\n");
        return;
    }
}

Adc::Adc() {
    buffers[0] = new Buffer();
//    buffers[1] = new Buffer();
}

