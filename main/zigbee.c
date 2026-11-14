#include "settings.h"
#ifdef USE_ZIGBEE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "ezbee/zha.h"
#include "esp_timer.h"
#include "esp_zigbee.h"

#include "zb7.h"

static const char *TAG = "GSB_ZB_7";

static char manufacturer[16], model[16], firmware_version[16];
extern bool connected;

extern bool light_state;
extern bool all_actual;
extern bool motion_state;     //
extern bool motion_state_act; //
// extern uint16_t PresentValue;
#if defined USE_TEMP_CHIP
extern int16_t temperature;
#endif

typedef struct device_params_s
{
    ezb_extaddr_t ieee_addr;
    uint8_t endpoint;
    uint16_t short_addr;
} device_params_t;

device_params_t client_params; // ??

#if defined USE_TEMP_CHIP
/*
static void report_temperature()
{

    if (connected)
    {
        ezb_lock_acquire(portMAX_DELAY);
        ezb_zcl_set_attribute_val(ZB1_ENDPOINT_1,
                                     EZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT,
                                     EZB_ZCL_CLUSTER_SERVER_ROLE,
                                     EZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID,
                                     &temperature,
                                     false);
        //      ezb_lock_release();
        //        ESP_LOGI(TAG, "Set attribute");

        ezb_zcl_report_attr_cmd_t report_attr_cmd = {0};
        report_attr_cmd.address_mode = EZB_APS_ADDR_MODE_16_ENDP_PRESENT;
        report_attr_cmd.attributeID = EZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID;
        report_attr_cmd.direction = EZB_ZCL_CMD_DIRECTION_TO_CLI;
        report_attr_cmd.clusterID = EZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT;
        report_attr_cmd.zcl_basic_cmd.src_endpoint = ZB1_ENDPOINT_1;
        report_attr_cmd.zcl_basic_cmd.dst_addr_u.addr_short = 0;
        report_attr_cmd.zcl_basic_cmd.dst_endpoint = 1;

        //      ezb_lock_acquire(portMAX_DELAY);
        esp_err_t err = ezb_zcl_report_attr_cmd_req(&report_attr_cmd);
        ezb_lock_release();

        if (err != ESP_OK)
            ESP_LOGW(TAG, "Reporting error");
    }
}
    */
#endif
// отправка команды при срабатывании датчика движения
void send_onoff_cmd(uint8_t endpoint, uint8_t state)
{

    ezb_zcl_on_off_cmd_t cmd = {
        .cmd_ctrl = {
            .dst_addr = {
                .addr_mode = EZB_ADDR_MODE_SHORT, // Замена для EZB_APS_ADDR_MODE_16_ENDP_PRESENT
                .u.short_addr = 0x0000,           // Адрес координатора
            },
            .dst_ep = 1,        // Эндпоинт назначения (координатора)
            .src_ep = endpoint, // Ваш эндпоинт
        }
        // Само состояние (on_off_cmd_id) теперь передавать сюда НЕ НАДО!
    };
    esp_zigbee_lock_acquire(portMAX_DELAY);
    // Вместо одной функции с параметром state, вызываем нужную команду в зависимости от state:
    if (state == EZB_ZCL_CMD_ON_OFF_ON_ID)
    {
        ezb_zcl_on_off_on_cmd_req(&cmd); // Отправить ON [48.1.3]
    }
    else if (state == EZB_ZCL_CMD_ON_OFF_OFF_ID)
    {
        ezb_zcl_on_off_off_cmd_req(&cmd); // Отправить OFF [48.1.3]
    }
    else
    {
        ezb_zcl_on_off_toggle_cmd_req(&cmd); // Отправить TOGGLE [48.1.3]
    }
    esp_zigbee_lock_release();
}
/*
// Task for update attribute value
// Автоматически буду отправлять редко, чисто для информации
// Реальную отправку делать при изменении параметра сразу.
void update_attribute(void* attr)
{
    while (true)
    {
        set_attribute();

        vTaskDelay(60000 / portTICK_PERIOD_MS); // 1 раз в 60 секунд
    }
}
*/

void set_attribute()
{
#ifdef USE_ZIGBEE
    if (connected)
    {
        esp_zigbee_lock_acquire(portMAX_DELAY);
        ezb_zcl_set_attr_value(ZB1_ENDPOINT_1,
                               EZB_ZCL_CLUSTER_ID_ON_OFF,
                               EZB_ZCL_CLUSTER_SERVER,
                               EZB_ZCL_ATTR_ON_OFF_ON_OFF_ID,
                               EZB_ZCL_STD_MANUF_CODE,
                               &motion_state,
                               false);

        ezb_zcl_report_attr_cmd_t report_attr_cmd = {0};
        // 1. Адресация и кластер (перенесено в cmd_ctrl)
        report_attr_cmd.cmd_ctrl.dst_addr.addr_mode = EZB_ADDR_MODE_SHORT;
        report_attr_cmd.cmd_ctrl.dst_addr.u.short_addr = 0x0000;
        report_attr_cmd.cmd_ctrl.dst_ep = 1;
        report_attr_cmd.cmd_ctrl.src_ep = ZB1_ENDPOINT_1;
        report_attr_cmd.cmd_ctrl.cluster_id = EZB_ZCL_CLUSTER_ID_ON_OFF;

        // 2. Направление передачи (перенесено в cmd_ctrl.fc)
        report_attr_cmd.cmd_ctrl.fc.direction = EZB_ZCL_CMD_DIRECTION_TO_CLI;
        report_attr_cmd.cmd_ctrl.manuf_code = EZB_ZCL_STD_MANUF_CODE;

        // 3. ID атрибута (перенесено в payload)
        report_attr_cmd.payload.attr_id = EZB_ZCL_ATTR_ON_OFF_ON_OFF_ID;

        esp_err_t err = ezb_zcl_report_attr_cmd_req(&report_attr_cmd);
        esp_zigbee_lock_release();

        if (err != ESP_OK)
            ESP_LOGW(TAG, "Reporting error");

        esp_zigbee_lock_acquire(portMAX_DELAY);
        ezb_zcl_set_attr_value(ZB1_ENDPOINT_1,
                               EZB_ZCL_CLUSTER_ID_TEMPERATURE_MEASUREMENT,
                               EZB_ZCL_CLUSTER_SERVER,
                               EZB_ZCL_ATTR_TEMPERATURE_MEASUREMENT_MEASURED_VALUE_ID,
                               EZB_ZCL_STD_MANUF_CODE,
                               &temperature,
                               false);

        memset(&report_attr_cmd, 0, sizeof(report_attr_cmd));

        report_attr_cmd.cmd_ctrl.dst_addr.addr_mode = EZB_ADDR_MODE_SHORT;
        report_attr_cmd.cmd_ctrl.dst_addr.u.short_addr = 0x0000;
        report_attr_cmd.cmd_ctrl.dst_ep = 1;
        report_attr_cmd.cmd_ctrl.src_ep = ZB1_ENDPOINT_1;
        report_attr_cmd.cmd_ctrl.cluster_id = EZB_ZCL_CLUSTER_ID_TEMPERATURE_MEASUREMENT;

        report_attr_cmd.cmd_ctrl.fc.direction = EZB_ZCL_CMD_DIRECTION_TO_CLI;
        report_attr_cmd.cmd_ctrl.manuf_code = EZB_ZCL_STD_MANUF_CODE;

        report_attr_cmd.payload.attr_id = EZB_ZCL_ATTR_TEMPERATURE_MEASUREMENT_MEASURED_VALUE_ID;
        err = ezb_zcl_report_attr_cmd_req(&report_attr_cmd);
        esp_zigbee_lock_release();
        if (err != ESP_OK)
            ESP_LOGW(TAG, "Reporting error");
    }
#endif
}

void set_zcl_string(char *buffer, char *value)
{
    buffer[0] = (char)strlen(value);
    memcpy(buffer + 1, value, buffer[0]);
}
/*
static void bdb_start_top_level_commissioning_cb(uint8_t mode_mask)
{
    ESP_ERROR_CHECK(ezb_bdb_start_top_level_commissioning(mode_mask));
}
*/
// 1. Колбэк, который выполнится внутри потока Zigbee
static void bdb_start_commissioning_cb(void *ctx)
{
    uint8_t mode = (uint8_t)(uintptr_t)ctx;
    ezb_bdb_start_top_level_commissioning(mode);
}

// 2. Колбэк аппаратного таймера esp_timer (ставит задачу в очередь Zigbee)
static void steering_retry_timer_cb(void *arg)
{
    esp_zigbee_task_queue_post(bdb_start_commissioning_cb, arg);
}

// 3. Вспомогательная функция запуска отложенного Steering
static void schedule_steering_retry(uint32_t delay_ms)
{
    static esp_timer_handle_t retry_timer = NULL;
    if (retry_timer == NULL)
    {
        const esp_timer_create_args_t timer_args = {
            .callback = &steering_retry_timer_cb,
            .arg = (void *)(uintptr_t)EZB_BDB_MODE_NETWORK_STEERING,
            .name = "zb_steering_retry"};
        ESP_ERROR_CHECK(esp_timer_create(&timer_args, &retry_timer));
    }
    esp_timer_stop(retry_timer);
    esp_timer_start_once(retry_timer, delay_ms * 1000ULL); // Перевод мс в мкс
}
// Обработчик сообщений от библиотеки
bool gsb_ezb_app_signal_handler(const ezb_app_signal_t *app_signal)
{
    ESP_LOGI(TAG, "ezb_app_signal_handler start");
    ezb_bdb_comm_status_t *status = (ezb_bdb_comm_status_t *)ezb_app_signal_get_params(app_signal);
    ezb_extaddr_t ieee_addr;
    // Получаем тип сигнала с новым макросом ezb_
    ezb_app_signal_type_t signal_type = ezb_app_signal_get_type(app_signal);

    switch (signal_type)
    {
        // 1. Обязательно обрабатываем этот первый сигнал, чтобы толкнуть стек дальше!
    case EZB_ZDO_SIGNAL_SKIP_STARTUP:
    {
        ESP_LOGI(TAG, "Инициализация Zigbee стека завершена, запускаем BDB...");
        connected = false;
        ezb_bdb_start_top_level_commissioning(EZB_BDB_MODE_INITIALIZATION);
        break;
    }
    case EZB_BDB_SIGNAL_DEVICE_FIRST_START: // Обратите внимание на префикс EZB_
                                            // 2. Получаем указатель на статус BDB-комиссионинга (Новый подход v2.0)

        // 3. Проверяем на успех с использованием нового макроса EZB_BDB_STATUS_SUCCESS
        if (*status == EZB_BDB_STATUS_SUCCESS)
        {
            ESP_LOGI(TAG, "Устройство успешно запустилось! Factory-new: %s",
                     ezb_bdb_is_factory_new() ? "Да" : "Нет");
            ezb_nwk_get_extended_address(&ieee_addr);
            uint8_t *mac = (uint8_t *)&ieee_addr;
            ESP_LOGI(TAG, "Device Extended (MAC) Address: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
                     mac[7], mac[6], mac[5], mac[4],
                     mac[3], mac[2], mac[1], mac[0]);
            // Стартуем поиск сети (Steering)
            ezb_bdb_start_top_level_commissioning(EZB_BDB_MODE_NETWORK_STEERING);
        }
        else
        {
            ESP_LOGE(TAG, "Сбой старта устройства со статусом: 0x%x", *status);
        }
        break;

    case EZB_BDB_SIGNAL_DEVICE_REBOOT:

        // 3. Проверяем на успех с использованием нового макроса EZB_BDB_STATUS_SUCCESS
        if (*status == EZB_BDB_STATUS_SUCCESS)
        {
            ESP_LOGI(TAG, "Устройство успешно запустилось! Factory-new: %s",
                     ezb_bdb_is_factory_new() ? "Да" : "Нет");
            ezb_nwk_get_extended_address(&ieee_addr);
            uint8_t *mac = (uint8_t *)&ieee_addr;
            ESP_LOGI(TAG, "Device Extended (MAC) Address: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
                     mac[7], mac[6], mac[5], mac[4],
                     mac[3], mac[2], mac[1], mac[0]);

            // Стартуем поиск сети (Steering)
            ezb_bdb_start_top_level_commissioning(EZB_BDB_MODE_NETWORK_STEERING);
        }
        else
        {
            ESP_LOGE(TAG, "Сбой старта устройства со статусом: 0x%x", *status);
        }
        break;

    case EZB_BDB_SIGNAL_STEERING:
    {
        ezb_bdb_comm_status_t *status = (ezb_bdb_comm_status_t *)ezb_app_signal_get_params(app_signal);
        if (*status == EZB_BDB_STATUS_SUCCESS)
        {
            ESP_LOGI(TAG, "Успешное подключение к сети! Short Address: 0x%04hx, Channel: %d",
                     ezb_nwk_get_short_address(), ezb_nwk_get_current_channel());
            connected = true;
        }
        else
        {
            ESP_LOGW(TAG, "Сеть не найдена (статус: 0x%x). Повторная попытка через 1 сек...", *status);
            schedule_steering_retry(1000);
        }
        break;
    }
    case EZB_NWK_SIGNAL_NETWORK_STATUS:
    {
        ESP_LOGI(TAG, "Получен сигнал: NETWORK_STATUS");

        break;
    }
    default:
        ESP_LOGI(TAG, "Получен сигнал: %s (0x%x)", ezb_app_signal_to_string(signal_type), signal_type);
        break;
    }

    return true;
}

// обработка команд установки аттрибутов от координатора
// Если функция возвращает ESP_OK, то сообщение дальше обрабатывается библиотекой с фиксацией аттрибутов (???)
esp_err_t zb_set_attribute_handler(const ezb_zcl_set_attr_value_message_t *message)
{
    esp_err_t ret = ESP_OK;
    ezb_zcl_set_attr_value_message_t *attr_msg = (ezb_zcl_set_attr_value_message_t *)message;

    if (attr_msg)
    {
        ESP_RETURN_ON_FALSE(attr_msg->info.status == EZB_ZCL_STATUS_SUCCESS, ESP_ERR_INVALID_ARG, TAG, "Received message: error status(%d)", message->info.status);
        ESP_LOGI(TAG, "Received message: endpoint(%d), cluster(0x%x), attribute(0x%x), data size(%d)",
                 attr_msg->info.dst_ep,
                 attr_msg->info.cluster_id,
                 attr_msg->in.attribute.id,
                 attr_msg->in.attribute.data.size);
    }
    if (attr_msg->info.dst_ep == ZB1_ENDPOINT_1)
    {
    }

    return ret;
}

// обработчик входных сообщений
void zb_action_handler(ezb_zcl_core_action_callback_id_t callback_id, void *message)
{
    //    ESP_LOGI(TAG, "zb_action_handler");
    // Проверяем, если координатор прислал команду Set Attribute (запись атрибута):
    if (callback_id == EZB_ZCL_CORE_SET_ATTR_VALUE_CB_ID)
    {
        // Приводим void* message к типу сообщения изменения атрибута v2.0
        ezb_zcl_set_attr_value_message_t *attr_msg = (ezb_zcl_set_attr_value_message_t *)message;

        if (attr_msg && attr_msg->info.cluster_id == EZB_ZCL_CLUSTER_ID_ON_OFF)
        {
            uint8_t current_state = *(uint8_t *)attr_msg->in.attribute.data.value;
            ESP_LOGI(TAG, "Координатор изменил состояние On/Off на: %d", current_state);
            relay_outer_control(current_state);
        }
    }
}

// задача инициализации ZB

void ezb_task(void *pvParameters)
{
    // Используем стандартный макрос конфигурации для End Device
    esp_zigbee_config_t config = {0};
    // Если вам нужно явно задать роль ZED и параметры Keep-Alive,
    // меняем поля уже ПОСЛЕ создания структуры, чтобы избежать ошибок компилятора со скобками:
    config.device_config.device_type = EZB_NWK_DEVICE_TYPE_END_DEVICE;
    config.device_config.zed_config.ed_timeout = EZB_NWK_ED_TIMEOUT_64MIN;
    config.device_config.zed_config.keep_alive = 3000;

    config.platform_config.radio_config.radio_mode = ESP_ZIGBEE_RADIO_MODE_NATIVE;

    // Инициализируем стек Zigbee
    ESP_ERROR_CHECK(esp_zigbee_init(&config));

    ezb_nwk_set_fast_poll_interval(3000);

    ezb_app_signal_add_handler(gsb_ezb_app_signal_handler);

    // 2. Создаем главный дескриптор устройства (Application Framework device descriptor)
    ezb_af_device_desc_t dev_desc = ezb_af_create_device_desc();

    // 3. Создаем дескриптор эндпоинта
    ezb_af_ep_config_t ep_config = {
        .ep_id = ZB1_ENDPOINT_1,
        .app_profile_id = EZB_AF_HA_PROFILE_ID,
        .app_device_id = EZB_ZHA_ON_OFF_SWITCH_DEVICE_ID,
        .app_device_version = 7};
    ezb_af_ep_desc_t ep_desc = ezb_af_create_endpoint_desc(&ep_config);

    // 4. ОБЪЯВЛЕНИЕ КЛАСТЕРОВ

    // Basic cluster
    // Задаем базовые параметры питания сразу через дефолтную структуру конфигурации
    ezb_zcl_basic_cluster_server_config_t basic_cfg = {
        .zcl_version = EZB_ZCL_BASIC_ZCL_VERSION_DEFAULT_VALUE,
        .power_source = 4, // DC Power (ваше значение dc_power_source = 4)
    };
    ezb_zcl_cluster_desc_t basic_desc = ezb_zcl_basic_create_cluster_desc(&basic_cfg, EZB_ZCL_CLUSTER_SERVER);

    // Добавляем строковые атрибуты в Basic кластер (через явное приведение типов)
    ezb_zcl_basic_cluster_desc_add_attr(basic_desc, EZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, (void *)manufacturer);
    ezb_zcl_basic_cluster_desc_add_attr(basic_desc, EZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, (void *)model);
    ezb_zcl_basic_cluster_desc_add_attr(basic_desc, EZB_ZCL_ATTR_BASIC_SW_BUILD_ID_ID, (void *)firmware_version);

    // Identify cluster
    ezb_zcl_identify_cluster_server_config_t identify_cfg = {
        .identify_time = 0, // Ваше значение identyfi_id = 0
    };
    ezb_zcl_cluster_desc_t identify_desc = ezb_zcl_identify_create_cluster_desc(&identify_cfg, EZB_ZCL_CLUSTER_SERVER);

#if defined USE_TEMP_CHIP
    // Temperature Measurement cluster
    int16_t undefined_value = 0x8000;
    int16_t value_min = 0x954d;
    int16_t value_max = 0x7ffe;

    ezb_zcl_temperature_measurement_cluster_server_config_t temp_cfg = {
        .measured_value = undefined_value,
        .min_measured_value = value_min,
        .max_measured_value = value_max,
    };
    ezb_zcl_cluster_desc_t temp_desc = ezb_zcl_temperature_measurement_create_cluster_desc(&temp_cfg, EZB_ZCL_CLUSTER_SERVER);
#endif

    // OnOff cluster (для датчика движения)
    ezb_zcl_on_off_cluster_server_config_t onoff_cfg = {
        .on_off = motion_state,
    };
    ezb_zcl_cluster_desc_t onoff_desc = ezb_zcl_on_off_create_cluster_desc(&onoff_cfg, EZB_ZCL_CLUSTER_SERVER);

    // 5. СВЯЗЫВАНИЕ: Добавляем созданные кластеры в дескриптор эндпоинта
    ezb_af_endpoint_add_cluster_desc(ep_desc, basic_desc);
    ezb_af_endpoint_add_cluster_desc(ep_desc, identify_desc);
    ezb_af_endpoint_add_cluster_desc(ep_desc, onoff_desc);
#if defined USE_TEMP_CHIP
    ezb_af_endpoint_add_cluster_desc(ep_desc, temp_desc);
#endif

    // 6. Добавляем эндпоинт в общий дескриптор устройства
    ezb_af_device_add_endpoint_desc(dev_desc, ep_desc);

    // 7. РЕГИСТРАЦИЯ УСТРОЙСТВА В СТЕКЕ (вместо старой ezb_device_register)
    ESP_ERROR_CHECK(ezb_af_device_desc_register(dev_desc));

    // 8. Регистрация callback-обработчика входящих ZCL-команд
    ezb_zcl_core_action_handler_register(zb_action_handler);

    // 9. Настройка частотных каналов (используются новые имена bdb функций)
    // Заменяем ненайденный макрос на прямое битовое выражение для всех каналов (11-26)
    // 0x07FFF800 — это битовая маска, открывающая каналы с 11 по 26 включительно
    ezb_bdb_set_primary_channel_set(0x07FFF800);

    // 10. Запуск стека Zigbee
    ESP_ERROR_CHECK(esp_zigbee_start(false));

    // Главный цикл итераций стека v2.0
    esp_zigbee_launch_mainloop();

    // Деинициализация в случае выхода из цикла
    esp_zigbee_deinit();
    vTaskDelete(NULL);
}

#endif
