//
// Created by Braden Nicholson on 4/19/23.
//

#include "gyro.h"

#include <stdio.h>
#include <esp_log.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "lsm6dsm_reg.h"

#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_SDA_IO 1
#define I2C_MASTER_SCL_IO 2
#define I2C_MASTER_FREQ_HZ 100000
#define LSM6DSM_I2C_ADDRESS 0x6B

static const char *TAG = "LSM6DSM";

static int32_t platform_write(void *handle, uint8_t reg, const uint8_t *bufp,
                              uint16_t len);

static int32_t platform_read(void *handle, uint8_t reg, uint8_t *bufp,
                             uint16_t len);

int32_t platform_read(void *handle, uint8_t reg, uint8_t *bufp, uint16_t len) {
    i2c_port_t i2c_num = *((i2c_port_t *) handle);

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (LSM6DSM_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (LSM6DSM_I2C_ADDRESS << 1) | I2C_MASTER_READ, true);
    i2c_master_read(cmd, bufp, len, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(i2c_num, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);

    return ret;
}

int32_t platform_write(void *handle, uint8_t reg, const uint8_t *bufp, uint16_t len) {
    i2c_port_t i2c_num = *((i2c_port_t *) handle);

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (LSM6DSM_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_write(cmd, bufp, len, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(i2c_num, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);

    return ret;
}

stmdev_ctx_t i2c_master_init() {
    i2c_config_t conf = {
            .mode = I2C_MODE_MASTER,
            .sda_io_num = I2C_MASTER_SDA_IO,
            .scl_io_num = I2C_MASTER_SCL_IO,
            .sda_pullup_en = GPIO_PULLUP_ENABLE,
            .scl_pullup_en = GPIO_PULLUP_ENABLE,
    };
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ,
            i2c_param_config(I2C_MASTER_NUM, &conf);
    i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
    auto *p = static_cast<i2c_port_t *>(malloc(sizeof(i2c_port_t)));
    if (p == nullptr) {
        // Handle memory allocation error
        printf("Failed to allocate memory for i2c_port_t\n");
        // You should return an error or an invalid dev_ctx in this case
    }
    *p = I2C_NUM_0;
    // Configure LSM6DSM registers using the library
    stmdev_ctx_t dev_ctx;
    dev_ctx.write_reg = platform_write;
    dev_ctx.read_reg = platform_read;
    dev_ctx.handle = (void *) p;

    lsm6dsm_xl_data_rate_set(&dev_ctx, LSM6DSM_XL_ODR_104Hz);
    lsm6dsm_xl_full_scale_set(&dev_ctx, LSM6DSM_2g);
    lsm6dsm_gy_data_rate_set(&dev_ctx, LSM6DSM_GY_ODR_104Hz);
    lsm6dsm_gy_full_scale_set(&dev_ctx, LSM6DSM_2000dps);

    return dev_ctx;

}

void
lsm6dsm_read_data(stmdev_ctx_t *dev_ctx, int16_t *ax, int16_t *ay, int16_t *az, int16_t *gx, int16_t *gy, int16_t *gz) {
    uint8_t data_raw_accel[6];
    uint8_t data_raw_gyro[6];

    // Read accelerometer and gyroscope data
    lsm6dsm_acceleration_raw_get(dev_ctx, reinterpret_cast<int16_t *>(data_raw_accel));
    lsm6dsm_angular_rate_raw_get(dev_ctx, reinterpret_cast<int16_t *>(data_raw_gyro));

    // Convert raw data to int16_t
    *ax = (int16_t) (data_raw_accel[0] | (data_raw_accel[1] << 8));
    *ay = (int16_t) (data_raw_accel[2] | (data_raw_accel[3] << 8));
    *az = (int16_t) (data_raw_accel[4] | (data_raw_accel[5] << 8));
    *gx = (int16_t) (data_raw_gyro[0] | (data_raw_gyro[1] << 8));
    *gy = (int16_t) (data_raw_gyro[2] | (data_raw_gyro[3] << 8));
    *gz = (int16_t) (data_raw_gyro[4] | (data_raw_gyro[5] << 8));
}


void calculate_angles(int16_t ax, int16_t ay, int16_t az, float *roll, float *pitch) {
    float ax_g = ax * 0.061 / 1000.0; // Convert to g (assuming ±4g scale)
    float ay_g = ay * 0.061 / 1000.0; // Convert to g (assuming ±4g scale)
    float az_g = az * 0.061 / 1000.0; // Convert to g (assuming ±4g scale)

    *roll = atan2(-ay_g, -az_g) * 180.0 / M_PI;
    *pitch = atan2(ax_g, sqrt(ay_g * ay_g + az_g * az_g)) * 180.0 / M_PI;
}

struct GyroContext {
    stmdev_ctx_t ctx{};
    RingbufHandle_t handle{};
};

void Gyro::lsm6dsm_task(void *pvParameters) {
    auto ctx = (GyroContext *) pvParameters;
    int16_t ax, ay, az, gx, gy, gz;
    float roll, pitch;
    GyroData data{};
    while (true) {
        lsm6dsm_read_data(&ctx->ctx, &ax, &ay, &az, &gx, &gy, &gz);
        calculate_angles(ax, ay, az, &roll, &pitch);
        data.roll = roll;
        data.pitch = pitch;
        xRingbufferSend(ctx->handle, (void *) &data, sizeof(GyroData), 0);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

GyroContext gc = {};

Gyro::Gyro(RingbufHandle_t handle) {
    auto ctx = i2c_master_init();
    gc = {
            .ctx = ctx,
            .handle = handle
    };
    xTaskCreatePinnedToCore(lsm6dsm_task, "lsm6dsm_task", 4096, &gc, 5, nullptr, 1);

}
