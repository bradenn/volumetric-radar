//
// Created by Braden Nicholson on 3/5/23.
//

#include <esp_timer.h>
#include <driver/gpio.h>
#include <driver/pulse_cnt.h>
#include <driver/ledc.h>
#include <rom/ets_sys.h>
#include "controller.h"
//#include "analog.h"


//pcnt_unit_handle_t unit = nullptr;
//
//void counterTask(void *arg) {
//    auto adc = (Adc *) arg;
//    int pulse_count = 0;
//    ESP_ERROR_CHECK(pcnt_unit_get_count(unit, &pulse_count));
//    adc->buffers[4]->push(pulse_count);
//    pcnt_unit_clear_count(unit);
//}

// Chirp
void timerTask(void *arg) {
    gpio_set_level(GPIO_NUM_9, 1);
    usleep(10);
    gpio_set_level(GPIO_NUM_9, 0);
}


static bool example_pcnt_on_reach(pcnt_unit_handle_t unit, const pcnt_watch_event_data_t *edata, void *user_ctx) {
    BaseType_t high_task_wakeup;
    QueueHandle_t queue = (QueueHandle_t) user_ctx;
    // send event data to queue, from this interrupt callback
    xQueueSendFromISR(queue, &(edata->watch_point_value), &high_task_wakeup);
    return (high_task_wakeup == pdTRUE);
}

Controller::Controller(Adc* adc) {
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
/*
    pcnt_unit_config_t unit_config = {
            .low_limit = -1000,
            .high_limit = 1000,
    };

    pcnt_new_unit(&unit_config, &unit);
    pcnt_glitch_filter_config_t filter_config = {
            .max_glitch_ns = 1000,
    };
    pcnt_unit_set_glitch_filter(unit, &filter_config);

    pcnt_chan_config_t chan_a_config = {
            .edge_gpio_num = GPIO_NUM_8,
    };
    pcnt_channel_handle_t pcnt_chan = NULL;
    pcnt_new_channel(unit, &chan_a_config, &pcnt_chan);

    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan, PCNT_CHANNEL_EDGE_ACTION_INCREASE,
                                                 PCNT_CHANNEL_EDGE_ACTION_DECREASE));

    pcnt_event_callbacks_t cbs = {
            .on_reach = example_pcnt_on_reach,
    };
    QueueHandle_t queue = xQueueCreate(10, sizeof(int));
    ESP_ERROR_CHECK(pcnt_unit_register_event_callbacks(unit, &cbs, queue));

    ESP_ERROR_CHECK(pcnt_unit_enable(unit));
    ESP_ERROR_CHECK(pcnt_unit_clear_count(unit));
    ESP_ERROR_CHECK(pcnt_unit_start(unit));*/


    esp_timer_handle_t chirpTimer;
    esp_timer_create_args_t chirpArgs = {
            .callback = timerTask,
            .arg = adc,
            .name = "chirpTask",
    };
    esp_timer_create(&chirpArgs, &chirpTimer);
    esp_timer_start_periodic((esp_timer_handle_t) chirpTimer, 250);

//    esp_timer_handle_t counterTimer;
//    esp_timer_create_args_t counterArgs = {
//            .callback = counterTask,
//            .arg = adc,
//            .name = "counterTask",
//    };
//
//    esp_timer_create(&counterArgs, &counterTimer);
//    esp_timer_start_periodic((esp_timer_handle_t) counterTimer, 100);


}
