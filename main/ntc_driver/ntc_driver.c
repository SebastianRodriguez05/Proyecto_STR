#include "ntc_driver.h"
#include "esp_log.h"
#include <math.h>

static const char *TAG = "NTC_DRIVER";

static adc_oneshot_unit_handle_t adc_handle = NULL;

void ntc_init(void)
{
    // ============================
    // 1) Inicializar unidad ADC
    // ============================
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = NTC_ADC_UNIT,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle));

    // ============================
    // 2) Configurar canal
    // ============================
    adc_oneshot_chan_cfg_t chan_config = {
        .bitwidth = ADC_BITWIDTH_12,   // 0–4095
        .atten    = ADC_ATTEN_DB_11    // permite ~0–3.3V
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle,
                                               NTC_ADC_CHANNEL,
                                               &chan_config));

    ESP_LOGI(TAG, "NTC initialized on ADC1 channel %d", NTC_ADC_CHANNEL);
}

float ntc_read_celsius(void)
{
    int raw = 0;
    ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, NTC_ADC_CHANNEL, &raw));

    // ============================
    // 1) Convertir a voltaje (aprox.)
    // ============================
    float vout = ((float)raw / 4095.0f) * NTC_VCC;  // en Voltios

    // Proteger de divisiones raras
    if (vout <= 0.01f || vout >= (NTC_VCC - 0.01f)) {
        ESP_LOGW(TAG, "Voltaje fuera de rango: raw=%d -> V=%.3f", raw, vout);
        return NAN;
    }

    // ============================
    // 2) Calcular R_NTC según el MÓDULO:
    //
    //    3.3V --[ R_FIXED ]--+--[ NTC ]-- GND
    //                        |
    //                       Vout
    //
    //    => Vout = Vcc * (R_NTC / (R_FIXED + R_NTC))
    //    => R_NTC = (Vout * R_FIXED) / (Vcc - Vout)
    // ============================
    float r_ntc = (vout * NTC_R_FIXED) / (NTC_VCC - vout);

    // ============================
    // 3) Ecuación Beta
    // ============================
    float ln_ratio = logf(r_ntc / NTC_R0);
    float inv_T    = (1.0f / NTC_T0) + (1.0f / NTC_BETA) * ln_ratio;
    float temp_K   = 1.0f / inv_T;
    float temp_C   = temp_K - 273.15f;

    // ============================
    // 4) Aplicar offset de calibración
    // ============================
    temp_C += NTC_TEMP_OFFSET_C;

    // (Opcional) Para debug:
    // ESP_LOGI(TAG, "raw=%d, V=%.3f, Rntc=%.1f -> T=%.2f°C (corr.)",
    //          raw, vout, r_ntc, temp_C);

    return temp_C;
}

float ntc_read_fahrenheit(void)
{
    float c = ntc_read_celsius();
    if (isnan(c)) {
        return NAN;
    }
    return (c * 9.0f / 5.0f) + 32.0f;
}
