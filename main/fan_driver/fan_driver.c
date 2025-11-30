#include "fan_driver.h"
#include "driver/ledc.h"
#include "esp_log.h"

static const char *TAG = "FAN_DRIVER";
static uint8_t current_pwm_percent = 0;

#define FAN_PWM_GPIO        GPIO_NUM_9    // Cambia según tu pin
#define LEDC_TIMER          LEDC_TIMER_0
#define LEDC_MODE           LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL        LEDC_CHANNEL_0
#define LEDC_DUTY_RES       LEDC_TIMER_10_BIT
#define LEDC_FREQUENCY      25000           // 1kHz

void fan_init(void)
{
    // Configurar timer LEDC
    ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = FAN_PWM_FREQ,
        .duty_resolution = FAN_PWM_RES,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer));

    // Canal PWM
    ledc_channel_config_t channel = {
        .gpio_num = FAN_PWM_GPIO,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0
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

    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0));
}

uint8_t fan_get_percent(void)
{
    return current_pwm_percent;
}
