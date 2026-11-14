idf_component_register(
    SRCS
   "zb7.c"
    "light_driver.c"
    "zigbee.c"
    "temp_chip.c"
INCLUDE_DIRS  "../components" "../managed_components" 
REQUIRES nvs_flash espressif__button esp_driver_tsens esp_timer
)
