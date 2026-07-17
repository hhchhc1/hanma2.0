#include <stdio.h>
#include <string.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <nvs_flash.h>
#include <esp_http_client.h>
#include <cJSON.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "dht22.h"
#include "bh1750.h"

#define TAG "C3_SENSOR"

// ================================================================
// 用户配置 —�? 根据您的硬件接线修改这里
// ================================================================

// DHT22 数据引脚
#define DHT22_GPIO          GPIO_NUM_3

// BH1750 I2C 引脚 (软件模拟 I2C)
#define BH1750_SDA_GPIO     GPIO_NUM_6
#define BH1750_SCL_GPIO     GPIO_NUM_7

// 家里的 WiFi 名称和密码 (C3 和 P4 都连同一个路由器)
#define WIFI_SSID           "ZBCK-E"
#define WIFI_PASS           "ZBCK-E123"

// P4 主机 URL (IP 改为 P4 连接家庭 WiFi 的 IP)
#define P4_SERVER_URL       "http://192.168.1.253/api/room2/sensors"

// 数据上报间隔 (�?)
#define REPORT_INTERVAL_SEC 5

// ================================================================

static void wifi_event_handler(void *arg, esp_event_base_t base,
                               int32_t id, void *data)
{
    if (id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "Disconnected, retrying...");
        esp_wifi_connect();
    } else if (id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
    }
}

static void wifi_init(void)
{
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                        &wifi_event_handler, NULL, &instance_any_id);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                        &wifi_event_handler, NULL, &instance_got_ip);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();

    ESP_LOGI(TAG, "Connecting to WiFi: %s", WIFI_SSID);
}

static void send_sensor_data(float temp, float humid, float lux)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "temperature", temp);
    cJSON_AddNumberToObject(root, "humidity", humid);
    cJSON_AddNumberToObject(root, "light", lux);

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    esp_http_client_config_t cfg = {
        .url = P4_SERVER_URL,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 5000,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, json, strlen(json));

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "Report OK, status=%d, data=%s", status, json);
    } else {
        ESP_LOGW(TAG, "Report failed, err=%d, data=%s", err, json);
    }

    esp_http_client_cleanup(client);
    cJSON_free(json);
}

static void sensor_task(void *arg)
{
    ESP_LOGI(TAG, "Sensor task started");

    // 初始化传感器
    dht22_init(DHT22_GPIO);
    dht22_start_reading_task();

    bh1750_init(BH1750_SDA_GPIO, BH1750_SCL_GPIO);
    bh1750_start_reading_task();

    // 等待首次读数
    vTaskDelay(pdMS_TO_TICKS(3000));

    while (1) {
        float temp = (dht22_valid && dht22_temperature > -10 && dht22_temperature < 60) ? dht22_temperature : -1;
        float humid = (dht22_valid && dht22_temperature > -10 && dht22_temperature < 60) ? dht22_humidity : -1;
        float lux = bh1750_valid ? bh1750_lux : -1;

        ESP_LOGI(TAG, "Sensors: temp=%.1f, humid=%.1f, lux=%.0f",
                 temp, humid, lux);

        if (dht22_valid || bh1750_valid) {
            send_sensor_data(temp, humid, lux);
        } else {
            ESP_LOGW(TAG, "No valid sensor data yet");
        }

        vTaskDelay(pdMS_TO_TICKS(REPORT_INTERVAL_SEC * 1000));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "C3 Sensor Node starting...");

    nvs_flash_init();
    wifi_init();

    xTaskCreate(sensor_task, "sensor_task", 4096, NULL, 5, NULL);
}
