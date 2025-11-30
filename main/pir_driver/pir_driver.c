#include "pir_driver.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

static const char *TAG = "PIR_DRIVER";

static pir_callback_t pir_cb = NULL;
static QueueHandle_t pir_evt_queue = NULL;
static TaskHandle_t pir_evt_task_handle = NULL;
static bool last_state = false;

static void pir_evt_task(void *arg)
{
    uint32_t lvl;
    for (;;) {
        if (xQueueReceive(pir_evt_queue, &lvl, portMAX_DELAY) == pdTRUE) {
            // Debounce: esperar 50ms y leer nuevamente
            vTaskDelay(pdMS_TO_TICKS(50));
            bool raw_level = gpio_get_level(PIR_GPIO);
            // INVERTIR: si raw=1 → stable=0 (sin movimiento); si raw=0 → stable=1 (movimiento)
            bool stable = !raw_level;
            
            if (stable != (bool)(!lvl)) {
                continue;
            }
            if (stable != last_state) {
                last_state = stable;
                if (pir_cb) pir_cb(stable);
            }
        }
    }
}

// ISR: encolar el nivel crudo
static void IRAM_ATTR pir_isr_handler(void* arg)
{
    uint32_t level = gpio_get_level(PIR_GPIO);
    BaseType_t hpw = pdFALSE;
    if (pir_evt_queue) {
        xQueueSendFromISR(pir_evt_queue, &level, &hpw);
        if (hpw) portYIELD_FROM_ISR();
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

    if (pir_evt_queue == NULL) {
        pir_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    }
    if (pir_evt_task_handle == NULL && pir_evt_queue != NULL) {
        xTaskCreate(pir_evt_task, "pir_evt_task", 2048, NULL, configMAX_PRIORITIES - 1, &pir_evt_task_handle);
    }

    // Estabilización: esperar 2 segundos a que el sensor se calibre
    ESP_LOGI(TAG, "PIR warming up for 2 seconds...");
    vTaskDelay(pdMS_TO_TICKS(2000));

    // Leer estado inicial (invertido)
    bool raw_initial = gpio_get_level(PIR_GPIO);
    last_state = !raw_initial;  // INVERTIDO

    ESP_LOGI(TAG, "PIR initialized on GPIO %d, initial state=%d (inverted logic)", PIR_GPIO, last_state);
}

bool pir_read(void)
{
    // Retornar invertido: 0=sin movimiento, 1=movimiento detectado
    return !gpio_get_level(PIR_GPIO);
}

void pir_enable_interrupt(pir_callback_t cb)
{
    pir_cb = cb;

    gpio_set_intr_type(PIR_GPIO, GPIO_INTR_ANYEDGE);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(PIR_GPIO, pir_isr_handler, NULL);

    ESP_LOGI(TAG, "PIR interrupt enabled (inverted logic: 0=rest, 1=motion)");
}