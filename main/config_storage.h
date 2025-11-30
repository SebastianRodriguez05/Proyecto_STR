#ifndef CONFIG_STORAGE_H
#define CONFIG_STORAGE_H

#include <stdint.h>

#define NVS_NAMESPACE "fan_config"

typedef struct {
    uint8_t mode;           // 0: Manual, 1: Automático, 2: Programado
    uint8_t manual_pwm;     // 0-100% para modo manual
    uint8_t auto_temp_min;  // Temperatura mínima modo automático
    uint8_t auto_temp_max;  // Temperatura máxima modo automático
} fan_config_t;

void config_storage_init(void);
void config_storage_save_mode(uint8_t mode);
void config_storage_save_manual_pwm(uint8_t pwm);
uint8_t config_storage_get_manual_pwm(void);
uint8_t config_storage_get_mode(void);
void config_storage_reset(void);

#endif