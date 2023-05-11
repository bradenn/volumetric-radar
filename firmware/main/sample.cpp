//
// Created by Braden Nicholson on 4/20/23.
//

#include <cstring>
#include "sample.h"

static TaskHandle_t adcTaskHandle;

#define SAMPLE_ATTENUATION ADC_ATTEN_DB_11

static bool IRAM_ATTR adcConversionDone(adc_continuous_handle_t handle, const adc_continuous_evt_data_t *edata, void
*user_data) {
    BaseType_t mustYield = pdTRUE;
    vTaskNotifyGiveFromISR(adcTaskHandle,&mustYield);
    portYIELD_FROM_ISR(mustYield);
    return (mustYield == pdFALSE);
}

esp_err_t Sample::listen(int64_t chirpDuration, uint16_t **out) {
    if (xSemaphoreTake(runtime, pdMS_TO_TICKS(0.5)) != pdTRUE) {
        printf("Sample semaphore cannot lock!\n");
        return ESP_ERR_TIMEOUT;
    }


    esp_err_t err;
//    err = initializeContinuousAdc();
//    if(err != ESP_OK) {
//        return err;
//    }
    adcTaskHandle = xTaskGetCurrentTaskHandle();

    err = adc_continuous_start(adcContinuousHandle);
    if (err != ESP_OK) {
//        destructContinuousAdc();
        xSemaphoreGive(runtime);
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
        for (int offset: offsets) {
            int channelRemain = ((int) chirpDuration - offset);
            remain += channelRemain * SOC_ADC_DIGI_RESULT_BYTES;
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
    // Stop the ADC from listening and free DMA memory
    adc_continuous_stop(adcContinuousHandle);
//    destructContinuousAdc();
    xSemaphoreGive(runtime);
    return ESP_OK;
}

esp_err_t initializeRadarGpio() {

    gpio_config_t io_conf = {
            .pin_bit_mask = (1ULL << CONFIG_vRADAR_ENABLE),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t err;

    err = gpio_config(&io_conf);
    if (err != ESP_OK) {
        return err;
    }

    err = gpio_set_level(static_cast<gpio_num_t>(CONFIG_vRADAR_ENABLE), 1);
    if (err != ESP_OK) {
        return err;
    }

    return ESP_OK;
}

esp_err_t Sample::reinitialize() {
    if (xSemaphoreTake(runtime, pdMS_TO_TICKS(5)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t err;
    err = destructCalibrationProfile();
    if (err != ESP_OK) {
        xSemaphoreGive(runtime);
        return err;
    }

    err = destructContinuousAdc();
    if (err != ESP_OK) {
        xSemaphoreGive(runtime);
        return err;
    }

    err = initializeCalibrationProfile();
    if (err != ESP_OK) {
        xSemaphoreGive(runtime);
        return err;
    }

    err = initializeContinuousAdc();
    if (err != ESP_OK) {
        xSemaphoreGive(runtime);
        return err;
    }

    xSemaphoreGive(runtime);
    return ESP_OK;
}

esp_err_t Sample::initializeContinuousAdc() {

    int channelMap[SAMPLE_CHANNEL_COUNT] = {CONFIG_vRADAR_I1, CONFIG_vRADAR_Q1, CONFIG_vRADAR_Q2, CONFIG_vRADAR_I2};

    adc_continuous_handle_cfg_t continuousHandleCfg = {
            .max_store_buf_size = SAMPLE_CONVERSION_FRAME_SIZE*2,
            .conv_frame_size = SAMPLE_CONVERSION_FRAME_SIZE,
    };

    esp_err_t err;
    err = adc_continuous_new_handle(&continuousHandleCfg, &adcContinuousHandle);
    if (err != ESP_OK) {
        return err;
    }

    adc_continuous_config_t adcContinuousConfig = {
            .pattern_num = SAMPLE_CHANNEL_COUNT,
            .sample_freq_hz = 4 * (uint32_t) sampling.frequency,
            .conv_mode = ADC_CONV_SINGLE_UNIT_1,
            .format = ADC_DIGI_OUTPUT_FORMAT_TYPE2,
    };

    adc_digi_pattern_config_t adcPatternConfig[SOC_ADC_PATT_LEN_MAX] = {};

    for (int i = 0; i < SAMPLE_CHANNEL_COUNT; i++) {
        adc_unit_t unit{};
        adc_channel_t channel{};

        err = adc_continuous_io_to_channel(channelMap[i], &unit, &channel);
        if (err != ESP_OK) {
            continue;
        }

        adcPatternConfig[i].atten = static_cast<adc_atten_t>(sampling.attenuation);
        adcPatternConfig[i].channel = channel;
        adcPatternConfig[i].unit = unit;
        adcPatternConfig[i].bit_width = SOC_ADC_DIGI_MAX_BITWIDTH;
    }

    adcContinuousConfig.adc_pattern = adcPatternConfig;

    err = adc_continuous_config(adcContinuousHandle, &adcContinuousConfig);
    if (err != ESP_OK) {
        printf("Failed to configure continuous ADC: %s\n", esp_err_to_name(err));
        return err;
    }

    adc_continuous_evt_cbs_t adcContinuousEvtCbs = {
            .on_conv_done = adcConversionDone
    };

    err = adc_continuous_register_event_callbacks(adcContinuousHandle, &adcContinuousEvtCbs, nullptr);
    if (err != ESP_OK) {
        printf("Failed to register continuous adc event: %s\n", esp_err_to_name(err));
        return err;
    }

    return ESP_OK;
}

esp_err_t Sample::initializeCalibrationProfile() {

    adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = ADC_UNIT_1,
            .atten = static_cast<adc_atten_t>(sampling.attenuation),
            .bitwidth = ADC_BITWIDTH_12,
    };

    if (adc_cali_create_scheme_curve_fitting(&cali_config, &calHandle) != ESP_OK) {
        printf("ADC Calibration failed!\n");
        return ESP_ERR_INVALID_STATE;
    }

    return ESP_OK;
}

esp_err_t Sample::destructContinuousAdc() {

    esp_err_t err;
    err = adc_continuous_deinit(adcContinuousHandle);
    if (err != ESP_OK) {
        return err;
    }

    return ESP_OK;
}

esp_err_t Sample::destructCalibrationProfile() {

    esp_err_t err;
    err = adc_cali_delete_scheme_curve_fitting(calHandle);
    if (err != ESP_OK) {
        return err;
    }

    return ESP_OK;
}

Sample::Sample() {
    esp_err_t err;

    runtime = xSemaphoreCreateMutex();

    err = initializeRadarGpio();
    if (err != ESP_OK) {
        printf("Failed to initialize the radar enable gpio: %s\n", esp_err_to_name(err));
    }

    err = initializeConfigurationTimer();
    if (err != ESP_OK) {
        printf("Failed to initialize the sample configuration timer: %s\n", esp_err_to_name(err));
    }

    err = initializeContinuousAdc();
    if (err != ESP_OK) {
        printf("Failed to initialized continuous adc: %s\n", esp_err_to_name(err));
        return;
    }

    err = initializeCalibrationProfile();
    if (err != ESP_OK) {
        printf("Failed to initialized adc calibration profile: %s\n", esp_err_to_name(err));
        return;
    }

}

void sampleConfigTimerCallback(void *params) {
    auto sample = (Sample *) params;

    auto settings = Settings::instance();
    auto sampling = settings.getSystem().sampling;

    bool runtimeChanged = false;

    if(settings.getSystem().enabled == 0){
        gpio_set_level(static_cast<gpio_num_t>(CONFIG_vRADAR_ENABLE), 0);
    }else{
        gpio_set_level(static_cast<gpio_num_t>(CONFIG_vRADAR_ENABLE), 1);
    }

    if (sample->sampling.attenuation != sampling.attenuation) {
        runtimeChanged = true;
    }

    if (sample->sampling.samples != sampling.samples) {
        runtimeChanged = true;
    }

    if (sample->sampling.frequency != sampling.frequency) {
        runtimeChanged = true;
    }

    if (runtimeChanged) {
        esp_err_t err;
        err = sample->reinitialize();
        if (err != ESP_OK) {
            printf("Sample reinitialization failed: %s\n", esp_err_to_name(err));
            return;
        }
        sample->sampling = sampling;
    }

}

esp_err_t Sample::initializeConfigurationTimer() {
    auto settings = Settings::instance();
    sampling = settings.getSystem().sampling;

    configTimer = {};
    // Define the periodic configuration timer
    const esp_timer_create_args_t periodic_timer_args = {
            .callback = sampleConfigTimerCallback,
            .arg = this,
            .name = "sampleConfiguration"
    };
    esp_err_t err;
    // Initialize the periodic timer
    err = esp_timer_create(&periodic_timer_args, &configTimer);
    if (err != ESP_OK) {
        return err;
    }
    // Set the periodic timer to update every second
    err = esp_timer_start_periodic(configTimer, 1000 * 1125);
    if (err != ESP_OK) {
        return err;
    }

    return ESP_OK;
}

Sample::~Sample() {
    esp_err_t err;

    err = destructCalibrationProfile();
    if (err != ESP_OK) {
        printf("Failed to destruct calibration profile: %s\n", esp_err_to_name(err));
    }

    err = destructContinuousAdc();
    if (err != ESP_OK) {
        printf("Failed to destruct continuous adc: %s\n", esp_err_to_name(err));
    }

    err = esp_timer_stop(configTimer);
    if (err != ESP_OK) {
        printf("Failed to stop sample configuration timer: %s\n", esp_err_to_name(err));
    }

    err = esp_timer_delete(configTimer);
    if (err != ESP_OK) {
        printf("Failed to destruct sample config timer: %s\n", esp_err_to_name(err));
    }

    vSemaphoreDelete(runtime);
}
