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

#define SPI_MOSI CONFIG_vRADAR_DAC_SDI
#define SPI_SCLK CONFIG_vRADAR_DAC_SCK

#define SPI_SHDN ((gpio_num_t) CONFIG_vRADAR_DAC_SHDN)
#define SPI_LDAC ((gpio_num_t) CONFIG_vRADAR_DAC_LDAC)
#define SPI_CS CONFIG_vRADAR_DAC_CS

#define SPI_HOST SPI2_HOST
#define SPI_CLOCK 20000000

#define CONFIG_UPDATE_PERIOD (1000 * 1000) // 1s

esp_err_t DAC::initializeSPI() {
    // Define configuration for the Shutdown and DAC latch GPIO pins
    gpio_config_t gpioConfig = {
            .pin_bit_mask = (1ULL << SPI_SHDN) | (1ULL << SPI_LDAC),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
    };
    // Configure the GPIO pads
    gpio_config(&gpioConfig);
    // Enable the MCP4922 IC
    gpio_set_level(SPI_SHDN, 1);
    // Initialize the latch pin
    gpio_set_level(SPI_LDAC, 0);
    // Define the SPI bus configuration
    spi_bus_config_t spiBusConfig = {
            .mosi_io_num = SPI_MOSI,
            .miso_io_num = -1,
            .sclk_io_num = SPI_SCLK,
            .quadwp_io_num = -1,
            .quadhd_io_num = -1,
            .max_transfer_sz = 0
    };
    // Define the device configuration for the MCP4922
    spi_device_interface_config_t deviceInterfaceConfig = {
            .mode = 0,
            .clock_speed_hz = SPI_CLOCK,
            .spics_io_num = SPI_CS,
            .queue_size = 10
    };
    // Attempt to initialize the SPI bus
    esp_err_t ret = spi_bus_initialize(SPI_HOST, &spiBusConfig, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        return ret;
    }
    // Add the MCP4922 as an SPI device on the bus
    ret = spi_bus_add_device(SPI_HOST, &deviceInterfaceConfig, &device);
    if (ret != ESP_OK) {
        return ret;
    }
    return ESP_OK;
}

// 0001 ---- ---- ---- (Channel A, Unbuffered, 2x Gain, Enabled) + input data
// 1111 ---- ---- ---- (Channel B, Buffered, 1x Gain, Enabled) + input data
static void IRAM_ATTR transmit(spi_device_handle_t handle, uint16_t value, uint16_t mask) {
    uint16_t data = (value & 0x0FFF) | mask;
    gpio_set_level(SPI_LDAC, 1); // Set LDAC high

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

    gpio_set_level(SPI_LDAC, 0);
//    gpio_set_level(SPI_LDAC, 1);
//    gpio_set_level(SPI_LDAC, 0);
}

void configTimerCallback(void *params) {
    auto dac = (DAC *) params;
    auto settings = Settings::instance();
    auto system = settings.getSystem();
    dac->chirp = system.chirp;
    dac->audible = system.audible;
}

volatile int32_t alternate = 0;
volatile int32_t power = 0;

static bool IRAM_ATTR chirpTriggerCallback(gptimer_handle_t timer,
                                           const gptimer_alarm_event_data_t *_,
                                           void *user_ctx) {

    auto dac = (DAC *) user_ctx;

    power = (power + 1) % (125);

    if (power == 0) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        vTaskNotifyGiveFromISR(dac->adcHandle, &xHigherPriorityTaskWoken)
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }

    if (dac->chirp > 0) {
        // 0x3000 => [ch0, unbuffered, gain 1x, enabled]
        transmit(dac->device, (uint16_t) (power % dac->chirp), 0x3000);
    }

    if (dac->audible > 0) {
        alternate = (alternate + 1) % dac->audible;
        if (alternate == 0) {
            transmit(dac->device, (uint16_t) 2048, 0xF000);
        } else {
            transmit(dac->device, (uint16_t) 0, 0xF000);
        }
    }

    return true;
}

esp_err_t DAC::initializeChirpTimer() {

    chirpTimer = {};

    gptimer_config_t chirpTimerConfig = {
            .clk_src = GPTIMER_CLK_SRC_DEFAULT,
            .direction = GPTIMER_COUNT_UP,
            .resolution_hz = 1 * 1000 * 1000, // 1MHz, 1 tick = 1us
    };

    esp_err_t err;

    err = gptimer_new_timer(&chirpTimerConfig, &chirpTimer);
    if (err != ESP_OK) {
        return err;
    }

    gptimer_alarm_config_t alarm_on = {
            .alarm_count = 100, // period = 1s @resolution 1MHz
            .reload_count = 0, // counter will reload with 0 on alarm event
            .flags {
                    .auto_reload_on_alarm = true, // enable auto-reload
            }
    };

    err = gptimer_set_alarm_action(chirpTimer, &alarm_on);
    if (err != ESP_OK) {
        return err;
    }

    gptimer_event_callbacks_t alarm_on_callback = {
            .on_alarm = chirpTriggerCallback, // register user callback
    };

    err = gptimer_register_event_callbacks(chirpTimer, &alarm_on_callback, this);
    if (err != ESP_OK) {
        return err;
    }

    err = gptimer_enable(chirpTimer);
    if (err != ESP_OK) {
        return err;
    }

    err = gptimer_start(chirpTimer);
    if (err != ESP_OK) {
        return err;
    }

    return ESP_OK;
}

DAC::DAC(TaskHandle_t handle) : adcHandle(handle) {

    esp_err_t err;
    // Attempt to initialize the DAC SPI device
    err = initializeSPI();
    if (err != ESP_OK) {
        printf("Failed to initialize DAC SPI: %s\n", esp_err_to_name(err));
        return;
    }
    // Attempt to initialize the configuration timer
    err = initializeConfigurationTimer();
    if (err != ESP_OK) {
        printf("Failed to initialize configuration timer: %s\n", esp_err_to_name(err));
        return;
    }
    // Attempt to configure the chirp timer
    err = initializeChirpTimer();
    if (err != ESP_OK) {
        printf("Failed to initialize configuration timer: %s\n", esp_err_to_name(err));
        return;
    }
}

esp_err_t DAC::initializeConfigurationTimer() {
    configTimer = {};
    // Define the periodic configuration timer
    const esp_timer_create_args_t periodic_timer_args = {
            .callback = configTimerCallback,
            .arg = this,
            .name = "dacConfiguration"
    };
    esp_err_t err;
    // Initialize the periodic timer
    err = esp_timer_create(&periodic_timer_args, &configTimer);
    if (err != ESP_OK) {
        return err;
    }
    // Set the periodic timer to update every second
    err = esp_timer_start_periodic(configTimer, CONFIG_UPDATE_PERIOD);
    if (err != ESP_OK) {
        return err;
    }

    return ESP_OK;
}


DAC::~DAC() {
    esp_err_t err;

    err = esp_timer_stop(configTimer);
    if (err != ESP_OK) {
        printf("Failed to stop chirpTimer: %s\n", esp_err_to_name(err));
    }

    err = gptimer_stop(chirpTimer);
    if (err != ESP_OK) {
        printf("Failed to stop chirpTimer: %s\n", esp_err_to_name(err));
    }

    err = gptimer_del_timer(chirpTimer);
    if (err != ESP_OK) {
        printf("Failed to delete chirpTimer: %s\n", esp_err_to_name(err));
    }

    err = spi_bus_remove_device(device);
    if (err != ESP_OK) {
        printf("Failed to remove SPI device: %s\n", esp_err_to_name(err));
    }

    err = spi_bus_free(SPI_HOST);
    if (err != ESP_OK) {
        printf("Failed to free spi host: %s\n", esp_err_to_name(err));
    }

}


