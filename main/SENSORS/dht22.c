#include "dht22.h"
#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "dht22";
static gpio_num_t s_gpio = GPIO_NUM_19;

volatile float dht22_temperature = 0;
volatile float dht22_humidity = 0;
volatile uint8_t dht22_valid = 0;

static void set_output(void)
{
    gpio_set_direction(s_gpio, GPIO_MODE_OUTPUT_OD);
}

static void set_input(void)
{
    gpio_set_direction(s_gpio, GPIO_MODE_INPUT);
}

static int read_sensor(void)
{
    uint8_t data[5] = {0};

    set_output();
    gpio_set_level(s_gpio, 1);
    esp_rom_delay_us(20000);
    gpio_set_level(s_gpio, 0);
    esp_rom_delay_us(20000);
    gpio_set_level(s_gpio, 1);
    esp_rom_delay_us(30);
    set_input();

    uint32_t wait = 0;
    while (gpio_get_level(s_gpio) == 1) {
        if (++wait > 200) return -1;
        esp_rom_delay_us(1);
    }
    wait = 0;
    while (gpio_get_level(s_gpio) == 0) {
        if (++wait > 200) return -2;
        esp_rom_delay_us(1);
    }

    for (int i = 0; i < 40; i++) {
        wait = 0;
        while (gpio_get_level(s_gpio) == 1) {
            if (++wait > 200) return -3;
            esp_rom_delay_us(1);
        }
        wait = 0;
        while (gpio_get_level(s_gpio) == 0) {
            if (++wait > 200) return -4;
            esp_rom_delay_us(1);
        }
        esp_rom_delay_us(35);
        if (gpio_get_level(s_gpio) == 1) {
            data[i / 8] = (data[i / 8] << 1) | 1;
        } else {
            data[i / 8] = (data[i / 8] << 1) | 0;
        }
    }

    uint8_t sum = data[0] + data[1] + data[2] + data[3];
    if ((sum & 0xFF) != data[4]) {
        return -5;
    }

    int16_t hum_raw = ((int16_t)data[0] << 8) | data[1];
    int16_t temp_raw = ((int16_t)data[2] << 8) | data[3];

    if (temp_raw & 0x8000) {
        dht22_temperature = -(temp_raw & 0x7FFF) / 10.0f;
    } else {
        dht22_temperature = temp_raw / 10.0f;
    }
    dht22_humidity = hum_raw / 10.0f;
    dht22_valid = 1;

    return 0;
}

static void dht22_task(void *pvParameters)
{
    while (1) {
        int ret = read_sensor();
        if (ret == 0) {
            ESP_LOGI(TAG, "Temp: %.1fC  Hum: %.1f%%", dht22_temperature, dht22_humidity);
        } else {
            ESP_LOGW(TAG, "read failed (%d)", ret);
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

esp_err_t dht22_init(gpio_num_t gpio_num)
{
    s_gpio = gpio_num;

    gpio_config_t conf = {
        .pin_bit_mask = (1ULL << gpio_num),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&conf));

    ESP_LOGI(TAG, "initialized on GPIO %d", gpio_num);
    return ESP_OK;
}

void dht22_start_reading_task(void)
{
    xTaskCreate(dht22_task, "dht22", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "reading task started");
}
