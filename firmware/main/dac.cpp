//
// Created by Braden Nicholson on 4/11/23.
//

#include <driver/gpio.h>
#include <driver/spi_master.h>
#include <freertos/task.h>
#include <driver/timer_types_legacy.h>
#include <esp_timer.h>
#include <driver/gptimer.h>
#include "dac.h"
#include "settings.h"
#include "runtime.h"

#define SPI_MOSI CONFIG_vRADAR_DAC_SDI
#define SPI_SCLK CONFIG_vRADAR_DAC_SCK

#define SPI_SHDN CONFIG_vRADAR_DAC_SHDN
#define SPI_LDAC CONFIG_vRADAR_DAC_LDAC
#define SPI_CS CONFIG_vRADAR_DAC_CS

#define SPI_CLOCK 20000000

#define DAC_CHIRP_DURATION 25000 // 25ms
#define DAC_RESOLUTION (2^12) // 4096 bit
#define DAC_TIMER_INTERVAL 100 // Called evert 100us

#define DAC_STEPS (DAC_CHIRP_DURATION/DAC_TIMER_INTERVAL)


void DAC::setupDAC() {
    gpio_config_t io_conf = {
            .pin_bit_mask = (1ULL << SPI_SHDN) | (1ULL << SPI_LDAC),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
    };

    gpio_config(&io_conf);

    powerEnable();
    loadOff();

    spi_bus_config_t buscfg = {
            .mosi_io_num = SPI_MOSI,
            .miso_io_num = -1, // Not used for MCP4922
            .sclk_io_num = SPI_SCLK,
            .quadwp_io_num = -1,
            .quadhd_io_num = -1,
            .max_transfer_sz = 0
    };

    spi_device_interface_config_t devcfg = {
            .mode = 0,
            .clock_speed_hz = SPI_CLOCK,
            .spics_io_num = SPI_CS,
            .queue_size = 2
    };

// Initialize the SPI bus
    esp_err_t ret = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        printf("Error with SPI Init: %s\n", esp_err_to_name(ret));
    }

// Add the MCP4922 as an SPI device on the bus
    ret = spi_bus_add_device(SPI2_HOST, &devcfg, &device);
    if (ret != ESP_OK) {
        printf("Error with SPI add device: %s\n", esp_err_to_name(ret));
    }

}

#define DELAY = (1000/4000)

static void IRAM_ATTR transmit(spi_device_handle_t handle, uint16_t value, uint16_t mask) {
    uint16_t data = (value & 0x0FFF) | mask;
    gpio_set_level(static_cast<gpio_num_t>(SPI_LDAC), 1); // Set LDAC high
//    gpio_set_level(static_cast<gpio_num_t>(SPI_CS), 0);   // Set CS low

    spi_transaction_t transaction = {
            .flags = SPI_TRANS_USE_TXDATA,
            .length = 16,
            .tx_data = {static_cast<uint8_t>(data >> 8), static_cast<uint8_t>(data & 0xFF)}
    };

    esp_err_t ret;
    spi_transaction_t *ret_trans;
    ret = spi_device_queue_trans(handle, &transaction, pdMS_TO_TICKS(0.1));
    if (ret != ESP_OK) return;

    ret = spi_device_get_trans_result(handle, &ret_trans, pdMS_TO_TICKS(0.1));
    if (ret != ESP_OK) {
    }

    gpio_set_level(static_cast<gpio_num_t>(SPI_LDAC), 0); // Set LDAC low
}

void IRAM_ATTR DAC::send(uint16_t channel_a, uint16_t channel_b) {
    // A/B Buffered/Unbuffered Gain Shutdown [DATA] [DATA] [DATA]
    // 0001 ---- ---- ---- (Channel A, Unbuffered, 2x Gain, Enabled) + input data
    uint16_t data_a = (channel_a & 0x0FFF) | 0x1000;
    // 1111 ---- ---- ---- (Channel B, Buffered, 1x Gain, Enabled) + input data
    uint16_t data_b = (channel_b & 0x0FFF) | 0xF000; // Truncated to 11 bits to prevent damaging speaker (1.5v)

    gpio_set_level(static_cast<gpio_num_t>(SPI_LDAC), 1); // Set LDAC high
//    gpio_set_level(static_cast<gpio_num_t>(SPI_CS), 0);   // Set CS low

// Transmit data for channel A
    spi_transaction_t trans_desc_a = {
            .flags = SPI_TRANS_USE_TXDATA,
            .length = 16, // 16 bits
            .tx_data = {static_cast<uint8_t>(data_a >> 8), static_cast<uint8_t>(data_a & 0xFF)}
    };
    esp_err_t ret = spi_device_transmit(device, &trans_desc_a);
    assert(ret == ESP_OK);

// Transmit data for channel B
    spi_transaction_t trans_desc_b = {
            .flags = SPI_TRANS_USE_TXDATA,
            .length = 16, // 16 bits
            .tx_data = {static_cast<uint8_t>(data_b >> 8), static_cast<uint8_t>(data_b & 0xFF)}
    };
    ret = spi_device_transmit(device, &trans_desc_b);
    assert(ret == ESP_OK);

//    gpio_set_level(static_cast<gpio_num_t>(SPI_CS), 1);   // Set CS high
    gpio_set_level(static_cast<gpio_num_t>(SPI_LDAC), 0); // Set LDAC low
//    gpio_set_level(static_cast<gpio_num_t>(SPI_LDAC), 1); // Set LDAC high
}

volatile int power = 0;
static bool dir = true;
// 7812.5us



static int loop = 0;
static int alternate = 0;

volatile int configAudible = 0;
volatile int configChirp = 1;

void periodic_timer_callback(void *params) {
    auto settings = Settings::instance();
    configChirp = (int) settings.system.chirp;
    configAudible = (int) settings.system.audible;
    printf("Configured DAC. %d %d\n", configChirp, configAudible);
}

volatile int sleepLoop = 0;

static int64_t startTime = 0;
static int64_t stopTime = 0;


static bool IRAM_ATTR dac_alarm(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx) {
    auto dac = (DAC *) user_ctx;

    // DAC Resolution: 4096 levels 0->0v, 4095->2.5v
    // Chirp duration: 1ms
    // Chirp VCO Frequency: 10khz

    power = (power + 32) % (4000);


//    if(power >= 4000) {
//        dir = false;
//        power = 4000;
//    }else if(power <= 0){
//        dir = true;
//        power = 0;
//    }


//    if (power == 0) {
//        loop = (loop + 1) % sleepLoop;
//        if(loop != 0) {
//            transmit(dac->device, 0, 0x1000);
//            return true;
//        }
//    }


    if (power == 0) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        vTaskNotifyGiveFromISR(dac->adcHandle, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
//    if (power == 0) {
//        startTime = esp_timer_get_time();
//    }

//    if (power == 4096) {
//        SampleData sd{};
//        stopTime = esp_timer_get_time();
//        sd.chirpStart = startTime;
//        sd.chirpStop = stopTime;
//        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
//
//// Allocate a buffer in the ring buffer
//        void *buf = xRingbufferMallocFromISR(dac->rbHandle, sizeof(sd));
//        if (buf != nullptr) {
//            // Copy the struct data into the buffer
//            memcpy(buf, &sd, sizeof(sd));
//
//            // Send the buffer to the ring buffer
//            if (xRingbufferSendFromISR(dac->rbHandle, buf, sizeof(sd), &xHigherPriorityTaskWoken) == pdTRUE) {
//                portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
//            } else {
//                // If the send fails, free the buffer to avoid memory leaks
//                vRingbufferFreeFromISR(dac->rbHandle, buf);
//            }
//        }
//    }


    transmit(dac->device, (uint16_t) (power/32)%25, 0x3000);
//    transmit(dac->device, (abs(2048 - ((power + 2048) % 4096)) * 2)/32, 0x3000);

    if (configAudible == 1) {
        alternate = (alternate + 1) % 32;
        if (alternate == 0) {
            transmit(dac->device, power / 2, 0xF000);
        }
    }


    return true;
}

DAC::DAC(TaskHandle_t handle, RingbufHandle_t pVoid) {
    adcHandle = handle;
    rbHandle = pVoid;
    setupDAC();
    auto settings = Settings::instance();
    configChirp = settings.system.chirp;
    configAudible = settings.system.audible;
    printf("Configuring DAC...\n");

    power = 0;

    gptimer_handle_t pulse_timer;
    gptimer_config_t pulse_timer_config = {
            .clk_src = GPTIMER_CLK_SRC_DEFAULT,
            .direction = GPTIMER_COUNT_UP,
            .resolution_hz = 1 * 1000 * 1000, // 1MHz, 1 tick = 1us
    };
    ESP_ERROR_CHECK(gptimer_new_timer(&pulse_timer_config, &pulse_timer));

    gptimer_alarm_config_t alarm_on = {
            .alarm_count = 100, // period = 1s @resolution 1MHz
            .reload_count = 0, // counter will reload with 0 on alarm event
            .flags {
                    .auto_reload_on_alarm = true, // enable auto-reload
            }
    };
    ESP_ERROR_CHECK(gptimer_set_alarm_action(pulse_timer, &alarm_on));

    gptimer_event_callbacks_t alarm_on_callback = {
            .on_alarm = dac_alarm, // register user callback
    };

    ESP_ERROR_CHECK(gptimer_register_event_callbacks(pulse_timer, &alarm_on_callback, this));
    ESP_ERROR_CHECK(gptimer_enable(pulse_timer));
    ESP_ERROR_CHECK(gptimer_start(pulse_timer));

}

void DAC::enable() {
    powerEnable();
}

void DAC::disable() {
    powerDisable();
}

void DAC::powerEnable() {
    gpio_set_level(static_cast<gpio_num_t>(SPI_SHDN), 1);
}

void DAC::powerDisable() {
    gpio_set_level(static_cast<gpio_num_t>(SPI_SHDN), 0);
}

void DAC::loadOn() {
    gpio_set_level(static_cast<gpio_num_t>(SPI_LDAC), 1);
}

void DAC::loadOff() {
    gpio_set_level(static_cast<gpio_num_t>(SPI_LDAC), 0);
}

