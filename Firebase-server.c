#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include "driver/adc.h"
#include "driver/gpio.h"

#define urlRTDB "https://selasa4proj-default-rtdb.asia-southeast1.firebasedatabase.app/Door.json"

#define TAG_A "FIREBASE"
#define TAG_B "WiFi"
#define TAG_C "PINTU"

void client_patch_function();

//HallSensor
int raw_adc_value;
#define HALL_SENSOR_ADC_CHANNEL ADC1_CHANNEL_4 //32
#define RELAY GPIO_NUM_25
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
    ESP_LOGW(TAG_C, "Waiting for door to close. . .\n");
    while (DoorOpened) {
        raw_adc_value = adc1_get_raw(HALL_SENSOR_ADC_CHANNEL);
        vTaskDelay(100/portTICK_PERIOD_MS); }
            
    ESP_LOGI(TAG_C, "Pintu Tertutup, ADC Value : %d\n", raw_adc_value);
    sprintf (Pintu, "Pintu Tertutup\n");
    client_patch_function();

    ESP_LOGW(TAG_C, "Waiting for door to open. . .\n");
    while (DoorClosed) {
        raw_adc_value = adc1_get_raw(HALL_SENSOR_ADC_CHANNEL);
        vTaskDelay(100/portTICK_PERIOD_MS); }
        
    ESP_LOGI(TAG_C, "Pintu Terbuka, ADC Value : %d\n", raw_adc_value);
    sprintf (Pintu, "Pintu Terbuka\n");
    client_patch_function();
}

//Client Patch Function
void client_patch_function() {
    esp_http_client_config_t config_patch = {
        .url = urlRTDB,
        .method = HTTP_METHOD_PATCH};

    esp_http_client_handle_t patch = esp_http_client_init(&config_patch);

    esp_http_client_set_header(patch, "Content-Type", "application/json");

    cJSON *patch_json = cJSON_CreateObject();
    cJSON_AddStringToObject(patch_json, "\"Pintu\"", Pintu);

    char *patch_str = cJSON_Print(patch_json);
    esp_http_client_set_post_field(patch, patch_str, strlen(patch_str));

    esp_err_t err = esp_http_client_perform(patch);
    if (err == ESP_OK) {
        ESP_LOGI(TAG_A, "PATCH request successful\n");
    } else {
        ESP_LOGE(TAG_A, "PATCH request failed: %s\n", esp_err_to_name(err));
    } 
    esp_http_client_cleanup(patch);
    cJSON_Delete(patch_json);
    free(patch_str);
}

//WiFi
#define WIFI_SSID "Galaxy A50E3F7"
#define WIFI_PASS "koby6074"

static EventGroupHandle_t s_wifi_event_group;

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG_B, "WiFi Start. \n");
        esp_wifi_connect();
    } else if (event_id == WIFI_EVENT_STA_DISCONNECTED){
        ESP_LOGW(TAG_B, "WiFi Reconnecting . . .\n");
        esp_wifi_connect();
    } else if (event_id == WIFI_EVENT_STA_CONNECTED){
        ESP_LOGI(TAG_B, "WiFi Connected.\n");
        Hallsensor_conf();
        while (true){
            Hallsensor();
            vTaskDelay(200/portTICK_PERIOD_MS);
        }
    }
}

static void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();
    
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    esp_event_handler_instance_t instance_any_id;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    wifi_config_t sta_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .bssid_set = false
        }
    };
    ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &sta_config) );
    ESP_ERROR_CHECK( esp_wifi_start() );
    ESP_ERROR_CHECK( esp_wifi_connect() );
}


static void nvs_init()
{
    ESP_ERROR_CHECK(nvs_flash_erase());
    ESP_ERROR_CHECK(nvs_flash_init());
}

void app_main(void){
    nvs_init(); //non volatile storage initialize
    wifi_init_sta(); //event handler
}