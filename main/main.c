#include "nvs_flash.h"
#include "wifi_app.h"
#include "config_storage.h"
#include "fan_driver.h"
#include "ntc_driver.h"
#include "pir_driver.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define BLINK_GPIO  2

static const char *TAG = "MAIN";

static void configure_led(void)
{
    gpio_reset_pin(BLINK_GPIO);
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
}

void app_main(void)
{
    // NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Configuración en flash
    config_storage_init();

    // Drivers de hardware
    fan_init();     // PWM ventilador
    ntc_init();     // NTC temperatura
    pir_init();     // PIR presencia

    // Leer modo y PWM manual guardado
    uint8_t mode       = config_storage_get_mode();
    uint8_t manual_pwm = config_storage_get_manual_pwm();

    ESP_LOGI(TAG, "Modo guardado: %d, PWM guardado: %d %%", mode, manual_pwm);

    if (mode == 0) {
        fan_set_percent(manual_pwm);
    } else {
        fan_set_percent(0);
    }

    // Hora por red
    init_obtain_time();

    // LED de prueba
    configure_led();

    // WiFi + servidor HTTP
    wifi_app_start();
}
