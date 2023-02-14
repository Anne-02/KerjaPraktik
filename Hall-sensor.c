#include <stdio.h>
#include "driver/adc.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#define HALL_SENSOR_ADC_CHANNEL ADC1_CHANNEL_4

#define RELAY GPIO_NUM_25

#define TAG "PINTU"

int raw_adc_value;
#define THRESHOLD 3300
#define DoorClosed raw_adc_value > THRESHOLD
#define DoorOpened raw_adc_value < THRESHOLD
int state;
char Pintu [50];

static void Hallsensor_conf()
{
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(HALL_SENSOR_ADC_CHANNEL, ADC_ATTEN_DB_0);
    esp_rom_gpio_pad_select_gpio(RELAY);
    gpio_set_direction(RELAY, GPIO_MODE_OUTPUT);
}

static void Hallsensor()
{
    Hallsensor_conf();
    while (true) {
        ESP_LOGW(TAG, "Waiting for door to close. . .\n");
        while (DoorOpened) {
            raw_adc_value = adc1_get_raw(HALL_SENSOR_ADC_CHANNEL);
            vTaskDelay(100/portTICK_PERIOD_MS);
        }

        ESP_LOGI(TAG, "Door closed, ADC value: %d\n", raw_adc_value);
        gpio_set_level(RELAY, 1);

        ESP_LOGW(TAG, "Waiting for door to open. . .\n");
        while (DoorClosed) {
            raw_adc_value = adc1_get_raw(HALL_SENSOR_ADC_CHANNEL);
            vTaskDelay(100/portTICK_PERIOD_MS);
        }

        ESP_LOGI(TAG, "Door opened, ADC value: %d . . . . .\n", raw_adc_value);
        gpio_set_level(RELAY, 0);
    }
}

 void app_main()
{
    Hallsensor();
}