//
// Created by Braden Nicholson on 3/5/23.
//

#include <esp_timer.h>
#include <driver/gpio.h>
#include <rom/ets_sys.h>
#include <driver/pulse_cnt.h>
#include <driver/gptimer.h>
#include <esp_adc/adc_oneshot.h>
#include "controller.h"

static bool pulse_off(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx) {
    gpio_set_level(GPIO_NUM_9, 0);
    return false;
}

static bool pulse_on(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx) {
    gpio_set_level(GPIO_NUM_9, 1);
    return false;
}

adc_oneshot_unit_handle_t adc1_handle;

static bool adc_read(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx) {
    int out = 0;
    adc_oneshot_read(adc1_handle, ADC_CHANNEL_4, &out);
    return false;
}


Controller::Controller(Adc *adc, uint32_t prf, uint32_t pulse) {
    printf("Controller: PRF: %lu, Pulse: %lu\n", prf, pulse);
    esp_err_t err;
    //zero-initialize the config structure.
    gpio_config_t io_conf = {};
    //disable interrupt
    io_conf.intr_type = GPIO_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set,e.g.GPIO18/19
    io_conf.pin_bit_mask = 1ULL << GPIO_NUM_9;
    //disable pull-down mode
    io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
    //disable pull-up mode
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    //configure GPIO with the given settings
    gpio_config(&io_conf);

    gptimer_handle_t pulse_timer;
    gptimer_config_t pulse_timer_config = {
            .clk_src = GPTIMER_CLK_SRC_DEFAULT,
            .direction = GPTIMER_COUNT_UP,
            .resolution_hz = 1 * 1000 * 1000, // 1MHz, 1 tick = 1us
    };
    ESP_ERROR_CHECK(gptimer_new_timer(&pulse_timer_config, &pulse_timer));

    gptimer_handle_t pulse_timer_off;
    gptimer_config_t pulse_timer_config_off = {
            .clk_src = GPTIMER_CLK_SRC_DEFAULT,
            .direction = GPTIMER_COUNT_UP,
            .resolution_hz = 1 * 1000 * 1000, // 1MHz, 1 tick = 1us
    };
    ESP_ERROR_CHECK(gptimer_new_timer(&pulse_timer_config_off, &pulse_timer_off));

//    gptimer_handle_t adc_timer;
//    gptimer_config_t adc_timer_config = {
//            .clk_src = GPTIMER_CLK_SRC_DEFAULT,
//            .direction = GPTIMER_COUNT_UP,
//            .resolution_hz = 1 * 1000 * 1000, // 1MHz, 1 tick = 1us
//    };
//    ESP_ERROR_CHECK(gptimer_new_timer(&adc_timer_config, &adc_timer));

    gptimer_alarm_config_t alarm_on = {
            .alarm_count = prf, // period = 1s @resolution 1MHz
            .reload_count = 0, // counter will reload with 0 on alarm event
            .flags {
                    .auto_reload_on_alarm = true, // enable auto-reload
            }
    };
    ESP_ERROR_CHECK(gptimer_set_alarm_action(pulse_timer, &alarm_on));

    gptimer_alarm_config_t alarm_off = {
            .alarm_count = prf + pulse, // period = 1s @resolution 1MHz
            .reload_count = 0, // counter will reload with 0 on alarm event
            .flags {
                    .auto_reload_on_alarm = true, // enable auto-reload
            }
    };
//
//    gptimer_alarm_config_t adc_alarm = {
//            .alarm_count = 12, // period = 1s @resolution 1MHz
//            .reload_count = 0, // counter will reload with 0 on alarm event
//            .flags {
//                    .auto_reload_on_alarm = true, // enable auto-reload
//            }
//    };
//

    ESP_ERROR_CHECK(gptimer_set_alarm_action(pulse_timer_off, &alarm_off));
//    ESP_ERROR_CHECK(gptimer_set_alarm_action(adc_timer, &adc_alarm));

    gptimer_event_callbacks_t alarm_on_callback = {
            .on_alarm = pulse_on, // register user callback
    };

    gptimer_event_callbacks_t alarm_off_callback = {
            .on_alarm = pulse_off, // register user callback
    };

//    gptimer_event_callbacks_t adc_alarm_callback = {
//            .on_alarm = adc_read, // register user callback
//    };

    ESP_ERROR_CHECK(gptimer_register_event_callbacks(pulse_timer, &alarm_on_callback, nullptr));
    ESP_ERROR_CHECK(gptimer_enable(pulse_timer));

    ESP_ERROR_CHECK(gptimer_register_event_callbacks(pulse_timer_off, &alarm_off_callback, nullptr));
    ESP_ERROR_CHECK(gptimer_enable(pulse_timer_off));
//
//    ESP_ERROR_CHECK(gptimer_register_event_callbacks(adc_timer, &adc_alarm_callback, nullptr));
//    ESP_ERROR_CHECK(gptimer_enable(adc_timer));



    adc_oneshot_unit_init_cfg_t init_config1 = {
            .unit_id = ADC_UNIT_1,
            .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));
    adc_oneshot_chan_cfg_t config = {
            .atten = ADC_ATTEN_DB_0,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_4, &config));

    ESP_ERROR_CHECK(gptimer_start(pulse_timer));
    ESP_ERROR_CHECK(gptimer_start(pulse_timer_off));
//    ESP_ERROR_CHECK(gptimer_start(adc_timer));

}
