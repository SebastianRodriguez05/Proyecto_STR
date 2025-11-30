#include "config_storage.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"

static const char *TAG = "CONFIG_STORAGE";

void config_storage_init(void) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGI(TAG, "NVS partition truncated, erasing...");
        nvs_flash_erase();
        nvs_flash_init();
    }
    ESP_LOGI(TAG, "NVS initialized");
}

void config_storage_save_manual_pwm(uint8_t pwm) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        nvs_set_u8(handle, "manual_pwm", pwm);
        nvs_commit(handle);
        nvs_close(handle);
        ESP_LOGI(TAG, "Manual PWM saved: %d%%", pwm);
    }
}

uint8_t config_storage_get_manual_pwm(void) {
    nvs_handle_t handle;
    uint8_t pwm = 0;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_OK) {
        nvs_get_u8(handle, "manual_pwm", &pwm);
        nvs_close(handle);
    }
    return pwm;
}

void config_storage_save_mode(uint8_t mode) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        nvs_set_u8(handle, "mode", mode);
        nvs_commit(handle);
        nvs_close(handle);
        ESP_LOGI(TAG, "Mode saved: %d", mode);
    }
}

uint8_t config_storage_get_mode(void) {
    nvs_handle_t handle;
    uint8_t mode = 0;  // Default: Manual
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_OK) {
        nvs_get_u8(handle, "mode", &mode);
        nvs_close(handle);
    }
    return mode;
}

void config_storage_reset(void) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        nvs_erase_all(handle);
        nvs_commit(handle);
        nvs_close(handle);
        ESP_LOGI(TAG, "Configuration reset");
    }
}