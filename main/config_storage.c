#include "config_storage.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"

static const char *TAG = "CONFIG_STORAGE";

/**
 * NOTA:
 * Se asume que NVS_NAMESPACE está definido en config_storage.h
 *   #define NVS_NAMESPACE "fan_config"
 */

// Valores por defecto para modo automático
#define AUTO_TMIN_DEFAULT   24   // °C
#define AUTO_TMAX_DEFAULT   28   // °C

// === Array global de registros programados en RAM ===
static program_slot_t g_prog_slots[PROGRAM_SLOTS];
static bool g_prog_slots_loaded = false;


// ----------------------------------------
//  PROTOTIPO INTERNO
// ----------------------------------------
static void program_slot_set_default(program_slot_t *slot);
static void ensure_slots_loaded(void);


/* ============================================================
 *  INICIALIZACIÓN
 * ============================================================ */

void config_storage_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGI(TAG, "NVS partition truncated, erasing...");
        nvs_flash_erase();
        nvs_flash_init();
    }
    ESP_LOGI(TAG, "NVS initialized");

    // Cargar los registros programados en el array global
    config_storage_load_program_slots(g_prog_slots, PROGRAM_SLOTS);
    g_prog_slots_loaded = true;
}

/* ============================================================
 *  MODO MANUAL: PWM guardado
 * ============================================================ */

void config_storage_save_manual_pwm(uint8_t pwm)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        nvs_set_u8(handle, "manual_pwm", pwm);
        nvs_commit(handle);
        nvs_close(handle);
        ESP_LOGI(TAG, "Manual PWM saved: %d%%", pwm);
    } else {
        ESP_LOGE(TAG, "Failed to open NVS for manual_pwm save: %s", esp_err_to_name(err));
    }
}

uint8_t config_storage_get_manual_pwm(void)
{
    nvs_handle_t handle;
    uint8_t pwm = 0;   // default 0%
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_OK) {
        esp_err_t err_get = nvs_get_u8(handle, "manual_pwm", &pwm);
        if (err_get != ESP_OK && err_get != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGE(TAG, "Error reading manual_pwm: %s", esp_err_to_name(err_get));
        }
        nvs_close(handle);
    } else {
        ESP_LOGE(TAG, "Failed to open NVS for manual_pwm get: %s", esp_err_to_name(err));
    }
    return pwm;
}

/* ============================================================
 *  MODO GENERAL: modo de operación (0=Manual, 1=Auto, 2=Prog)
 * ============================================================ */

void config_storage_save_mode(uint8_t mode)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        nvs_set_u8(handle, "mode", mode);
        nvs_commit(handle);
        nvs_close(handle);
        ESP_LOGI(TAG, "Mode saved: %d", mode);
    } else {
        ESP_LOGE(TAG, "Failed to open NVS for mode save: %s", esp_err_to_name(err));
    }
}

uint8_t config_storage_get_mode(void)
{
    nvs_handle_t handle;
    uint8_t mode = 0;  // Default: Manual
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_OK) {
        esp_err_t err_get = nvs_get_u8(handle, "mode", &mode);
        if (err_get == ESP_ERR_NVS_NOT_FOUND) {
            // Si no existe, dejamos 0 (manual) sin log de error fuerte
            ESP_LOGI(TAG, "Mode key not found, using default (0=manual)");
        } else if (err_get != ESP_OK) {
            ESP_LOGE(TAG, "Error reading mode: %s", esp_err_to_name(err_get));
        }
        nvs_close(handle);
    } else {
        ESP_LOGE(TAG, "Failed to open NVS for mode get: %s", esp_err_to_name(err));
    }
    return mode;
}

/* ============================================================
 *  MODO AUTOMÁTICO: T_min y T_max
 * ============================================================ */

void config_storage_save_auto_tmin(int tmin)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        nvs_set_i32(handle, "auto_tmin", tmin);
        nvs_commit(handle);
        nvs_close(handle);
        ESP_LOGI(TAG, "Auto T_min saved: %d C", tmin);
    } else {
        ESP_LOGE(TAG, "Failed to open NVS for auto_tmin save: %s", esp_err_to_name(err));
    }
}

void config_storage_save_auto_tmax(int tmax)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        nvs_set_i32(handle, "auto_tmax", tmax);
        nvs_commit(handle);
        nvs_close(handle);
        ESP_LOGI(TAG, "Auto T_max saved: %d C", tmax);
    } else {
        ESP_LOGE(TAG, "Failed to open NVS for auto_tmax save: %s", esp_err_to_name(err));
    }
}

int config_storage_get_auto_tmin(void)
{
    nvs_handle_t handle;
    int32_t tmin = AUTO_TMIN_DEFAULT;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_OK) {
        esp_err_t err_get = nvs_get_i32(handle, "auto_tmin", &tmin);
        if (err_get == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGI(TAG, "auto_tmin not found, using default %d C", AUTO_TMIN_DEFAULT);
        } else if (err_get != ESP_OK) {
            ESP_LOGE(TAG, "Error reading auto_tmin: %s", esp_err_to_name(err_get));
        }
        nvs_close(handle);
    } else {
        ESP_LOGE(TAG, "Failed to open NVS for auto_tmin get: %s", esp_err_to_name(err));
    }
    return (int)tmin;
}

int config_storage_get_auto_tmax(void)
{
    nvs_handle_t handle;
    int32_t tmax = AUTO_TMAX_DEFAULT;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_OK) {
        esp_err_t err_get = nvs_get_i32(handle, "auto_tmax", &tmax);
        if (err_get == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGI(TAG, "auto_tmax not found, using default %d C", AUTO_TMAX_DEFAULT);
        } else if (err_get != ESP_OK) {
            ESP_LOGE(TAG, "Error reading auto_tmax: %s", esp_err_to_name(err_get));
        }
        nvs_close(handle);
    } else {
        ESP_LOGE(TAG, "Failed to open NVS for auto_tmax get: %s", esp_err_to_name(err));
    }
    return (int)tmax;
}

/* ============================================================
 *  MODO PROGRAMADO: registros programados (PROGRAM_SLOTS)
 * ============================================================ */

static void program_slot_set_default(program_slot_t *slot)
{
    if (!slot) return;

    slot->active  = 0;
    slot->h_start = 0;
    slot->m_start = 0;
    slot->h_end   = 0;
    slot->m_end   = 0;
    slot->t0      = 24;
    slot->t100    = 28;
}

void config_storage_load_program_slots(program_slot_t *slots, int n_slots)
{
    if (!slots || n_slots <= 0) {
        return;
    }

    // Inicializar con valores por defecto
    for (int i = 0; i < n_slots; i++) {
        program_slot_set_default(&slots[i]);
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "load_program_slots: NVS open failed (%s), using defaults",
                 esp_err_to_name(err));
        return;
    }

    size_t required_size = 0;
    err = nvs_get_blob(handle, "prog_slots", NULL, &required_size);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "load_program_slots: no prog_slots found, using defaults");
        nvs_close(handle);
        return;
    } else if (err != ESP_OK) {
        ESP_LOGE(TAG, "load_program_slots: nvs_get_blob size error: %s",
                 esp_err_to_name(err));
        nvs_close(handle);
        return;
    }

    size_t max_size = n_slots * sizeof(program_slot_t);
    if (required_size > max_size) {
        ESP_LOGW(TAG,
                 "load_program_slots: blob size (%d) > local buffer (%d), truncating",
                 (int)required_size, (int)max_size);
        required_size = max_size;
    }

    err = nvs_get_blob(handle, "prog_slots", slots, &required_size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "load_program_slots: nvs_get_blob data error: %s",
                 esp_err_to_name(err));
        // Dejamos los valores por defecto
    } else {
        ESP_LOGI(TAG, "load_program_slots: loaded %d bytes of program slots",
                 (int)required_size);
    }

    nvs_close(handle);
}

void config_storage_save_program_slots(const program_slot_t *slots, int n_slots)
{
    if (!slots || n_slots <= 0) {
        return;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "save_program_slots: NVS open failed: %s", esp_err_to_name(err));
        return;
    }

    size_t size = n_slots * sizeof(program_slot_t);
    err = nvs_set_blob(handle, "prog_slots", slots, size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "save_program_slots: nvs_set_blob failed: %s", esp_err_to_name(err));
        nvs_close(handle);
        return;
    }

    err = nvs_commit(handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "save_program_slots: commit failed: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "save_program_slots: saved %d slots (%d bytes)",
                 n_slots, (int)size);
    }

    nvs_close(handle);
}

// --- Helpers internos para asegurar que el array global está cargado ---
static void ensure_slots_loaded(void)
{
    if (!g_prog_slots_loaded) {
        config_storage_load_program_slots(g_prog_slots, PROGRAM_SLOTS);
        g_prog_slots_loaded = true;
    }
}

// Lee un slot concreto (1..PROGRAM_SLOTS)
void program_get_slot(int id, program_slot_t *out)
{
    if (!out) return;

    ensure_slots_loaded();

    if (id < 1 || id > PROGRAM_SLOTS) {
        ESP_LOGW(TAG, "program_get_slot: id fuera de rango: %d", id);
        program_slot_set_default(out);
        return;
    }

    *out = g_prog_slots[id - 1];
}

// Escribe un slot concreto (1..PROGRAM_SLOTS) y guarda todos en NVS
void program_set_slot(int id, const program_slot_t *in)
{
    if (!in) return;

    ensure_slots_loaded();

    if (id < 1 || id > PROGRAM_SLOTS) {
        ESP_LOGW(TAG, "program_set_slot: id fuera de rango: %d", id);
        return;
    }

    g_prog_slots[id - 1] = *in;
    config_storage_save_program_slots(g_prog_slots, PROGRAM_SLOTS);
}

// Borra un slot (lo pone por defecto, active=0)
void program_erase_slot(int id)
{
    ensure_slots_loaded();

    if (id < 1 || id > PROGRAM_SLOTS) {
        ESP_LOGW(TAG, "program_erase_slot: id fuera de rango: %d", id);
        return;
    }

    program_slot_set_default(&g_prog_slots[id - 1]);
    config_storage_save_program_slots(g_prog_slots, PROGRAM_SLOTS);
}

/* ============================================================
 *  RESET COMPLETO
 * ============================================================ */

void config_storage_reset(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        nvs_erase_all(handle);
        nvs_commit(handle);
        nvs_close(handle);
        ESP_LOGI(TAG, "Configuration reset");
    } else {
        ESP_LOGE(TAG, "Failed to open NVS for reset: %s", esp_err_to_name(err));
    }
}
