#include "config_storage.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"

static const char *TAG = "CONFIG_STORAGE";

/**
 * NOTA:
 * Se asume que NVS_NAMESPACE está definido en config_storage.h,
 * por ejemplo:
 *   #define NVS_NAMESPACE "fan_cfg"
 */

// Valores por defecto para modo automático
#define AUTO_TMIN_DEFAULT   24   // °C
#define AUTO_TMAX_DEFAULT   28   // °C

void config_storage_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGI(TAG, "NVS partition truncated, erasing...");
        nvs_flash_erase();
        nvs_flash_init();
    }
    ESP_LOGI(TAG, "NVS initialized");
}

/* ============================================================
 *  MODO MANUAL: PWM guardado
 * ============================================================ */

void config_storage_save_manual_pwm(uint8_t pwm)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        nvs_set_u8(handle, "manual_pwm", pwm);
        nvs_commit(handle);
        nvs_close(handle);
        ESP_LOGI(TAG, "Manual PWM saved: %d%%", pwm);
    } else {
        ESP_LOGE(TAG, "Failed to open NVS for manual_pwm save: %s", esp_err_to_name(err));
    }
}

uint8_t config_storage_get_manual_pwm(void)
{
    nvs_handle_t handle;
    uint8_t pwm = 0;   // default 0%
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_OK) {
        esp_err_t err_get = nvs_get_u8(handle, "manual_pwm", &pwm);
        if (err_get != ESP_OK && err_get != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGE(TAG, "Error reading manual_pwm: %s", esp_err_to_name(err_get));
        }
        nvs_close(handle);
    } else {
        ESP_LOGE(TAG, "Failed to open NVS for manual_pwm get: %s", esp_err_to_name(err));
    }
    return pwm;
}

/* ============================================================
 *  MODO GENERAL: modo de operación (0=Manual, 1=Auto, 2=Prog)
 * ============================================================ */

void config_storage_save_mode(uint8_t mode)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        nvs_set_u8(handle, "mode", mode);
        nvs_commit(handle);
        nvs_close(handle);
        ESP_LOGI(TAG, "Mode saved: %d", mode);
    } else {
        ESP_LOGE(TAG, "Failed to open NVS for mode save: %s", esp_err_to_name(err));
    }
}

uint8_t config_storage_get_mode(void)
{
    nvs_handle_t handle;
    uint8_t mode = 0;  // Default: Manual
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_OK) {
        esp_err_t err_get = nvs_get_u8(handle, "mode", &mode);
        if (err_get == ESP_ERR_NVS_NOT_FOUND) {
            // Si no existe, dejamos 0 (manual) sin log de error fuerte
            ESP_LOGI(TAG, "Mode key not found, using default (0=manual)");
        } else if (err_get != ESP_OK) {
            ESP_LOGE(TAG, "Error reading mode: %s", esp_err_to_name(err_get));
        }
        nvs_close(handle);
    } else {
        ESP_LOGE(TAG, "Failed to open NVS for mode get: %s", esp_err_to_name(err));
    }
    return mode;
}

/* ============================================================
 *  MODO AUTOMÁTICO: T_min y T_max
 * ============================================================ */

void config_storage_save_auto_tmin(int tmin)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        nvs_set_i32(handle, "auto_tmin", tmin);
        nvs_commit(handle);
        nvs_close(handle);
        ESP_LOGI(TAG, "Auto T_min saved: %d C", tmin);
    } else {
        ESP_LOGE(TAG, "Failed to open NVS for auto_tmin save: %s", esp_err_to_name(err));
    }
}

void config_storage_save_auto_tmax(int tmax)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        nvs_set_i32(handle, "auto_tmax", tmax);
        nvs_commit(handle);
        nvs_close(handle);
        ESP_LOGI(TAG, "Auto T_max saved: %d C", tmax);
    } else {
        ESP_LOGE(TAG, "Failed to open NVS for auto_tmax save: %s", esp_err_to_name(err));
    }
}

int config_storage_get_auto_tmin(void)
{
    nvs_handle_t handle;
    int32_t tmin = AUTO_TMIN_DEFAULT;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_OK) {
        esp_err_t err_get = nvs_get_i32(handle, "auto_tmin", &tmin);
        if (err_get == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGI(TAG, "auto_tmin not found, using default %d C", AUTO_TMIN_DEFAULT);
        } else if (err_get != ESP_OK) {
            ESP_LOGE(TAG, "Error reading auto_tmin: %s", esp_err_to_name(err_get));
        }
        nvs_close(handle);
    } else {
        ESP_LOGE(TAG, "Failed to open NVS for auto_tmin get: %s", esp_err_to_name(err));
    }
    return (int)tmin;
}

int config_storage_get_auto_tmax(void)
{
    nvs_handle_t handle;
    int32_t tmax = AUTO_TMAX_DEFAULT;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_OK) {
        esp_err_t err_get = nvs_get_i32(handle, "auto_tmax", &tmax);
        if (err_get == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGI(TAG, "auto_tmax not found, using default %d C", AUTO_TMAX_DEFAULT);
        } else if (err_get != ESP_OK) {
            ESP_LOGE(TAG, "Error reading auto_tmax: %s", esp_err_to_name(err_get));
        }
        nvs_close(handle);
    } else {
        ESP_LOGE(TAG, "Failed to open NVS for auto_tmax get: %s", esp_err_to_name(err));
    }
    return (int)tmax;
}

/* ============================================================
 *  RESET COMPLETO
 * ============================================================ */

void config_storage_reset(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        nvs_erase_all(handle);
        nvs_commit(handle);
        nvs_close(handle);
        ESP_LOGI(TAG, "Configuration reset");
    } else {
        ESP_LOGE(TAG, "Failed to open NVS for reset: %s", esp_err_to_name(err));
    }
}
