
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "runtime.h"

extern "C" void app_main() {
//    auto in = Indicator();
//    in.setIndicator(POWER, true);
    auto runtime = Runtime();

//    in.setIndicator(LINK, true);
//    uint32_t *buffer = (uint32_t *) heap_caps_malloc(sizeof(uint32_t), MALLOC_CAP_SPIRAM);
//
//    printf("%lu\n", *buffer);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
