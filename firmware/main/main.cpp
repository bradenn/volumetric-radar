
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "runtime.h"

extern "C" void app_main()
{
//    auto in = Indicator();
//    in.setIndicator(POWER, true);
    auto runtime = Runtime();

//    in.setIndicator(LINK, true);

    while(1){
        vTaskDelay(1);
    }
}
