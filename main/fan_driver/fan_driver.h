#pragma once

#include "driver/ledc.h"
#include <stdint.h>

#define FAN_PWM_GPIO    GPIO_NUM_9
#define FAN_PWM_FREQ    25000                 // 25 kHz → silencioso
#define FAN_PWM_RES     LEDC_TIMER_10_BIT     // 10 bits → 0–1023

// Inicializa el driver
void fan_init(void);

// Cambia el PWM (0–100%)
void fan_set_percent(uint8_t percent);

// Obtiene el PWM actual (0–100%)
uint8_t fan_get_percent(void);

// ---- Compatibilidad con nombres antiguos ----

// Wrapper: hace lo mismo que fan_set_percent()
void fan_driver_set_pwm(uint8_t pwm);

// Pone el ventilador en 0%
void fan_driver_stop(void);
