#include "nvs_flash.h"
#include "wifi_app.h"
#include "config_storage.h"
#include "fan_driver.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define BLINK_GPIO 2

static void configure_led(void)
{
    gpio_reset_pin(BLINK_GPIO);
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
}

void app_main(void)
{
    // Inicializar NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Inicializar almacenamiento (PWM, modo)
    config_storage_init();

    // Inicializar ventilador (LEDC)
    fan_init();

    // Leer modo y PWM guardado
    uint8_t mode = config_storage_get_mode();
    uint8_t manual_pwm = config_storage_get_manual_pwm();

    ESP_LOGI("MAIN", "Modo guardado: %d, PWM guardado: %d %%", mode, manual_pwm);

    // Aplicar PWM si estamos en modo manual
    if (mode == 0)
    {
        fan_set_percent(manual_pwm);
    }
    else
    {
        fan_set_percent(0);
    }

    // Inicializar reloj
    init_obtain_time();

    // LED indicador
    configure_led();

    // Iniciar WiFi (esto llama al HTTP server internamente)
    wifi_app_start();
}
