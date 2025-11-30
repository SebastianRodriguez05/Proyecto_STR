#pragma once

#include "esp_adc/adc_oneshot.h"

// ============================
// Configuración del NTC
// ============================

// Unidad y canal ADC usados (ESP32-C6)
#define NTC_ADC_UNIT        ADC_UNIT_1
#define NTC_ADC_CHANNEL     ADC_CHANNEL_2   // GPIO2 = ADC1_CH2

// Parámetros eléctricos del módulo
#define NTC_VCC             3.3f           // Alimentación del módulo (3.3V)
#define NTC_R_FIXED         10000.0f       // 10k de la resistencia fija del módulo

// Parámetros del NTC 10k típico
#define NTC_BETA            3950.0f        // Beta típico del NTC 10k
#define NTC_R0              10000.0f       // 10k @ 25°C
#define NTC_T0              298.15f        // 25°C en Kelvin (25 + 273.15)

// ⚠️ Offset de calibración (en °C):
// Si en ambiente (~16°C) te mide ~40°C, el error es aprox +24°C.
// Aplicamos un offset de -24°C para corregir en el rango de interés.
#define NTC_TEMP_OFFSET_C   (-24.0f)

// API del driver
void  ntc_init(void);
float ntc_read_celsius(void);
float ntc_read_fahrenheit(void);
