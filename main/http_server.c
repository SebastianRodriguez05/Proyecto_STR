/*
 * http_server.c
 *
 *  Created on: Oct 20, 2021
 *      Author: kjagu (modificado para modo manual / automático / programado)
 */

#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_timer.h"
#include "sys/param.h"
#include <stdlib.h>

#include "http_server.h"
#include "tasks_common.h"
#include "wifi_app.h"
#include "cJSON.h"
#include "driver/gpio.h"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <time.h>      // <-- para hora actual (modo programado)

#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "mqtt_client.h"
#include "esp_sntp.h"

#include "config_storage.h"
#include "fan_driver.h"
#include "ntc_driver.h"
#include "pir_driver.h"

// Tag used for ESP serial console messages
static const char TAG[] = "http_server";

/*======================================================================
 *  PROTOTIPOS ESTÁTICOS
 *====================================================================*/
static void http_server_monitor(void *parameter);
static httpd_handle_t http_server_configure(void);
static void http_server_fw_update_reset_timer(void);
void http_server_fw_update_reset_callback(void *arg);

static esp_err_t http_server_get_dht_sensor_readings_json_handler(httpd_req_t *req);
static esp_err_t http_server_toogle_led_handler(httpd_req_t *req);
static esp_err_t http_server_update_temperature_range_handler(httpd_req_t *req);

// Resumen de registros programados
static esp_err_t http_server_read_regs_summary_handler(httpd_req_t *req);

// WIFI
static esp_err_t http_server_wifi_connect_json_handler(httpd_req_t *req);
static esp_err_t http_server_wifi_connect_status_json_handler(httpd_req_t *req);

// Archivos estáticos
static esp_err_t http_server_jquery_handler(httpd_req_t *req);
static esp_err_t http_server_index_html_handler(httpd_req_t *req);
static esp_err_t http_server_app_css_handler(httpd_req_t *req);
static esp_err_t http_server_app_js_handler(httpd_req_t *req);
static esp_err_t http_server_favicon_ico_handler(httpd_req_t *req);

// Modo manual / automático
static esp_err_t http_server_get_manual_config_handler(httpd_req_t *req);
static esp_err_t http_server_get_auto_config_handler(httpd_req_t *req);
static esp_err_t http_server_set_auto_config_handler(httpd_req_t *req);
static esp_err_t http_server_manual_pwm_handler(httpd_req_t *req);
static esp_err_t http_server_status_json_handler(httpd_req_t *req);

// Modo programado (usando program_* de config_storage.h)
static esp_err_t http_server_program_get_handler(httpd_req_t *req);
static esp_err_t http_server_program_set_handler(httpd_req_t *req);
static esp_err_t http_server_program_erase_handler(httpd_req_t *req);

// Tarea de control del ventilador
static void fan_control_task(void *parameter);

/**
 * ESP32 timer configuration passed to esp_timer_create.
 */
const esp_timer_create_args_t fw_update_reset_args = {
    .callback = &http_server_fw_update_reset_callback,
    .arg = NULL,
    .dispatch_method = ESP_TIMER_TASK,
    .name = "fw_update_reset"
};
esp_timer_handle_t fw_update_reset;

// Wifi connect status
static int g_wifi_connect_status = NONE;

// Firmware update status
static int g_fw_update_status = OTA_UPDATE_PENDING;

// HTTP server task handle
static httpd_handle_t http_server_handle = NULL;

// HTTP server monitor task handle
static TaskHandle_t task_http_server_monitor = NULL;

// Queue handle used to manipulate the main queue of events
static QueueHandle_t http_server_monitor_queue_handle;

// Tarea de control del ventilador
static TaskHandle_t fan_control_task_handle = NULL;

// Embedded files: JQuery, index.html, app.css, app.js and favicon.ico files
extern const uint8_t jquery_3_3_1_min_js_start[] asm("_binary_jquery_3_3_1_min_js_start");
extern const uint8_t jquery_3_3_1_min_js_end[]   asm("_binary_jquery_3_3_1_min_js_end");
extern const uint8_t index_html_start[]          asm("_binary_index_html_start");
extern const uint8_t index_html_end[]            asm("_binary_index_html_end");
extern const uint8_t app_css_start[]             asm("_binary_app_css_start");
extern const uint8_t app_css_end[]               asm("_binary_app_css_end");
extern const uint8_t app_js_start[]              asm("_binary_app_js_start");
extern const uint8_t app_js_end[]                asm("_binary_app_js_end");
extern const uint8_t favicon_ico_start[]         asm("_binary_favicon_ico_start");
extern const uint8_t favicon_ico_end[]           asm("_binary_favicon_ico_end");

uint8_t s_led_state = 0;

/*======================================================================
 *  UTILIDADES
 *====================================================================*/

void toogle_led(void)
{
    s_led_state = !s_led_state;
    gpio_set_level(BLINK_GPIO, s_led_state);
}

static esp_err_t http_server_get_dht_sensor_readings_json_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "/dhtSensor.json requested");

    char json_response[64];

    // Temperatura REAL del NTC
    float temp_c = ntc_read_celsius();

    int len = snprintf(json_response, sizeof(json_response),
                       "{\"temp\":%.2f}", temp_c);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_response, len);

    return ESP_OK;
}

static esp_err_t http_server_toogle_led_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "/toogle_led.json requested");

    toogle_led();

    httpd_resp_set_hdr(req, "Connection", "close");
    httpd_resp_send(req, NULL, 0);

    return ESP_OK;
}

static esp_err_t http_server_update_temperature_range_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "/update_temp_range.json requested");
    // Handler vacío (legacy). Puedes borrarlo si no lo usas.
    return ESP_OK;
}

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

/*======================================================================
 *  MQTT
 *====================================================================*/

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        msg_id = esp_mqtt_client_subscribe(client, "test", 1);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        if (strncmp(event->data, "toggle", event->data_len) == 0) {
            printf("toogle LED received");
            toogle_led();
        }
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno", event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

static void mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = "mqtt://victor:banano@ec2-35-93-50-123.us-west-2.compute.amazonaws.com",
        .session.keepalive = 15,
        .network.reconnect_timeout_ms = 50,
        .task.priority = 5,
        .task.stack_size = 4096,
    };

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}

/*======================================================================
 *  OTA
 *====================================================================*/

static void http_server_fw_update_reset_timer(void)
{
    if (g_fw_update_status == OTA_UPDATE_SUCCESSFUL)
    {
        ESP_LOGI(TAG, "http_server_fw_update_reset_timer: FW updated successful starting FW update reset timer");

        ESP_ERROR_CHECK(esp_timer_create(&fw_update_reset_args, &fw_update_reset));
        ESP_ERROR_CHECK(esp_timer_start_once(fw_update_reset, 8000000));
    }
    else
    {
        ESP_LOGI(TAG, "http_server_fw_update_reset_timer: FW update unsuccessful");
    }
}

static void http_server_monitor(void *parameter)
{
    http_server_queue_message_t msg;

    for (;;)
    {
        if (xQueueReceive(http_server_monitor_queue_handle, &msg, portMAX_DELAY))
        {
            switch (msg.msgID)
            {
            case HTTP_MSG_WIFI_CONNECT_INIT:
                ESP_LOGI(TAG, "HTTP_MSG_WIFI_CONNECT_INIT");
                g_wifi_connect_status = HTTP_WIFI_STATUS_CONNECTING;
                break;

            case HTTP_MSG_WIFI_CONNECT_SUCCESS:
                ESP_LOGI(TAG, "HTTP_MSG_WIFI_CONNECT_SUCCESS");
                g_wifi_connect_status = HTTP_WIFI_STATUS_CONNECT_SUCCESS;
                //mqtt_app_start();
                break;

            case HTTP_MSG_WIFI_CONNECT_FAIL:
                ESP_LOGI(TAG, "HTTP_MSG_WIFI_CONNECT_FAIL");
                g_wifi_connect_status = HTTP_WIFI_STATUS_CONNECT_FAILED;
                break;

            case HTTP_MSG_OTA_UPDATE_SUCCESSFUL:
                ESP_LOGI(TAG, "HTTP_MSG_OTA_UPDATE_SUCCESSFUL");
                g_fw_update_status = OTA_UPDATE_SUCCESSFUL;
                http_server_fw_update_reset_timer();
                break;

            case HTTP_MSG_OTA_UPDATE_FAILED:
                ESP_LOGI(TAG, "HTTP_MSG_OTA_UPDATE_FAILED");
                g_fw_update_status = OTA_UPDATE_FAILED;
                break;

            default:
                break;
            }
        }
    }
}

/*======================================================================
 *  HANDLERS DE ARCHIVOS ESTÁTICOS
 *====================================================================*/

static esp_err_t http_server_jquery_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Jquery requested");

    httpd_resp_set_type(req, "application/javascript");
    httpd_resp_send(req, (const char *)jquery_3_3_1_min_js_start,
                    jquery_3_3_1_min_js_end - jquery_3_3_1_min_js_start);
    return ESP_OK;
}

static esp_err_t http_server_index_html_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "index.html requested");

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, (const char *)index_html_start,
                    index_html_end - index_html_start);
    return ESP_OK;
}

static esp_err_t http_server_app_css_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "app.css requested");

    httpd_resp_set_type(req, "text/css");
    httpd_resp_send(req, (const char *)app_css_start,
                    app_css_end - app_css_start);
    return ESP_OK;
}

static esp_err_t http_server_app_js_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "app.js requested");

    httpd_resp_set_type(req, "application/javascript");
    httpd_resp_send(req, (const char *)app_js_start,
                    app_js_end - app_js_start);
    return ESP_OK;
}

static esp_err_t http_server_favicon_ico_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "favicon.ico requested");

    httpd_resp_set_type(req, "image/x-icon");
    httpd_resp_send(req, (const char *)favicon_ico_start,
                    favicon_ico_end - favicon_ico_start);
    return ESP_OK;
}

/*======================================================================
 *  OTA: RECEPCIÓN Y ESTADO
 *====================================================================*/

esp_err_t http_server_OTA_update_handler(httpd_req_t *req)
{
    esp_ota_handle_t ota_handle;

    char ota_buff[1024];
    int content_length = req->content_len;
    int content_received = 0;
    int recv_len;
    bool is_req_body_started = false;
    bool flash_successful = false;

    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);

    do
    {
        if ((recv_len = httpd_req_recv(req, ota_buff, MIN(content_length, sizeof(ota_buff)))) < 0)
        {
            if (recv_len == HTTPD_SOCK_ERR_TIMEOUT)
            {
                ESP_LOGI(TAG, "http_server_OTA_update_handler: Socket Timeout");
                continue;
            }
            ESP_LOGI(TAG, "http_server_OTA_update_handler: OTA other Error %d", recv_len);
            return ESP_FAIL;
        }
        printf("http_server_OTA_update_handler: OTA RX: %d of %d\r", content_received, content_length);

        if (!is_req_body_started)
        {
            is_req_body_started = true;

            char *body_start_p = strstr(ota_buff, "\r\n\r\n") + 4;
            int body_part_len = recv_len - (body_start_p - ota_buff);

            printf("http_server_OTA_update_handler: OTA file size: %d\r\n", content_length);

            esp_err_t err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle);
            if (err != ESP_OK)
            {
                printf("http_server_OTA_update_handler: Error with OTA begin, cancelling OTA\r\n");
                return ESP_FAIL;
            }
            else
            {
                printf("http_server_OTA_update_handler: Writing to partition subtype %d at offset 0x%lx\r\n",
                       update_partition->subtype, update_partition->address);
            }

            esp_ota_write(ota_handle, body_start_p, body_part_len);
            content_received += body_part_len;
        }
        else
        {
            esp_ota_write(ota_handle, ota_buff, recv_len);
            content_received += recv_len;
        }

    } while (recv_len > 0 && content_received < content_length);

    if (esp_ota_end(ota_handle) == ESP_OK)
    {
        if (esp_ota_set_boot_partition(update_partition) == ESP_OK)
        {
            const esp_partition_t *boot_partition = esp_ota_get_boot_partition();
            ESP_LOGI(TAG, "http_server_OTA_update_handler: Next boot partition subtype %d at offset 0x%lx",
                     boot_partition->subtype, boot_partition->address);
            flash_successful = true;
        }
        else
        {
            ESP_LOGI(TAG, "http_server_OTA_update_handler: FLASHED ERROR!!!");
        }
    }
    else
    {
        ESP_LOGI(TAG, "http_server_OTA_update_handler: esp_ota_end ERROR!!!");
    }

    if (flash_successful) {
        http_server_monitor_send_message(HTTP_MSG_OTA_UPDATE_SUCCESSFUL);
    } else {
        http_server_monitor_send_message(HTTP_MSG_OTA_UPDATE_FAILED);
    }

    return ESP_OK;
}

esp_err_t http_server_OTA_status_handler(httpd_req_t *req)
{
    char otaJSON[100];

    ESP_LOGI(TAG, "OTAstatus requested");

    sprintf(otaJSON,
            "{\"ota_update_status\":%d,\"compile_time\":\"%s\",\"compile_date\":\"%s\"}",
            g_fw_update_status, __TIME__, __DATE__);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, otaJSON, strlen(otaJSON));

    return ESP_OK;
}

/*======================================================================
 *  MODO PROGRAMADO – HANDLERS (GET/SET/ERASE)
 *====================================================================*/

// GET /get_program.json?id=N
static esp_err_t http_server_program_get_handler(httpd_req_t *req)
{
    char param[8];
    int id = 0;

    if (httpd_req_get_url_query_len(req) > 0) {
        char buf[64];
        if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK) {
            if (httpd_query_key_value(buf, "id", param, sizeof(param)) == ESP_OK) {
                id = atoi(param);
            }
        }
    }

    if (id < 1 || id > PROGRAM_SLOTS) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, "Invalid id");
        return ESP_FAIL;
    }

    program_slot_t slot;
    program_get_slot(id, &slot);   // devuelve default si está fuera de rango internamente

    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return ESP_FAIL;
    }

    // En el front usas active === 1, así que lo mandamos como número 0/1
    cJSON_AddNumberToObject(root, "active",  slot.active);
    cJSON_AddNumberToObject(root, "h_start", slot.h_start);
    cJSON_AddNumberToObject(root, "m_start", slot.m_start);
    cJSON_AddNumberToObject(root, "h_end",   slot.h_end);
    cJSON_AddNumberToObject(root, "m_end",   slot.m_end);
    cJSON_AddNumberToObject(root, "t0",      slot.t0);
    cJSON_AddNumberToObject(root, "t100",    slot.t100);

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);

    free(json_str);
    return ESP_OK;
}

// POST /set_program.json
// body JSON: { "id":1, "active":1, "h_start":20, "m_start":0, "h_end":21, "m_end":0, "t0":24, "t100":28 }
static esp_err_t http_server_program_set_handler(httpd_req_t *req)
{
    char buf[256];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) {
        return ESP_FAIL;
    }
    buf[len] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        return ESP_FAIL;
    }

    cJSON *j_id     = cJSON_GetObjectItem(root, "id");
    cJSON *j_active = cJSON_GetObjectItem(root, "active");
    cJSON *j_hs     = cJSON_GetObjectItem(root, "h_start");
    cJSON *j_ms     = cJSON_GetObjectItem(root, "m_start");
    cJSON *j_he     = cJSON_GetObjectItem(root, "h_end");
    cJSON *j_me     = cJSON_GetObjectItem(root, "m_end");
    cJSON *j_t0     = cJSON_GetObjectItem(root, "t0");
    cJSON *j_t100   = cJSON_GetObjectItem(root, "t100");

    if (!cJSON_IsNumber(j_id)   || !cJSON_IsNumber(j_active) ||
        !cJSON_IsNumber(j_hs)   || !cJSON_IsNumber(j_ms)     ||
        !cJSON_IsNumber(j_he)   || !cJSON_IsNumber(j_me)     ||
        !cJSON_IsNumber(j_t0)   || !cJSON_IsNumber(j_t100)) {

        cJSON_Delete(root);
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, "Invalid JSON fields");
        return ESP_FAIL;
    }

    int id = j_id->valueint;
    if (id < 1 || id > PROGRAM_SLOTS) {
        cJSON_Delete(root);
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, "Invalid id");
        return ESP_FAIL;
    }

    program_slot_t slot;
    slot.active  = (uint8_t)j_active->valueint;
    slot.h_start = (uint8_t)j_hs->valueint;
    slot.m_start = (uint8_t)j_ms->valueint;
    slot.h_end   = (uint8_t)j_he->valueint;
    slot.m_end   = (uint8_t)j_me->valueint;
    slot.t0      = (int16_t)j_t0->valueint;
    slot.t100    = (int16_t)j_t100->valueint;

    ESP_LOGI(TAG,
             "Saving program id=%d: active=%d %02d:%02d - %02d:%02d, t0=%d, t100=%d",
             id,
             slot.active,
             slot.h_start, slot.m_start,
             slot.h_end,   slot.m_end,
             slot.t0, slot.t100);

    program_set_slot(id, &slot);

    // Opcional: al guardar, cambiar a modo programado
    config_storage_save_mode(2); // 2 = programado

    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    return ESP_OK;
}

// POST /erase_program.json
// body JSON: { "id":1 }
static esp_err_t http_server_program_erase_handler(httpd_req_t *req)
{
    char buf[64];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) {
        return ESP_FAIL;
    }
    buf[len] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        return ESP_FAIL;
    }

    cJSON *j_id = cJSON_GetObjectItem(root, "id");
    if (!cJSON_IsNumber(j_id)) {
        cJSON_Delete(root);
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, "Invalid id");
        return ESP_FAIL;
    }

    int id = j_id->valueint;
    if (id < 1 || id > PROGRAM_SLOTS) {
        cJSON_Delete(root);
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, "Invalid id");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Erasing program id=%d", id);
    program_erase_slot(id);

    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    return ESP_OK;
}

/*======================================================================
 *  RESUMEN DE REGISTROS – /read_regs.json
 *====================================================================*/

// Devuelve algo como:
// { "reg1":"--", "reg2":"20:00-21:00", ... "reg10":"--" }
static esp_err_t http_server_read_regs_summary_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return ESP_FAIL;
    }

    // Mostramos siempre 10 filas en la web, pero solo hay PROGRAM_SLOTS reales
    for (int i = 1; i <= 10; ++i) {
        char key[8];
        snprintf(key, sizeof(key), "reg%d", i);

        if (i > PROGRAM_SLOTS) {
            cJSON_AddStringToObject(root, key, "--");
            continue;
        }

        program_slot_t slot;
        program_get_slot(i, &slot);

        if (!slot.active) {
            cJSON_AddStringToObject(root, key, "--");
        } else {
            char value[32];
            snprintf(value, sizeof(value),
                     "%02d:%02d-%02d:%02d",
                     slot.h_start, slot.m_start,
                     slot.h_end,   slot.m_end);
            cJSON_AddStringToObject(root, key, value);
        }
    }

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);
    free(json_str);
    return ESP_OK;
}

/*======================================================================
 *  WIFI CREDENTIALS
 *====================================================================*/

static esp_err_t http_server_wifi_connect_json_handler(httpd_req_t *req)
{
    size_t header_len;
    char *header_value;
    char *ssid_str = NULL;
    char *pass_str = NULL;
    int content_length;

    ESP_LOGI(TAG, "/wifiConnect.json requested");

    header_len = httpd_req_get_hdr_value_len(req, "Content-Length");
    if (header_len <= 0) {
        ESP_LOGI(TAG, "Content-Length header is missing or invalid");
        return ESP_FAIL;
    }

    header_value = (char *)malloc(header_len + 1);
    if (httpd_req_get_hdr_value_str(req, "Content-Length", header_value, header_len + 1) != ESP_OK) {
        free(header_value);
        ESP_LOGI(TAG, "Failed to get Content-Length header value");
        return ESP_FAIL;
    }

    content_length = atoi(header_value);
    free(header_value);

    if (content_length <= 0) {
        ESP_LOGI(TAG, "Invalid Content-Length value");
        return ESP_FAIL;
    }

    char *data_buffer = (char *)malloc(content_length + 1);

    if (httpd_req_recv(req, data_buffer, content_length) <= 0) {
        free(data_buffer);
        ESP_LOGI(TAG, "Failed to receive request body");
        return ESP_FAIL;
    }

    data_buffer[content_length] = '\0';

    cJSON *root = cJSON_Parse(data_buffer);
    free(data_buffer);

    if (root == NULL) {
        ESP_LOGI(TAG, "Invalid JSON data");
        return ESP_FAIL;
    }

    cJSON *ssid_json = cJSON_GetObjectItem(root, "selectedSSID");
    cJSON *pwd_json  = cJSON_GetObjectItem(root, "pwd");

    if (ssid_json == NULL || pwd_json == NULL ||
        !cJSON_IsString(ssid_json) || !cJSON_IsString(pwd_json)) {
        cJSON_Delete(root);
        ESP_LOGI(TAG, "Missing or invalid JSON data fields");
        return ESP_FAIL;
    }

    ssid_str = strdup(ssid_json->valuestring);
    pass_str = strdup(pwd_json->valuestring);

    cJSON_Delete(root);

    ESP_LOGI(TAG, "Received SSID: %s", ssid_str);
    ESP_LOGI(TAG, "Received Password: %s", pass_str);

    wifi_config_t *wifi_config = wifi_app_get_wifi_config();
    memset(wifi_config, 0x00, sizeof(wifi_config_t));
    memcpy(wifi_config->sta.ssid, ssid_str, strlen(ssid_str));
    memcpy(wifi_config->sta.password, pass_str, strlen(pass_str));
    save_wifi_credentials(ssid_str, pass_str);
    esp_wifi_disconnect();
    connect_to_wifi();

    free(ssid_str);
    free(pass_str);

    return ESP_OK;
}

static esp_err_t http_server_wifi_connect_status_json_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "/wifiConnectStatus requested");

    char statusJSON[100];

    sprintf(statusJSON, "{\"wifi_connect_status\":%d}", g_wifi_connect_status);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, statusJSON, strlen(statusJSON));

    return ESP_OK;
}

/*======================================================================
 *  CONFIGURACIÓN HTTPD
 *====================================================================*/

static httpd_handle_t http_server_configure(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    http_server_monitor_queue_handle =
        xQueueCreate(3, sizeof(http_server_queue_message_t));

    xTaskCreatePinnedToCore(
        &http_server_monitor,
        "http_server_monitor",
        HTTP_SERVER_MONITOR_STACK_SIZE,
        NULL,
        HTTP_SERVER_MONITOR_PRIORITY,
        &task_http_server_monitor,
        HTTP_SERVER_MONITOR_CORE_ID);

    config.core_id       = HTTP_SERVER_TASK_CORE_ID;
    config.task_priority = HTTP_SERVER_TASK_PRIORITY;
    config.stack_size    = HTTP_SERVER_TASK_STACK_SIZE;
    config.max_uri_handlers   = 24;
    config.recv_wait_timeout  = 10;
    config.send_wait_timeout  = 10;

    ESP_LOGI(TAG,
             "http_server_configure: Starting server on port: '%d' with task priority: '%d'",
             config.server_port,
             config.task_priority);

    if (httpd_start(&http_server_handle, &config) == ESP_OK)
    {
        ESP_LOGI(TAG, "http_server_configure: Registering URI handlers");

        // Modo manual: leer configuración
        httpd_uri_t get_manual_cfg_uri = {
            .uri      = "/get_manual_config.json",
            .method   = HTTP_GET,
            .handler  = http_server_get_manual_config_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(http_server_handle, &get_manual_cfg_uri);

        // Modo manual: guardar/aplicar PWM
        httpd_uri_t manual_pwm_uri = {
            .uri      = "/manual_pwm.json",
            .method   = HTTP_POST,
            .handler  = http_server_manual_pwm_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(http_server_handle, &manual_pwm_uri);

        // Modo automático: leer configuración
        httpd_uri_t auto_get = {
            .uri      = "/auto_config.json",
            .method   = HTTP_GET,
            .handler  = http_server_get_auto_config_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(http_server_handle, &auto_get);

        // Modo automático: configurar tmin/tmax
        httpd_uri_t auto_set = {
            .uri      = "/set_auto_config.json",
            .method   = HTTP_POST,
            .handler  = http_server_set_auto_config_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(http_server_handle, &auto_set);

        // Lectura rápida de temperatura
        httpd_uri_t dht_sensor_json = {
            .uri      = "/dhtSensor.json",
            .method   = HTTP_GET,
            .handler  = http_server_get_dht_sensor_readings_json_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(http_server_handle, &dht_sensor_json);

        // Estado general del sistema
        httpd_uri_t status_json = {
            .uri      = "/status.json",
            .method   = HTTP_GET,
            .handler  = http_server_status_json_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(http_server_handle, &status_json);

        // Resumen "Tiempos configurados"
        httpd_uri_t read_regs_uri = {
            .uri      = "/read_regs.json",
            .method   = HTTP_GET,
            .handler  = http_server_read_regs_summary_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(http_server_handle, &read_regs_uri);

        // Modo programado: get / set / erase
        httpd_uri_t program_get_uri = {
            .uri      = "/get_program.json",
            .method   = HTTP_GET,
            .handler  = http_server_program_get_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(http_server_handle, &program_get_uri);

        httpd_uri_t program_set_uri = {
            .uri      = "/set_program.json",
            .method   = HTTP_POST,
            .handler  = http_server_program_set_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(http_server_handle, &program_set_uri);

        httpd_uri_t program_erase_uri = {
            .uri      = "/erase_program.json",
            .method   = HTTP_POST,
            .handler  = http_server_program_erase_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(http_server_handle, &program_erase_uri);

        // Archivos estáticos
        httpd_uri_t jquery_js = {
            .uri      = "/jquery-3.3.1.min.js",
            .method   = HTTP_GET,
            .handler  = http_server_jquery_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(http_server_handle, &jquery_js);

        httpd_uri_t index_html = {
            .uri      = "/",
            .method   = HTTP_GET,
            .handler  = http_server_index_html_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(http_server_handle, &index_html);

        httpd_uri_t app_css = {
            .uri      = "/app.css",
            .method   = HTTP_GET,
            .handler  = http_server_app_css_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(http_server_handle, &app_css);

        httpd_uri_t app_js = {
            .uri      = "/app.js",
            .method   = HTTP_GET,
            .handler  = http_server_app_js_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(http_server_handle, &app_js);

        httpd_uri_t favicon_ico = {
            .uri      = "/favicon.ico",
            .method   = HTTP_GET,
            .handler  = http_server_favicon_ico_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(http_server_handle, &favicon_ico);

        // LED debug
        httpd_uri_t toogle_led_uri = {
            .uri      = "/toogle_led.json",
            .method   = HTTP_POST,
            .handler  = http_server_toogle_led_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(http_server_handle, &toogle_led_uri);

        // Rango temperatura (legacy)
        httpd_uri_t update_temperature_range = {
            .uri      = "/update_temperature_range.json",
            .method   = HTTP_POST,
            .handler  = http_server_update_temperature_range_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(http_server_handle, &update_temperature_range);

        // WiFi
        httpd_uri_t wifi_connect_json = {
            .uri      = "/wifiConnect.json",
            .method   = HTTP_POST,
            .handler  = http_server_wifi_connect_json_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(http_server_handle, &wifi_connect_json);

        httpd_uri_t wifi_connect_status_json = {
            .uri      = "/wifiConnectStatus",
            .method   = HTTP_POST,
            .handler  = http_server_wifi_connect_status_json_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(http_server_handle, &wifi_connect_status_json);

        // OTA
        httpd_uri_t OTA_update = {
            .uri      = "/OTAupdate",
            .method   = HTTP_POST,
            .handler  = http_server_OTA_update_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(http_server_handle, &OTA_update);

        httpd_uri_t OTA_status = {
            .uri      = "/OTAstatus",
            .method   = HTTP_POST,
            .handler  = http_server_OTA_status_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(http_server_handle, &OTA_status);

        return http_server_handle;
    }

    return NULL;
}

/*======================================================================
 *  MODO MANUAL / AUTOMÁTICO (PWM y CONFIG)
 *====================================================================*/

static esp_err_t http_server_get_manual_config_handler(httpd_req_t *req)
{
    uint8_t mode = config_storage_get_mode();
    uint8_t pwm  = config_storage_get_manual_pwm();

    char json[64];
    snprintf(json, sizeof(json),
             "{\"mode\":%u,\"manual_pwm\":%u}", mode, pwm);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, strlen(json));
    return ESP_OK;
}

static esp_err_t http_server_get_auto_config_handler(httpd_req_t *req)
{
    int tmin = config_storage_get_auto_tmin();
    int tmax = config_storage_get_auto_tmax();
    uint8_t mode = config_storage_get_mode();

    char json[80];
    snprintf(json, sizeof(json),
             "{\"tmin\":%d,\"tmax\":%d,\"mode\":%u}",
             tmin, tmax, mode);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, strlen(json));
    return ESP_OK;
}

static esp_err_t http_server_set_auto_config_handler(httpd_req_t *req)
{
    char buf[128];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) return ESP_FAIL;
    buf[len] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (!root) return ESP_FAIL;

    cJSON *j_tmin = cJSON_GetObjectItem(root, "tmin");
    cJSON *j_tmax = cJSON_GetObjectItem(root, "tmax");

    if (!cJSON_IsNumber(j_tmin) || !cJSON_IsNumber(j_tmax)) {
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    int tmin = j_tmin->valueint;
    int tmax = j_tmax->valueint;

    // Guardar en flash
    config_storage_save_auto_tmin(tmin);
    config_storage_save_auto_tmax(tmax);

    // Cambiar modo
    config_storage_save_mode(1);   // 1 = automático

    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    return ESP_OK;
}

static esp_err_t http_server_manual_pwm_handler(httpd_req_t *req)
{
    char buf[64];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) {
        return ESP_FAIL;
    }
    buf[len] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        return ESP_FAIL;
    }

    cJSON *pwm_item = cJSON_GetObjectItem(root, "pwm");
    if (!cJSON_IsNumber(pwm_item)) {
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    int pwm = pwm_item->valueint;
    if (pwm < 0)   pwm = 0;
    if (pwm > 100) pwm = 100;

    cJSON_Delete(root);

    ESP_LOGI("HTTP_MANUAL", "PWM recibido desde web: %d %%", pwm);

    config_storage_save_manual_pwm((uint8_t)pwm);
    config_storage_save_mode(0);      // 0 = manual
    fan_set_percent((uint8_t)pwm);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    return ESP_OK;
}

/*======================================================================
 *  /status.json — ESTADO GENERAL
 *====================================================================*/

static esp_err_t http_server_status_json_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "/status.json requested");

    float    temp_c = ntc_read_celsius();   // temperatura real
    int      pir    = pir_read();           // 0/1
    uint8_t  mode   = config_storage_get_mode();
    uint8_t  pwm    = fan_get_percent();

    char json[128];
    snprintf(json, sizeof(json),
             "{\"temp\":%.1f,\"pir\":%d,\"mode\":%u,\"pwm\":%u}",
             temp_c, pir, mode, pwm);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, strlen(json));
    return ESP_OK;
}

/*======================================================================
 *  TAREA DE CONTROL DEL VENTILADOR
 *====================================================================*/

static void fan_control_task(void *parameter)
{
    while (1) {
        float   temp_c = ntc_read_celsius();
        int     pir    = pir_read();               // 0 ó 1
        uint8_t mode   = config_storage_get_mode();
        uint8_t pwm    = 0;

        if (mode == 0) {
            // MODO MANUAL: no depende de PIR ni temperatura
            pwm = config_storage_get_manual_pwm();
        }
        else if (mode == 1) {
            // MODO AUTOMÁTICO: depende de presencia y T_min/T_max
            if (pir) {
                int tmin = config_storage_get_auto_tmin();
                int tmax = config_storage_get_auto_tmax();

                if (tmax <= tmin) {
                    // Evitar división por cero: si está mal configurado, forzamos 100%
                    pwm = 100;
                } else if (temp_c <= tmin) {
                    pwm = 0;
                } else if (temp_c >= tmax) {
                    pwm = 100;
                } else {
                    pwm = (uint8_t)((temp_c - tmin) * 100.0f /
                                    (float)(tmax - tmin));
                }
            } else {
                // Sin presencia → 0 %
                pwm = 0;
            }
        }
        else if (mode == 2) {
            // MODO PROGRAMADO POR REGISTROS
            if (pir) {
                time_t now = time(NULL);
                struct tm tm_now;
                localtime_r(&now, &tm_now);

                int cur_min = tm_now.tm_hour * 60 + tm_now.tm_min;
                uint8_t pwm_calc = 0;
                bool found = false;

                for (int id = 1; id <= PROGRAM_SLOTS; ++id) {
                    program_slot_t slot;
                    program_get_slot(id, &slot);
                    if (!slot.active) {
                        continue;
                    }

                    int start_min = slot.h_start * 60 + slot.m_start;
                    int end_min   = slot.h_end   * 60 + slot.m_end;

                    bool inside = false;
                    if (start_min == end_min) {
                        // Intervalo vacío, nunca entra
                        inside = false;
                    } else if (start_min < end_min) {
                        // Intervalo normal (no cruza medianoche)
                        inside = (cur_min >= start_min && cur_min < end_min);
                    } else {
                        // Intervalo que cruza medianoche
                        inside = (cur_min >= start_min || cur_min < end_min);
                    }

                    if (inside) {
                        if (slot.t100 <= slot.t0) {
                            pwm_calc = 100;
                        } else if (temp_c <= slot.t0) {
                            pwm_calc = 0;
                        } else if (temp_c >= slot.t100) {
                            pwm_calc = 100;
                        } else {
                            pwm_calc = (uint8_t)((temp_c - slot.t0) * 100.0f /
                                                 (float)(slot.t100 - slot.t0));
                        }
                        found = true;
                        break;   // usamos el primer registro activo que coincida
                    }
                }

                if (found) {
                    pwm = pwm_calc;
                } else {
                    // No hay ningún registro activo que aplique en esta hora
                    pwm = 0;
                }
            } else {
                // Sin presencia → 0 %
                pwm = 0;
            }
        }
        else {
            // Modo desconocido → por seguridad, apagar
            pwm = 0;
        }

        // Aplicar resultado al ventilador
        fan_set_percent(pwm);

        // Periodo de actualización (1 s)
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/*======================================================================
 *  ARRANQUE / PARADA DEL SERVIDOR
 *====================================================================*/

void http_server_start(void)
{
    if (http_server_handle == NULL) {
        http_server_handle = http_server_configure();
        if (http_server_handle) {
            ESP_LOGI("HTTP_SERVER", "HTTP server started");

            // Crear tarea de control del ventilador (si no existe)
            if (fan_control_task_handle == NULL) {
                xTaskCreatePinnedToCore(
                    fan_control_task,
                    "fan_control_task",
                    4096,
                    NULL,
                    5,
                    &fan_control_task_handle,
                    0   
                );
            }

        } else {
            ESP_LOGE("HTTP_SERVER", "Failed to start HTTP server");
        }
    }
}

void http_server_stop(void)
{
    if (http_server_handle)
    {
        httpd_stop(http_server_handle);
        ESP_LOGI(TAG, "http_server_stop: stopping HTTP server");
        http_server_handle = NULL;
    }
    if (task_http_server_monitor)
    {
        vTaskDelete(task_http_server_monitor);
        ESP_LOGI(TAG, "http_server_stop: stopping HTTP server monitor");
        task_http_server_monitor = NULL;
    }
    if (fan_control_task_handle)
    {
        vTaskDelete(fan_control_task_handle);
        ESP_LOGI(TAG, "http_server_stop: stopping fan control task");
        fan_control_task_handle = NULL;
    }
}

BaseType_t http_server_monitor_send_message(http_server_message_e msgID)
{
    http_server_queue_message_t msg;
    msg.msgID = msgID;
    return xQueueSend(http_server_monitor_queue_handle, &msg, portMAX_DELAY);
}

void http_server_fw_update_reset_callback(void *arg)
{
    ESP_LOGI(TAG, "http_server_fw_update_reset_callback: Timer timed-out, restarting the device");
    esp_restart();
}
