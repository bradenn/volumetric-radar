//
// Created by Braden Nicholson on 2/6/23.
//

#include <esp_timer.h>
#include <cstring>
#include <esp_adc/adc_cali_scheme.h>
#include <driver/pulse_cnt.h>
#include <hal/gpio_types.h>
#include <cmath>
#include <driver/gptimer.h>
#include <driver/gpio.h>
#include <esp_adc/adc_oneshot.h>
#include "adc.h"
#include "dac.h"

static TaskHandle_t s_task_handle;
SemaphoreHandle_t adcMutex;

static QueueHandle_t clockQueue;
static pcnt_unit_handle_t pcnt_unit = nullptr;

struct counterCase {
    int pulses = 0;
    int duration = 0;
};

int64_t micros() {
    return esp_timer_get_time();
}

static double run = 0.0;
static int rate = 0.0;
static int cycle = 0;

//void Adc::counterTask(void *arg) {
//    auto adc = (Adc *) arg;
//    struct counterCase c{};
//
//    while (1) {
//        if (xQueueReceive(clockQueue, &c, portMAX_DELAY)) {
//                rate = (int) round(((double)c.pulses/((double)c.duration/1000.0/1000.0))*1000);
////            printf("%f\n", ((double)c.pulses/((double)c.duration/1000.0/1000.0)));
////            cycle = (cycle + 1) % 1;
//        }
//    }
//}

void Adc::adcTask(void *arg) {
    auto adc = (Adc *) arg;

    uint64_t ready = xRingbufferGetCurFreeSize(adc->rb);
    if (ready < 4 * 4 * 4) {
        return;
    }

    if (xSemaphoreTake(adcMutex, portMAX_DELAY) != pdTRUE) {
        return;
    }
    // DO stuff
    int *mk[4] = {};
    bool all = true;
    for (int j = 0; j < adc->conf.channels.count; ++j) {
        int *front = adc->buffers[j]->front();
        if (front == nullptr) {
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

}

//static uint64_t previous = micros();
//
//static bool fdo_counter(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx) {
//    BaseType_t high_task_awoken = pdFALSE;
//
//    int pulses = 0;
//    int ref = 0;
//    int now = (int) micros();
//
//    ESP_ERROR_CHECK(pcnt_unit_get_count(pcnt_unit, &ref));
//    pulses = ref;
//
//    ESP_ERROR_CHECK(pcnt_unit_clear_count(pcnt_unit));
//
//    auto q = (QueueHandle_t) user_ctx;
//    auto c = counterCase{
//            .pulses = pulses,
//            .duration = (int) (micros() - previous)
//    };
//    rate = pulses;
//    previous = now;
//
//    xQueueSendFromISR(q, &c, &high_task_awoken);
//    // return whether we need to yield at the end of ISR
//    return false;
//}

Adc::Adc(AdcConfig conf, RingbufHandle_t rb) : conf(conf) {
    printf("%d @ %d\n", conf.sampling.rate, conf.sampling.subSamples);
    this->rb = rb;

    // Set the sample bytes to the number of samples per measurement * bytes per measurement * number of channels
    numSamples = (conf.sampling.subSamples) * SOC_ADC_DIGI_DATA_BYTES_PER_CONV * conf.channels.count;

    // Define the buffer size as being large enough to contain a sample set from all channels
    bufferSize = numSamples * 4;
    adcMutex = xSemaphoreCreateMutex();

    xTaskCreatePinnedToCore(begin, "adcRunner", BUFFER_SIZE * BUFFER_COUNT * 4 + 4096, this, tskIDLE_PRIORITY + 5,
                            &runner, 1);

    esp_timer_handle_t chirpTimer;
    esp_timer_create_args_t chirpArgs = {
            .callback = adcTask,
            .arg = this,
            .name = "adcTimer",
    };
    esp_timer_create(&chirpArgs, &chirpTimer);
    esp_timer_start_periodic((esp_timer_handle_t) chirpTimer, 2 * 1000);


//    pcnt_unit_config_t unit_config = {
//            .low_limit = -16000,
//            .high_limit = 16000,
//    };
//
//    ESP_ERROR_CHECK(pcnt_new_unit(&unit_config, &pcnt_unit));
//
//
//    pcnt_chan_config_t chan_a_config = {
//            .edge_gpio_num = GPIO_NUM_8,
//            .level_gpio_num = -1,
//    };
//    pcnt_channel_handle_t pcnt_chan_a = NULL;
//    ESP_ERROR_CHECK(pcnt_new_channel(pcnt_unit, &chan_a_config, &pcnt_chan_a));
//
//    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan_a, PCNT_CHANNEL_EDGE_ACTION_INCREASE,
//                                                 PCNT_CHANNEL_EDGE_ACTION_INCREASE));
//
//    clockQueue = xQueueCreate(1, sizeof(counterCase));
//    ESP_ERROR_CHECK(pcnt_unit_enable(pcnt_unit));
//    ESP_ERROR_CHECK(pcnt_unit_clear_count(pcnt_unit));
//    ESP_ERROR_CHECK(pcnt_unit_start(pcnt_unit));
//
//    xTaskCreate(counterTask, "clockWatcher", 4096, this, tskIDLE_PRIORITY + 2, nullptr);
//
//    gptimer_handle_t gptimer = NULL;
//    gptimer_config_t timer_config = {
//            .clk_src = GPTIMER_CLK_SRC_DEFAULT,
//            .direction = GPTIMER_COUNT_UP,
//            .resolution_hz = 10 * 1000 * 1000, // 1MHz, 1 tick = 1us
//    };
//    ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &gptimer));
//
//    gptimer_alarm_config_t alarm_config = {
//            .alarm_count = 1000*10, // period = 1s @resolution 1MHz
//            .reload_count = 0, // counter will reload with 0 on alarm event
//            .flags {
//                    .auto_reload_on_alarm = true, // enable auto-reload
//            }
//    };
//    ESP_ERROR_CHECK(gptimer_set_alarm_action(gptimer, &alarm_config));
//
//    gptimer_event_callbacks_t cbs = {
//            .on_alarm = fdo_counter, // register user callback
//    };
//
//    ESP_ERROR_CHECK(gptimer_register_event_callbacks(gptimer, &cbs, clockQueue));
//    ESP_ERROR_CHECK(gptimer_enable(gptimer));
//    ESP_ERROR_CHECK(gptimer_start(gptimer));

}


/**
  * @brief Configure the analog to digital converter
  *
  * @return
  *    - ESP_OK: succeed
  */
void Adc::setup(adc_continuous_handle_t *out_handle) {
    while (1) {
        // Initialize a local handle
        adc_continuous_handle_t localHandle;
        // Define configuration for buffer size and samples per frame
        adc_continuous_handle_cfg_t adc_config = {
                .max_store_buf_size = static_cast<uint32_t>(bufferSize),
                .conv_frame_size = static_cast<uint32_t>(numSamples),
        };

        // Initialize handle

        esp_err_t err;
        err = adc_continuous_new_handle(&adc_config, &localHandle);
        if (err != ESP_OK) {
            printf("ADC handle failed: %s... (HEAP: %ld, %lu, %lu) Trying again in 100ms\n", esp_err_to_name(err),
                   esp_get_minimum_free_heap_size(), static_cast<uint32_t>(bufferSize),
                   static_cast<uint32_t>(numSamples));
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
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
        int channels = conf.channels.count;
        int samplesSec = (conf.sampling.rate / channels) / conf.sampling.subSamples;
        printf("ADC Configured: %d samples/sec * %d channels\n", samplesSec, channels);
        return;
    }
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
/*---------------------------------------------------------------
        ADC Calibration
---------------------------------------------------------------*/
static bool example_adc_calibration_init(adc_unit_t unit, adc_atten_t atten, adc_cali_handle_t *out_handle)
{
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (!calibrated) {
        adc_cali_curve_fitting_config_t cali_config = {
                .unit_id = unit,
                .atten = atten,
                .bitwidth = ADC_BITWIDTH_12,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

    *out_handle = handle;
    return calibrated;
}
void Adc::begin(void *params) {
    auto adc = (Adc *) params;

    adc->buffers[0] = new Buffer();
    adc->buffers[1] = new Buffer();
    adc->buffers[2] = new Buffer();
    adc->buffers[3] = new Buffer();
//    adc->buffers[4] = new Buffer();

//
//    adc_oneshot_unit_handle_t adc1_handle;
//    adc_oneshot_unit_init_cfg_t init_config1 = {
//            .unit_id = ADC_UNIT_1,
//    };
//    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));
//
//    //-------------ADC1 Config---------------//
//    adc_oneshot_chan_cfg_t config = {
//            .atten = ADC_ATTEN_DB_6,
//            .bitwidth = ADC_BITWIDTH_12,
//    };
//    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_3, &config));
//    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_4, &config));
//    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_5, &config));
//    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_6, &config));
//    adc_cali_handle_t s;
//    example_adc_calibration_init(ADC_UNIT_1, ADC_ATTEN_DB_6, &s);
//
//    while (1) {
//        int buf = 0;
//        if (xSemaphoreTake(adcMutex, 1) != pdTRUE) {
//            continue;
//        }
//        esp_err_t err;
//        err = adc_oneshot_read(adc1_handle, ADC_CHANNEL_3, &buf);
//        if (err != ESP_OK) {
//            printf("%s\n", esp_err_to_name(err));
//        }
//        adc_cali_raw_to_voltage(s, buf, &buf);
//        adc->buffers[0]->push(buf);
//        err = adc_oneshot_read(adc1_handle, ADC_CHANNEL_4, &buf);
//        if (err != ESP_OK) {
//            printf("%s\n", esp_err_to_name(err));
//        }
//        adc_cali_raw_to_voltage(s, buf, &buf);
//        adc->buffers[1]->push(buf);
//        err = adc_oneshot_read(adc1_handle, ADC_CHANNEL_5, &buf);
//        if (err != ESP_OK) {
//            printf("%s\n", esp_err_to_name(err));
//        }
//        adc_cali_raw_to_voltage(s, buf, &buf);
//        adc->buffers[2]->push(buf);
//        if (err != ESP_OK) {
//            printf("%s\n", esp_err_to_name(err));
//        }
//        err = adc_oneshot_read(adc1_handle, ADC_CHANNEL_6, &buf);
//        if (err != ESP_OK) {
//            printf("%s\n", esp_err_to_name(err));
//        }
//        adc_cali_raw_to_voltage(s, buf, &buf);
//        adc->buffers[3]->push(buf);
//        xSemaphoreGive(adcMutex);
//        vTaskDelay(pdMS_TO_TICKS(0.25));
//    }
//    return;

//    esp_err_t err;
    //zero-initialize the config structure.
    gpio_config_t io_conf = {};
    //disable interrupt
    io_conf.intr_type = GPIO_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_INPUT;
    //bit mask of the pins that you want to set,e.g.GPIO18/19
    io_conf.pin_bit_mask = 1ULL << GPIO_NUM_10;
    //disable pull-down mode
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    //disable pull-up mode
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    //configure GPIO with the given settings
    gpio_config(&io_conf);

    adc->running = true;


    uint32_t read = 0;
    uint8_t result[512] = {0};
    memset(result, 0xcc, 512);

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

    int64_t start = micros();
    while (adc->running) {

        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        while (true) {
            err = adc_continuous_read(adc->handle, result, adc->bufferSize, &read, 0);
            if (err == ESP_ERR_INVALID_STATE) {
                printf("Bad state\n");
                break;
            } else if (err != ESP_OK) {
                break;
            }

            if (xSemaphoreTake(adcMutex, portMAX_DELAY) == pdTRUE) {
//                adc->buffers[4]->push(gpio_get_level(GPIO_NUM_10)*128);
                for (int i = 0; i < read; i += SOC_ADC_DIGI_RESULT_BYTES) {
                    auto *out = (adc_digi_output_data_t *) &result[i];
                    uint32_t channel = out->type2.channel;
                    uint32_t value = out->type2.data;

                    int j = CHANNEL_TO_INDEX(channel);

                    if (j < 0 || j >= 4) {
                        continue;
                    }
                    int v = 0;
                    adc_cali_raw_to_voltage(calHandle, makeSigned(value), &v);
//                sampleBuffer[j] += makeSigned(value);
//                sampleCount[j] += 1;
                    adc->buffers[j]->push(v);
                    // Push the current timing rate for reference

//                    ejections++;
                }



//                cycles++;
                xSemaphoreGive(adcMutex);
            }

//                for (int j = 0; j < adc->conf.channels.count; ++j) {
//                    if (sampleCount[j] > 0) {
//                        adc->buffers[j]->push(sampleBuffer[j]);
//                    }
//                }

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
//
//            if (micros() - start > 1000 * 1000) {
//                double duration = ((double) (micros() - start) / 1000.0 / 1000.0);
////                printf("ADC: %.2f fps, %.2f samples/s | %lu | %lu B",
////                       (double) ((double) cycles / duration),
////                       (double) ((double) ejections / duration), read, esp_get_minimum_free_heap_size());
////                for (int j = 0; j < adc->conf.channels.count; ++j) {
////                    printf("b[%d] = %d, ", j, adc->buffers[j]->numBuffers());
////                }
////                printf("\n");
//                cycles = 0;
//                ejections = 0;
//                start = micros();
//            }

        }

    }


}

void Adc::capture(int us) {
    this->cap = micros() + us;
    while (this->cap > micros()) {

    }
    this->cap = 0;
}

Adc::~Adc() {
    esp_err_t err;
    printf("ADC Shutting down components...\n");
    err = adc_continuous_stop(handle);
    if (err != ESP_OK) {
        printf("ADC Register failed!\n");
        return;
    }

    err = adc_continuous_deinit(handle);
    if (err != ESP_OK) {
        printf("ADC Register failed!\n");
        return;
    }
    vTaskSuspend(runner);
    vTaskSuspend(watcher);

}




