//
// Created by Braden Nicholson on 4/11/23.
//

#ifndef RADAR_DAC_H
#define RADAR_DAC_H


#include <driver/spi_master.h>
#include <driver/gptimer_types.h>
#include <freertos/ringbuf.h>

class DAC {
public:
    DAC(TaskHandle_t handle, RingbufHandle_t pVoid);

    void enable();

    void disable();

    spi_device_handle_t device{};
    TaskHandle_t adcHandle{};
    RingbufHandle_t rbHandle{};

private:

    void powerEnable();

    void powerDisable();

    void loadOn();

    void loadOff();

    void setupDAC();



    void send(uint16_t channel_a, uint16_t channel_b);


//    static bool updateAnalog(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *params);
};


#endif //RADAR_DAC_H
