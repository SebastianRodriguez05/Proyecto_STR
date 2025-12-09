#pragma once

#include "driver/gpio.h"
#include <stdbool.h>

// Pin donde conectas la salida DIGITAL (DO) del módulo infrarrojo
// Cámbialo si usas otro pin.
#define PIR_GPIO    GPIO_NUM_10

// Callback opcional cuando cambia el estado (no lo estás usando ahora,
// pero dejamos la API igual por si lo quieres más adelante).
typedef void (*pir_callback_t)(bool state);

/**
 * @brief Inicializa el sensor infrarrojo (modo entrada digital).
 */
void pir_init(void);

/**
 * @brief Lee el estado del sensor.
 *
 * @return true  si hay obstáculo/presencia (DO = 0 en el módulo → invertimos)
 * @return false si NO hay obstáculo/presencia
 */
bool pir_read(void);

/**
 * @brief Habilita interrupción por cambio y llama a un callback opcional.
 *
 * No la estás usando en tu código actual, pero mantenemos la firma para
 * no romper nada si alguna parte la llama más adelante.
 */
void pir_enable_interrupt(pir_callback_t cb);
