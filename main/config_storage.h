#ifndef CONFIG_STORAGE_H
#define CONFIG_STORAGE_H

#include <stdint.h>
#include <stdbool.h>

#define NVS_NAMESPACE "fan_config"

// ===========================
//  CONFIG GENERAL
// ===========================
typedef struct {
    uint8_t mode;        // 0: Manual, 1: Automático, 2: Programado
    uint8_t manual_pwm;  // 0-100% para modo manual
    int32_t auto_tmin;   // Temperatura mínima modo automático (°C)
    int32_t auto_tmax;   // Temperatura máxima modo automático (°C)
} fan_config_t;


// ===========================
//  REGISTROS PROGRAMADOS (MODO 2)
// ===========================

#define PROGRAM_SLOTS 3

typedef struct {
    uint8_t active;      // 0 = inactivo, 1 = activo
    uint8_t h_start;     // hora inicio   [0..23]
    uint8_t m_start;     // minuto inicio [0..59]
    uint8_t h_end;       // hora fin      [0..23]
    uint8_t m_end;       // minuto fin    [0..59]
    int16_t t0;          // temperatura para 0% PWM (°C)
    int16_t t100;        // temperatura para 100% PWM (°C)
} program_slot_t;


/* ===========================
 *  INICIALIZACIÓN / RESET
 * =========================== */
void config_storage_init(void);
void config_storage_reset(void);

/* ===========================
 *  MODO GENERAL (mode)
 * =========================== */
void    config_storage_save_mode(uint8_t mode);
uint8_t config_storage_get_mode(void);

/* ===========================
 *  MODO MANUAL (PWM)
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

/* ===========================
 *  MODO PROGRAMADO (REGISTROS)
 * =========================== */

// Carga n_slots registros desde NVS en el array "slots".
// Si no hay nada guardado, rellena con valores por defecto.
void config_storage_load_program_slots(program_slot_t *slots, int n_slots);

// Guarda n_slots registros desde el array "slots" hacia NVS.
void config_storage_save_program_slots(const program_slot_t *slots, int n_slots);

// === Helpers de alto nivel (trabajan sobre un array global interno) ===

// Lee el registro [1..PROGRAM_SLOTS] en *out.
// Si el id es inválido, devuelve un slot por defecto (active=0, etc.).
void program_get_slot(int id, program_slot_t *out);

// Escribe el registro [1..PROGRAM_SLOTS] y guarda todos en NVS.
void program_set_slot(int id, const program_slot_t *in);

// Borra el registro (lo pone en valores por defecto, active=0).
void program_erase_slot(int id);

#endif
