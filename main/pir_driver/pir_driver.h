#pragma once

#include "driver/gpio.h"
#include <stdbool.h>

#define PIR_GPIO    GPIO_NUM_10   // <-- cámbialo si quieres otro pin

// Inicializa el PIR
void pir_init(void);

// Devuelve true si hay movimiento
bool pir_read(void);

// Callback opcional cuando cambia el PIR
typedef void (*pir_callback_t)(bool state);

// Habilita interrupción por cambio
void pir_enable_interrupt(pir_callback_t cb);
