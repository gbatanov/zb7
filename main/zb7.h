#ifndef ZB_1_H
#define ZB_1_H

#include "esp_zigbee.h"
#include "nvs_flash.h"

#include "light_driver.h"

// Zigbee configuration
#define INSTALLCODE_POLICY_ENABLE false // enable the install code policy for security /
#define ED_AGING_TIMEOUT EZB_NWK_ED_AGING_TIMEOUT_64MIN
/// #define ED_KEEP_ALIVE 3000                                               // 10 second
#define ZB1_ENDPOINT_1 1 //
#define MANUFACTURER_NAME "GSB"
#define MODEL_NAME "ZB7"
#define FIRMWARE_VERSION "v0.7.1"

#define MOTION_PIN_NUM GPIO_NUM_1
#define RELAY_PIN_NUM GPIO_NUM_12


#ifdef __cplusplus
extern "C"
{
#endif
    void set_attribute();
    void register_buttons();
    void button_click(void *arg, void *usr_data);
    void relay_control(uint8_t val);
    void relay_outer_control(uint8_t value);
    bool gsb_ezb_app_signal_handler(const ezb_app_signal_t *signal_struct);
    void ezb_task(void *pvParameters);
    void update_attribute(void *attr);
    void send_onoff_cmd(uint8_t endpoint, uint8_t state);
    void zb_action_handler(ezb_zcl_core_action_callback_id_t callback_id, void *message);

#ifdef __cplusplus
}
#endif

#endif