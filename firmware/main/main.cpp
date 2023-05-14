
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "runtime.h"

extern "C" void app_main() {
    auto runtime = Runtime();

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
