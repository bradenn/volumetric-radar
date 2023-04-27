//
// Created by Braden Nicholson on 4/19/23.
//

#include "gyro.h"
#include "settings.h"

#define I2C_MASTER_SDA_IO CONFIG_vRADAR_GYRO_SDA
#define I2C_MASTER_SCL_IO CONFIG_vRADAR_GYRO_SCL

#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_FREQ_HZ 100000
#define LSM6DSM_I2C_ADDRESS 0x6B
#define GYRO_CONFIG_UPDATE_PERIOD (1000*1000)
#define GYRO_POLL_PERIOD (1000*500)

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

esp_err_t Gyro::initializeGyroDevice() {

    i2c_config_t conf = {
            .mode = I2C_MODE_MASTER,
            .sda_io_num = I2C_MASTER_SDA_IO,
            .scl_io_num = I2C_MASTER_SCL_IO,
            .sda_pullup_en = GPIO_PULLUP_ENABLE,
            .scl_pullup_en = GPIO_PULLUP_ENABLE,
    };

    conf.master.clk_speed = I2C_MASTER_FREQ_HZ, i2c_param_config(I2C_MASTER_NUM, &conf);

    esp_err_t err;
    err = i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
    if (err != ESP_OK) {
        return err;
    }

    auto *p = (i2c_port_t *) malloc(sizeof(i2c_port_t));
    if (p == nullptr) {
        printf("Failed to allocate memory for i2c_port_t\n");
        return ESP_ERR_NO_MEM;
    }

    *p = I2C_NUM_0;

    device = {};
    device.write_reg = platform_write;
    device.read_reg = platform_read;
    device.handle = (void *) p;

    // Configure Accelerometer data rate
    lsm6dsm_xl_data_rate_set(&device, LSM6DSM_XL_ODR_104Hz);
    lsm6dsm_xl_full_scale_set(&device, LSM6DSM_2g);

    // Configure gyroscope data rate
    lsm6dsm_gy_data_rate_set(&device, LSM6DSM_GY_ODR_104Hz);
    lsm6dsm_gy_full_scale_set(&device, LSM6DSM_2000dps);

    return ESP_OK;


}

void lsm6dsm_read_data(stmdev_ctx_t *dev_ctx,
                       int16_t *ax, int16_t *ay, int16_t *az, int16_t *gx, int16_t *gy, int16_t *gz) {

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
    float scale = 0.061;

    float ax_g = (float) ax * scale / 1000.0f; // Convert to g (assuming ±2g scale)
    float ay_g = (float) ay * scale / 1000.0f; // Convert to g (assuming ±2g scale)
    float az_g = (float) az * scale / 1000.0f; // Convert to g (assuming ±2g scale)

    *roll = (float) atan2(-ay_g, -az_g) * 180.0f / (float) M_PI;
    *pitch = (float) atan2(ax_g, sqrt(ay_g * ay_g + az_g * az_g)) * 180.0f / (float) M_PI;
}

void Gyro::configTimerCallback(void *params) {
    auto *gyro = (Gyro *) params;

    auto settings = Settings::instance();
    auto system = settings.getSystem();

    if (gyro->rate != system.gyro) {
        esp_err_t err;

        err = esp_timer_stop(gyro->taskTimer);
        if (err != ESP_OK) {
            printf("Failed to stop periodic taskTimer: %s\n", esp_err_to_name(err));
            return;
        }

        err = esp_timer_start_periodic(gyro->taskTimer, gyro->rate * 1000);
        if (err != ESP_OK) {
            printf("Failed to start periodic taskTimer: %s\n", esp_err_to_name(err));
            return;
        }

        gyro->rate = system.gyro;

    }


}

void Gyro::taskTimerCallback(void *params) {
    auto *gyro = (Gyro *) params;

    if (gyro->rate <= 0) return;

    int16_t ax, ay, az, gx, gy, gz;
    float roll, pitch;

    GyroData data{};

    lsm6dsm_read_data(&gyro->device, &ax, &ay, &az, &gx, &gy, &gz);
    calculate_angles(ax, ay, az, &roll, &pitch);

    data.roll = roll;
    data.pitch = pitch;

    xRingbufferSend(gyro->ringBuffer, (void *) &data, sizeof(GyroData), 0);
}

esp_err_t Gyro::initializeConfigurationTimer() {
    configTimer = {};
    // Define the periodic configuration timer
    const esp_timer_create_args_t periodic_timer_args = {
            .callback = configTimerCallback,
            .arg = this,
            .name = "gyroConfiguration"
    };
    esp_err_t err;
    // Initialize the periodic timer
    err = esp_timer_create(&periodic_timer_args, &configTimer);
    if (err != ESP_OK) {
        return err;
    }
    // Set the periodic timer to update every second
    err = esp_timer_start_periodic(configTimer, GYRO_CONFIG_UPDATE_PERIOD);
    if (err != ESP_OK) {
        return err;
    }

    return ESP_OK;
}

esp_err_t Gyro::initializeTaskTimer() {
    taskTimer = {};
    rate = GYRO_POLL_PERIOD / 1000;
    // Define the periodic configuration timer
    const esp_timer_create_args_t periodic_timer_args = {
            .callback = taskTimerCallback,
            .arg = this,
            .name = "gyroTask"
    };

    esp_err_t err;

    // Initialize the periodic timer
    err = esp_timer_create(&periodic_timer_args, &taskTimer);
    if (err != ESP_OK) {
        return err;
    }

    // Set the periodic timer to update every second
    err = esp_timer_start_periodic(taskTimer, rate * 1000);
    if (err != ESP_OK) {
        return err;
    }

    return ESP_OK;
}

Gyro::Gyro(RingbufHandle_t handle) : ringBuffer(handle) {

    esp_err_t err;

    err = initializeGyroDevice();
    if (err != ESP_OK) {
        printf("Failed to initialize gyroscope: %s\n", esp_err_to_name(err));
        return;
    }

    err = initializeConfigurationTimer();
    if (err != ESP_OK) {
        printf("Failed to initialize config timers: %s\n", esp_err_to_name(err));
        return;
    }

    err = initializeTaskTimer();
    if (err != ESP_OK) {
        printf("Failed to initialize task timer: %s\n", esp_err_to_name(err));
        return;
    }

}

Gyro::~Gyro() {

    esp_err_t err;
    err = esp_timer_stop(configTimer);
    if (err != ESP_OK) {
        printf("Failed to stop configuration timer: %s\n", esp_err_to_name(err));
    }

    err = esp_timer_stop(taskTimer);
    if (err != ESP_OK) {
        printf("Failed to stop task timer: %s\n", esp_err_to_name(err));
    }

    err = i2c_master_stop(device.handle);
    if (err != ESP_OK) {
        printf("Failed to stop i2c master bus: %s\n", esp_err_to_name(err));
    }

    err = i2c_driver_delete(I2C_MASTER_NUM);
    if (err != ESP_OK) {
        printf("Failed to initialize the i2c driver: %s\n", esp_err_to_name(err));
    }

}
