// 2024-2026 GSB zb7 v1.7.7
//

#include "settings.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_check.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "ezbee/zha.h"
#include "iot_button.h"
#include "button_gpio.h" // Конфигурация для GPIO под v6.0
#include "driver/gpio.h" // для использования пинов на ввод/вывод
#include "rom/gpio.h"

#ifdef USE_TEMP_CHIP
#include "temp_chip.h"
#endif

#include "zb7.h"

#if !defined CONFIG_ZB_ZED
#error Define CONFIG_ZB_ZED in idf.py menuconfig to compile light (End Device) source code.
#endif

#ifndef USE_ZIGBEE
static const char *TAG = "GSB_ZB_7";
#endif

bool light_state = 0;   // светодиод на плате
bool connected = false; // подключен ли Zigbee

bool motion_state = false;     //  датчик движения
bool motion_state_act = false; //  датчик движения

// актуальность состояния
// bool all_actual = false;

/// uint16_t PresentValue = 0; // итоговое значение для отправки аттрибута в координатор

#if defined USE_TEMP_CHIP
int16_t temperature = -100;
float temp = 0;
bool temp_change = false;
#endif

uint8_t down = 0;
uint8_t up = 1;
uint8_t single = 2;

// Управление реле
// Управляется с координатора
void relay_outer_control(uint8_t value)
{
    gpio_set_level(RELAY_PIN_NUM, (uint32_t)value); //  Выводим его на GPIO12
    motion_state = (bool)value;
    motion_state_act = true;
       if (value == 0)
    {
        light_driver_set_green(45);
        light_driver_set_red(0);
    }
    else
    {
        light_driver_set_green(0);
        light_driver_set_red(45);
    }
    light_driver_set_power(1);
}

// Управление реле
// Включается контроллером по датчику движения
void relay_control(uint8_t value)
{
    gpio_set_level(RELAY_PIN_NUM, (uint32_t)value); //  Выводим его на GPIO12
    motion_state = (bool)value;
    motion_state_act = true;
#ifdef USE_ZIGBEE
    send_onoff_cmd(ZB1_ENDPOINT_1, (uint8_t)motion_state);
    set_attribute();
#else
    ESP_LOGI(TAG, "Реле  %s", motion_state ? "включено" : "выключено");
#endif
    if (value == 0)
    {
        light_driver_set_green(45);
        light_driver_set_red(0);
    }
    else
    {
        light_driver_set_green(0);
        light_driver_set_red(45);
    }
    light_driver_set_power(1);
}

// Обработчик датчика движения (имитация кнопки с длительным нажатием)
void button_click(void *arg, void *usr_data)
{
    uint8_t cmd = *(uint8_t *)usr_data;
#ifndef USE_ZIGBEE
    ESP_LOGI(TAG, "Button click %d", cmd);
#endif
    if (cmd < 2)
        relay_control(cmd % 2);
}

// Регистрация кнопок
void register_buttons()
{
    // create gpio button
    button_config_t btn_cfg = {
        .long_press_time = CONFIG_BUTTON_LONG_PRESS_TIME_MS,  // 1500ms
        .short_press_time = CONFIG_BUTTON_SHORT_PRESS_TIME_MS // 180ms
    };

    const button_gpio_config_t gpio_cfg = {
        .gpio_num = MOTION_PIN_NUM, // Номер пина на датчик движения
        .active_level = 0,          // 0 = активное состояние - нулевой уровень
    };

    button_handle_t btn_handle = NULL;
    esp_err_t ret = iot_button_new_gpio_device(&btn_cfg, &gpio_cfg, &btn_handle);

    if (ret != ESP_OK || btn_handle == NULL)
    {
#ifndef USE_ZIGBEE
        ESP_LOGE(TAG, "Ошибка создания GPIO кнопки!");
#endif
        return;
    }

    iot_button_register_cb(btn_handle, BUTTON_SINGLE_CLICK, NULL, button_click, (void *)&single);
    iot_button_register_cb(btn_handle, BUTTON_LONG_PRESS_START, NULL, button_click, (void *)&down);
    iot_button_register_cb(btn_handle, BUTTON_LONG_PRESS_UP, NULL, button_click, (void *)&up);
}

void app_main(void)
{
    light_driver_init(LIGHT_DEFAULT_ON);
    register_buttons();

    esp_rom_gpio_pad_select_gpio(MOTION_PIN_NUM);        // GPIO1 - на Motion sensor
    gpio_set_direction(MOTION_PIN_NUM, GPIO_MODE_INPUT); // Motion pin - input

    rom_gpio_pad_select_gpio(RELAY_PIN_NUM);
    gpio_set_direction(RELAY_PIN_NUM, GPIO_MODE_OUTPUT); // GPIO12 - на Реле

#ifdef USE_ZIGBEE
    nvs_flash_init_partition("zb_storage");
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    xTaskCreate(ezb_task, "Zigbee_main", 4096, NULL, 5, NULL);
 
#endif
    // xTaskCreate(TaskFunction, NameFunction, StackDepth, void* Parameters, Priority, TaskHandle)

#ifdef USE_TEMP_CHIP
    xTaskCreate(temp_chip_task, "temp_chip_task", 4096, NULL, 3, NULL);
#endif
    light_driver_set_green(45);
    light_driver_set_red(10);
    light_driver_set_blue(20);
    light_driver_set_power(1);

    // При перезапуске выключаем реле
    relay_control(0);
}
