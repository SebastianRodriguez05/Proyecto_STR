#ifndef CONFIG_STORAGE_H
#define CONFIG_STORAGE_H

#include <stdint.h>

#define NVS_NAMESPACE "fan_config"

// Configuración completa (por si más adelante quieres leer todo de una vez)
typedef struct {
    uint8_t mode;        // 0: Manual, 1: Automático, 2: Programado
    uint8_t manual_pwm;  // 0-100% para modo manual
    int32_t auto_tmin;   // Temperatura mínima modo automático (°C)
    int32_t auto_tmax;   // Temperatura máxima modo automático (°C)
} fan_config_t;

/* ===========================
 *  INICIALIZACIÓN / RESET
 * =========================== */
void config_storage_init(void);
void config_storage_reset(void);

/* ===========================
 *  MODO GENERAL
 * =========================== */
void    config_storage_save_mode(uint8_t mode);
uint8_t config_storage_get_mode(void);

/* ===========================
 *  MODO MANUAL
 * =========================== */
void    config_storage_save_manual_pwm(uint8_t pwm);
uint8_t config_storage_get_manual_pwm(void);

/* ===========================
 *  MODO AUTOMÁTICO (T_min/T_max)
 * =========================== */
void config_storage_save_auto_tmin(int tmin);
void config_storage_save_auto_tmax(int tmax);
int  config_storage_get_auto_tmin(void);
int  config_storage_get_auto_tmax(void);

#endif
