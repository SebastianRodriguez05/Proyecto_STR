#include "fan_driver.h"
#include "driver/ledc.h"
#include "esp_log.h"

static const char *TAG = "FAN_DRIVER";
static uint8_t current_pwm_percent = 0;

#define FAN_PWM_GPIO   GPIO_NUM_9      // Cambia según tu pin
// OJO: si ya tienes FAN_PWM_FREQ y FAN_PWM_RES en fan_driver.h, úsalos.
// Aquí asumo que existen esos defines.
#define LEDC_MODE      LEDC_LOW_SPEED_MODE
#define LEDC_TIMER     LEDC_TIMER_0
#define LEDC_CHANNEL   LEDC_CHANNEL_0

void fan_init(void)
{
    // Configurar timer LEDC
    ledc_timer_config_t timer = {
        .speed_mode       = LEDC_MODE,
        .timer_num        = LEDC_TIMER,
        .freq_hz          = FAN_PWM_FREQ,   // p.ej. 25000 Hz
        .duty_resolution  = FAN_PWM_RES,    // p.ej. LEDC_TIMER_10_BIT
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer));

    // Canal PWM
    ledc_channel_config_t channel = {
        .gpio_num   = FAN_PWM_GPIO,
        .speed_mode = LEDC_MODE,
        .channel    = LEDC_CHANNEL,
        .timer_sel  = LEDC_TIMER,
        .duty       = 0,
        .hpoint     = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&channel));

    ESP_LOGI(TAG, "Fan initialized on GPIO %d", FAN_PWM_GPIO);
}

void fan_set_percent(uint8_t percent)
{
    if (percent > 100) percent = 100;

    current_pwm_percent = percent;

    // Calcular el máximo duty válido
    uint32_t duty_max = (1 << FAN_PWM_RES) - 1;   // Si RES=10 → 1023

    // Convertir porcentaje a duty
    uint32_t duty = (percent * duty_max) / 100;

    ESP_LOGI(TAG, "Fan PWM -> %d%% (duty=%lu)", percent, duty);

    ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));
}

// ✅ ESTA función va FUERA de fan_set_percent
uint8_t fan_get_percent(void)
{
    return current_pwm_percent;
}
