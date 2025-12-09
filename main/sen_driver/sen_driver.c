#include "sen_driver.h"
#include "esp_log.h"
#include "driver/gpio.h"

static const char *TAG = "IR_DRIVER";

// Callback opcional (si algún día quieres usar interrupciones)
static pir_callback_t pir_cb = NULL;

static void IRAM_ATTR pir_isr_handler(void *arg)
{
    // DO del módulo: 0 = obstáculo, 1 = libre
    int level = gpio_get_level(PIR_GPIO);
    bool presence = (level == 0);   // true = obstáculo/presencia

    if (pir_cb) {
        pir_cb(presence);
    }
}

void pir_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIR_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    ESP_ERROR_CHECK(gpio_config(&io_conf));

    ESP_LOGI(TAG,
             "IR sensor (digital) initialized on GPIO %d (0=obstáculo, 1=libre)",
             PIR_GPIO);
}

bool pir_read(void)
{
    // En el módulo IR: DO = 0 cuando detecta obstáculo.
    // Queremos que la función devuelva true cuando HAY presencia/obstáculo.
    int level = gpio_get_level(PIR_GPIO);
    return (level == 0);   // 1 = obstáculo/presencia, 0 = libre
}

void pir_enable_interrupt(pir_callback_t cb)
{
    pir_cb = cb;

    // Si no hay callback, deshabilitamos las interrupciones
    if (cb == NULL) {
        gpio_set_intr_type(PIR_GPIO, GPIO_INTR_DISABLE);
        gpio_isr_handler_remove(PIR_GPIO);
        ESP_LOGI(TAG, "IR interrupt disabled");
        return;
    }

    // Configurar interrupción por cualquier cambio de nivel
    gpio_set_intr_type(PIR_GPIO, GPIO_INTR_ANYEDGE);

    // Instalar servicio de ISR (si no estaba)
    esp_err_t err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Error installing ISR service: 0x%x", err);
        return;
    }

    gpio_isr_handler_add(PIR_GPIO, pir_isr_handler, NULL);
    ESP_LOGI(TAG, "IR interrupt enabled (callback activo)");
}
