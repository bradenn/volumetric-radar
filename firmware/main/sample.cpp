//
// Created by Braden Nicholson on 4/20/23.
//

#include <cstring>
#include "sample.h"

static TaskHandle_t adcTaskHandle;

esp_err_t configureADC(adc_continuous_handle_t *adcContinuousHandle) {

    int channelMap[SAMPLE_CHANNEL_COUNT] = {CONFIG_vRADAR_I1, CONFIG_vRADAR_Q1, CONFIG_vRADAR_Q2, CONFIG_vRADAR_I2};


    adc_continuous_handle_cfg_t continuousHandleCfg = {
            .max_store_buf_size = SAMPLE_CONVERSION_FRAME_SIZE * 2,
            .conv_frame_size = SAMPLE_CONVERSION_FRAME_SIZE,
    };

    esp_err_t err;
    err = adc_continuous_new_handle(&continuousHandleCfg, adcContinuousHandle);
    if (err != ESP_OK) {
        return err;
    }


    adc_continuous_config_t adcContinuousConfig = {
            .pattern_num = SAMPLE_CHANNEL_COUNT,
            .sample_freq_hz = 4 * (1024 * 20),
            .conv_mode = ADC_CONV_SINGLE_UNIT_1,
            .format = ADC_DIGI_OUTPUT_FORMAT_TYPE2,
    };

    adc_digi_pattern_config_t adcDigiPatternConfig[SOC_ADC_PATT_LEN_MAX] = {};
    for (int i = 0; i < SAMPLE_CHANNEL_COUNT; i++) {
        adc_unit_t unit{};
        adc_channel_t channel{};

        err = adc_continuous_io_to_channel(channelMap[i], &unit, &channel);
        if (err != ESP_OK) {
            continue;
        }

        adcDigiPatternConfig[i].atten = ADC_ATTEN_DB_11;
        adcDigiPatternConfig[i].channel = channel;
        adcDigiPatternConfig[i].unit = unit;
        adcDigiPatternConfig[i].bit_width = SOC_ADC_DIGI_MAX_BITWIDTH;
    }
    adcContinuousConfig.adc_pattern = adcDigiPatternConfig;

    adc_continuous_config(*adcContinuousHandle, &adcContinuousConfig);

    return ESP_OK;
}

static bool IRAM_ATTR adcConversionDone(adc_continuous_handle_t handle, const adc_continuous_evt_data_t *edata, void
*user_data) {
    BaseType_t mustYield = pdTRUE;
    vTaskNotifyGiveFromISR(adcTaskHandle, &mustYield);
    portYIELD_FROM_ISR(mustYield);
    return (mustYield == pdFALSE);
}

esp_err_t Sample::listen(int64_t chirpDuration, uint16_t **out) {
    esp_err_t err;
    err = adc_continuous_start(adcContinuousHandle);
    if (err != ESP_OK) {
        return err;
    }

    const uint32_t maxLength = SAMPLE_CONVERSION_FRAME_SIZE;
    uint8_t buffer[maxLength] = {0};
    memset(buffer, 0x00, maxLength);

    uint32_t resolvedLength = 0;
    int offsets[4] = {0, 0, 0, 0};
    uint32_t remain;
    while (true) {
        remain = 0;
        // Calculate the remaining samples needed across all channels
        for (int offset : offsets) {
            int channelRemain = ((int) chirpDuration - offset);
            remain += channelRemain * 4;
        }
        // Return if there are no more samples to be collected
        if (remain == 0) {
            break;
        }
        // Cap the number of samples at the local stack buffer size
        remain = remain > maxLength ? maxLength : remain;
        // Wait for the conversion to complete
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        // Read the info from DMA into the local stack memory
        err = adc_continuous_read(adcContinuousHandle, buffer, remain, &resolvedLength, portMAX_DELAY);
        if (err != ESP_OK) {
            if (err == ESP_ERR_TIMEOUT) {
                vTaskDelay(1);
                continue;
            }
            continue;
        }
        // Process the raw ADC conversion data
        for (int i = 0; i < resolvedLength; i += SOC_ADC_DIGI_RESULT_BYTES) {
            auto data = (adc_digi_output_data_t *) &buffer[i];
            uint32_t channel = data->type2.channel;
            uint32_t value = data->type2.data;
            uint32_t unit = data->type2.unit;
            // Ensure there are no erroneous channels
            if (channel >= SOC_ADC_CHANNEL_NUM(unit)) {
                printf("Skipping unknown channel: %lu\n", channel);
                continue;
            }
            int voltage = 0;
            // Convert the raw ADC data to a voltage using the built-in ADC calibration profile
            adc_cali_raw_to_voltage(calHandle, (int) value, &voltage);
            // Convert the ADC channel to the pad number
            int idx = 0;
            err = adc_continuous_channel_to_io((adc_unit_t) unit, (adc_channel_t) channel, &idx);
            if (err != ESP_OK) {
                continue;
            }
            // Find the array index based on the pad number
            int index = idx - 4;
            // Reject indexes out of bounds
            if (index > 3 || index < 0) {
                continue;
            }
            // Prevent writing out of bounds
            if (offsets[index] >= (int) chirpDuration) {
                continue;
            }
            // Write the voltage value to the buffer
            out[index][offsets[index]] = (uint16_t) value;
            // Increment the offset index for the next iteration
            offsets[index]++;
        }
    }
//    for (int i = 0; i < 4; ++i) {
//        if (offsets[i] != 512) {
//            printf("Offset index %d is %d\n", i, offsets[i]);
//        }
//    }
    // Stop the ADC from listening and free DMA memory
    adc_continuous_stop(adcContinuousHandle);
    return ESP_OK;
}

#define NUM_SAMPLES (SOC_ADC_DIGI_DATA_BYTES_PER_CONV*SAMPLE_CHANNEL_COUNT*SAMPLE_CONVERSIONS_PER_FRAME)

#define SAMPLE_BUFFER (NUM_SAMPLES * SAMPLE_CHANNEL_COUNT)

Sample::Sample(RingbufHandle_t handle) {
    rbHandle = handle;
    gpio_config_t io_conf = {
            .pin_bit_mask = (1ULL << CONFIG_vRADAR_ENABLE),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
    };

    gpio_config(&io_conf);

    adcTaskHandle = xTaskGetCurrentTaskHandle();
    gpio_set_level(static_cast<gpio_num_t>(CONFIG_vRADAR_ENABLE), 1);


    esp_err_t err;
//    adc_continuous_handle_t adcContinuousHandle{};

    err = configureADC(&adcContinuousHandle);
    if (err != ESP_OK) {
        printf("ADC Config Failed: %s\n", esp_err_to_name(err));
        return;
    }

    adc_continuous_evt_cbs_t adcContinuousEvtCbs = {
            .on_conv_done = adcConversionDone
    };

    err = adc_continuous_register_event_callbacks(adcContinuousHandle, &adcContinuousEvtCbs, nullptr);
    if (err != ESP_OK) {
        printf("ADC Register Failed: %s\n", esp_err_to_name(err));
        return;
    }


    adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = ADC_UNIT_1,
            .atten = ADC_ATTEN_DB_11,
            .bitwidth = ADC_BITWIDTH_12,
    };

    if (adc_cali_create_scheme_curve_fitting(&cali_config, &calHandle) != ESP_OK) {
        printf("ADC Calibration failed!\n");
    }


}

Sample::~Sample() {

    adc_continuous_deinit(adcContinuousHandle);
}