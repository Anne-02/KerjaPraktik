#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_nimble_hci.h" //run menu config to setup BLE functionality
#include "nimble/nimble_port.h" 
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "sdkconfig.h"
#include "driver/adc.h"
#include "driver/gpio.h"

char *TAG = "BLE-Server";
uint8_t ble_addr_type;
void ble_app_advertise(void);

//Hall Sensor
#define TAG_PINTU "PINTU"
#define HALL_SENSOR_ADC_CHANNEL ADC1_CHANNEL_4 //32
#define RELAY GPIO_NUM_25
#define THRESHOLD 3300
#define DoorClosed raw_adc_value > THRESHOLD
#define DoorOpened raw_adc_value < THRESHOLD
char Pintu [30];
int raw_adc_value;

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
    raw_adc_value = adc1_get_raw(HALL_SENSOR_ADC_CHANNEL);
    if (DoorClosed) {
        sprintf (Pintu, "Pintu Tertutup\n");
        //printf ("Pintu Tertutup\n");
        //printf("Nilai : %d\n", raw_adc_value);
    } else {
        sprintf (Pintu, "Pintu Terbuka\n");
        //printf ("Pintu Terbuka . . . . . .\n");
        //printf("Nilai : %d\n", raw_adc_value);
    }
    vTaskDelay(250/portTICK_PERIOD_MS);
}

// Write data to ESP32 defined as server
static int device_write(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    printf("Data from the client: %.*s\n", ctxt->om->om_len, ctxt->om->om_data);

    uint16_t data_len = OS_MBUF_PKTLEN(ctxt->om);

    if(data_len != sizeof(uint32_t)) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }

    uint32_t data;
    memcpy(&data, ctxt->om->om_data, data_len);

    printf("Received data : %d\n", data);
    return 0;
}

// Read data from ESP32 defined as server
static int device_read(uint16_t con_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    Hallsensor();
    os_mbuf_append(ctxt->om, Pintu, strlen(Pintu));
    return 0;
}

// Array of pointers to other service definitions
// UUID - Universal Unique Identifier
static const struct ble_gatt_svc_def gatt_svcs[] = {
    {.type = BLE_GATT_SVC_TYPE_PRIMARY,
     .uuid = BLE_UUID16_DECLARE(0x180),                 // Define UUID for device type
     .characteristics = (struct ble_gatt_chr_def[]){
         {.uuid = BLE_UUID16_DECLARE(0xFEF4),           // Define UUID for reading
          .flags = BLE_GATT_CHR_F_READ,
          .access_cb = device_read},
         {.uuid = BLE_UUID16_DECLARE(0xDEAD),           // Define UUID for writing
          .flags = BLE_GATT_CHR_F_WRITE,
          .access_cb = device_write},
         {0}}},
    {0}};


// BLE event handling
static int ble_gap_event(struct ble_gap_event *event, void *arg)
{
    switch (event->type)
    {
    // Advertise if connected
    case BLE_GAP_EVENT_CONNECT:
        ESP_LOGI("GAP", "BLE GAP EVENT CONNECT %s", event->connect.status == 0 ? "OK!" : "FAILED!");
        ble_app_advertise();
        break;
    // Advertise if disconnected
    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI("GAP", "BLE GAP EVENT DISCONNECT %s", event->connect.status == 0 ? "OK!" : "FAILED!");
        ble_app_advertise();
        break;
    // Advertise again after completion of the event
    case BLE_GAP_EVENT_ADV_COMPLETE:
        ESP_LOGI("GAP", "BLE GAP EVENT");
        ble_app_advertise();
        break;
    default:
        break;
    }
    return 0;
}

// Define the BLE connection
void ble_app_advertise(void)
{
    // GAP - device name definition
    struct ble_hs_adv_fields fields;
    const char *device_name;
    memset(&fields, 0, sizeof(fields));
    device_name = ble_svc_gap_device_name(); // Read the BLE device name
    fields.name = (uint8_t *)device_name;
    fields.name_len = strlen(device_name);
    fields.name_is_complete = 1;
    ble_gap_adv_set_fields(&fields);

    // GAP - device connectivity definition
    struct ble_gap_adv_params adv_params;
    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND; // connectable or non-connectable
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN; // discoverable or non-discoverable
    ble_gap_adv_start(ble_addr_type, NULL, BLE_HS_FOREVER, &adv_params, ble_gap_event, NULL);
}

// The application
void ble_app_on_sync(void)
{
    ble_hs_id_infer_auto(0, &ble_addr_type); // Determines the best address type automatically
    ble_app_advertise();                     // Define the BLE connection
}

// The infinite task
void host_task(void *param)
{
    nimble_port_run(); // This function will return only when nimble_port_stop() is executed
    vTaskDelay(100/portTICK_PERIOD_MS);
}

void app_main()
{
    nvs_flash_init();                          // 1 - Initialize NVS memory flash using
    esp_nimble_hci_init();                     // 2 - Initialize ESP controller
    nimble_port_init();                        // 3 - Initialize the host stack                      
    ble_svc_gap_device_name_set("BLE-Serv");   // 4 - Initialize NimBLE configuration - server name
    ble_svc_gap_init();                        // 4 - Initialize NimBLE configuration - gap service
    ble_svc_gatt_init();                       // 4 - Initialize NimBLE configuration - gatt service
    ble_gatts_count_cfg(gatt_svcs);            // 4 - Initialize NimBLE configuration - config gatt services
    ble_gatts_add_svcs(gatt_svcs);             // 4 - Initialize NimBLE configuration - queues gatt services.
    ble_hs_cfg.sync_cb = ble_app_on_sync;      // 5 - Initialize application
    nimble_port_freertos_init(host_task);      // 6 - Run the thread
}
