//
// Created by Braden Nicholson on 2/6/23.
//

#include <esp_timer.h>
#include "adc.h"

Adc &Adc::instance() {
    static Adc the_instance;
    return the_instance;
}

uint64_t micros() {
    return esp_timer_get_time();
}


static adc_channel_t channels[ADC_NUM_CHANNELS] = {ADC_CHANNEL_4, ADC_CHANNEL_5};

void Adc::setup() {
    adc_continuous_handle_cfg_t adc_config = {
            .max_store_buf_size = ADC_CONV_BUFFER,
            .conv_frame_size = ADC_CONV_FRAME,
    };

    ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_config, &handle));

    adc_continuous_config_t dig_cfg = {
            .sample_freq_hz = SOC_ADC_SAMPLE_FREQ_THRES_HIGH,
            .conv_mode = ADC_CONV_MODE,
            .format = ADC_OUTPUT_TYPE,
    };

    adc_digi_pattern_config_t adc_pattern[SOC_ADC_PATT_LEN_MAX] = {};
    dig_cfg.pattern_num = ADC_NUM_CHANNELS;
    for (int i = 0; i < ADC_NUM_CHANNELS; i++) {
        adc_pattern[i].atten = ADC_ATTENUATION;
        adc_pattern[i].channel = channels[i] & 0x7;
        adc_pattern[i].unit = ADC_UNIT;
        adc_pattern[i].bit_width = ADC_BIT_WIDTH;
    }
    dig_cfg.adc_pattern = adc_pattern;
    ESP_ERROR_CHECK(adc_continuous_config(handle, &dig_cfg));
}

static bool IRAM_ATTR
s_conv_done_cb(adc_continuous_handle_t handle, const adc_continuous_evt_data_t *edata, void *user_data) {
    BaseType_t mustYield = pdFALSE;
    //Notify that ADC continuous driver has done enough number of conversions
    vTaskNotifyGiveFromISR(adcTask, &mustYield);

    return (mustYield == pdTRUE);
}

int makesigned(unsigned x) {
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
    adc_continuous_evt_cbs_t cbs = {
            .on_conv_done = s_conv_done_cb
    };

    setup();
    adcTask = xTaskGetCurrentTaskHandle();


    adc_continuous_register_event_callbacks(handle, &cbs, nullptr);
    adc_continuous_start(handle);
    uint32_t read = 0;
    uint8_t result[ADC_CONV_FRAME] = {0};
    uint64_t lt = micros();
    printf("ADC Running.\n");
    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        // For each channel that the ADC is configured to read

        while (1) {

            esp_err_t err = adc_continuous_read(handle, result, ADC_CONV_FRAME, &read, 0);
            if (err != ESP_OK) break;

//            int results[ADC_NUM_CHANNELS][ADC_CONV_FRAME] = {{}, {}};
            auto results = new int *[ADC_NUM_CHANNELS];

            for (int i = 0; i < ADC_NUM_CHANNELS; ++i) {
                results[i] = new int[ADC_CONV_FRAME]{0};
            }

            int refs[4] = {0};

            for (int i = 0; i < read; i += SOC_ADC_DIGI_RESULT_BYTES) {
                auto *out = (adc_digi_output_data_t *) &result[i];
                uint32_t channel = out->type1.channel;
                uint32_t value = out->type1.data;
                if (channel == 4 || channel == 5) {

                    results[channel-4][refs[channel-4]] = makesigned(value);
                    refs[channel-4]++;
                    if(refs[channel-4]+1 >= ADC_CONV_FRAME) break;
                }

            }

            overflow(ADC_NUM_CHANNELS, ADC_CONV_FRAME, results);

            for (int i = 0; i < ADC_NUM_CHANNELS; ++i) {
                delete[] results[i];
            }

            delete[] results;
            vTaskDelay(1);
        }


    }
}

Adc::Adc() {

}

void Adc::overflow(int m, int n, int **results) {

    if (func == nullptr) return;
    func(ADC_NUM_CHANNELS, ADC_CONV_FRAME, results);

}


