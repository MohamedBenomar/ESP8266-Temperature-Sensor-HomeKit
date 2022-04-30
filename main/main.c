#include <stdio.h>
#include <stdlib.h>
#include <espressif/esp_common.h>
#include <esp/uart.h>
#include <esp8266.h>
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <string.h>

#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include <wifi_config.h>

#include <dht/dht.h>

const int led_gpio = 2;

#define SENSOR_PIN 0

void led_write(bool on) {
        gpio_write(led_gpio, on ? 0 : 1);
}

void identify_task(void *_args) {
  // We identify the `Temperature Sensor` by Flashing it's LED.
  for (int i=0; i<3; i++) {
          for (int j=0; j<2; j++) {
                  led_write(true);
                  vTaskDelay(100 / portTICK_PERIOD_MS);
                  led_write(false);
                  vTaskDelay(100 / portTICK_PERIOD_MS);
          }

          vTaskDelay(250 / portTICK_PERIOD_MS);
  }

  led_write(false);

  vTaskDelete(NULL);
}

void identify(homekit_value_t _value) {
    printf("identify\n\n");
    xTaskCreate(identify_task, "identify", 128, NULL, 2, NULL);
}

homekit_characteristic_t temperature = HOMEKIT_CHARACTERISTIC_(CURRENT_TEMPERATURE, 0);
homekit_characteristic_t humidity    = HOMEKIT_CHARACTERISTIC_(CURRENT_RELATIVE_HUMIDITY, 0);

void temperature_sensor_callback(uint8_t gpio) {

    gpio_set_pullup(SENSOR_PIN, false, false);

    float humidity_value, temperature_value;
    while (1) {
        bool success = dht_read_float_data(DHT_TYPE_DHT22, SENSOR_PIN, &humidity_value, &temperature_value);
        if (success) {
            temperature.value.float_value = temperature_value;
            humidity.value.float_value = humidity_value;

            homekit_characteristic_notify(&temperature, HOMEKIT_FLOAT(temperature_value));
            homekit_characteristic_notify(&humidity, HOMEKIT_FLOAT(humidity_value));
        } else {
            printf("Couldnt read data from sensor\n");
        }

        vTaskDelay(3000 / portTICK_PERIOD_MS);
    }

}

void temperature_sensor_init() {
    xTaskCreate(temperature_sensor_task, "Temperature Sensor", 256, NULL, 2, NULL);
}

homekit_characteristic_t name = HOMEKIT_CHARACTERISTIC_(NAME, "Temperature Sensor");

homekit_accessory_t *accessories[] = {
        HOMEKIT_ACCESSORY(.id=1, .category=homekit_accessory_category_thermostat, .services=(homekit_service_t*[]){
                HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]){
                        &name,
                        HOMEKIT_CHARACTERISTIC(MANUFACTURER, "Mohamed Benomar"),
                        HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, "83920AZ110N"),
                        HOMEKIT_CHARACTERISTIC(MODEL, "Temperature and Humidity Sensor"),
                        HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "1.0.0"),
                        HOMEKIT_CHARACTERISTIC(IDENTIFY, identify),
                        NULL
                }),
                HOMEKIT_SERVICE(TEMPERATURE_SENSOR, .primary=true, .characteristics=(homekit_characteristic_t*[]){
                        HOMEKIT_CHARACTERISTIC(NAME, "Temperature Sensor"),
                        &temperature,
                        NULL
                }),
                HOMEKIT_SERVICE(HUMIDITY_SENSOR, .characteristics=(homekit_characteristic_t*[]){
                        HOMEKIT_CHARACTERISTIC(NAME, "Humidity Sensor"),
                        &humidity,
                        NULL
                }),
                NULL
        }),
        NULL
};

homekit_server_config_t config = {
        .accessories = accessories,
        .password = "987-44-321",
        .setupId="9SW7"
};

void create_accessory_name() {
        uint8_t macaddr[6];
        sdk_wifi_get_macaddr(STATION_IF, macaddr);

        int name_len = snprintf(NULL, 0, "Sensor de movimiento-%02X%02X%02X",
                                macaddr[3], macaddr[4], macaddr[5]);
        char *name_value = malloc(name_len+1);
        snprintf(name_value, name_len+1, "Sensor de movimiento-%02X%02X%02X",
                 macaddr[3], macaddr[4], macaddr[5]);

        name.value = HOMEKIT_STRING(name_value);
        string id = macaddr[2] + macaddr[3] + macaddr[4] + macaddr[5];
        //config.setupId = id;
}

void on_wifi_ready() {
    homekit_server_init(&config);
}

void user_init(void) {
        uart_set_baud(0, 115200);

        create_accessory_name();

        wifi_config_init("Temperature Sensor", NULL, on_wifi_ready);
}
