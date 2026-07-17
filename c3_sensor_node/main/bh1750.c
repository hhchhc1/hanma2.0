#include "bh1750.h"
#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "bh1750";

#define BH1750_ADDR_W          0x46
#define BH1750_ADDR_R          0x47
#define BH1750_CMD_POWER_ON    0x01
#define BH1750_CMD_RESET       0x07
#define BH1750_CMD_CONT_HRES   0x10

static int s_sda, s_scl;

volatile float bh1750_lux = 0;
volatile uint8_t bh1750_valid = 0;

#define I2C_DELAY 5

static void i2c_delay(void)
{
    esp_rom_delay_us(I2C_DELAY);
}

static void scl_low(void)  { gpio_set_level(s_scl, 0); }
static void scl_high(void) { gpio_set_level(s_scl, 1); }
static void sda_low(void)  { gpio_set_level(s_sda, 0); }
static void sda_high(void) { gpio_set_level(s_sda, 1); }
static int  sda_read(void) { return gpio_get_level(s_sda); }

static void i2c_start(void)
{
    sda_high(); i2c_delay();
    scl_high(); i2c_delay();
    sda_low();  i2c_delay();
}

static void i2c_stop(void)
{
    sda_low();  i2c_delay();
    scl_high(); i2c_delay();
    sda_high(); i2c_delay();
}

static int i2c_write_byte(uint8_t data)
{
    for (int i = 0; i < 8; i++) {
        scl_low();  i2c_delay();
        if (data & 0x80) sda_high(); else sda_low();
        i2c_delay();
        scl_high(); i2c_delay();
        data <<= 1;
    }
    scl_low();  i2c_delay();
    sda_high(); i2c_delay();
    scl_high(); i2c_delay();
    int ack = sda_read();
    i2c_delay();
    scl_low();  i2c_delay();
    return ack;
}

static uint8_t i2c_read_byte(int send_nack)
{
    uint8_t data = 0;
    sda_high();
    for (int i = 0; i < 8; i++) {
        scl_low();  i2c_delay();
        scl_high(); i2c_delay();
        data = (data << 1) | (sda_read() ? 1 : 0);
    }
    scl_low(); i2c_delay();
    if (send_nack) sda_high(); else sda_low();
    i2c_delay();
    scl_high(); i2c_delay();
    scl_low();  i2c_delay();
    return data;
}

static int bh1750_write_cmd(uint8_t cmd)
{
    i2c_start();
    if (i2c_write_byte(BH1750_ADDR_W)) { i2c_stop(); return -1; }
    if (i2c_write_byte(cmd))           { i2c_stop(); return -1; }
    i2c_stop();
    return 0;
}

static int bh1750_read_raw(uint16_t *raw)
{
    i2c_start();
    if (i2c_write_byte(BH1750_ADDR_R)) { i2c_stop(); return -1; }
    uint8_t hi = i2c_read_byte(0);
    uint8_t lo = i2c_read_byte(1);
    i2c_stop();
    *raw = ((uint16_t)hi << 8) | lo;
    return 0;
}

static void bh1750_task(void *pvParameters)
{
    vTaskDelay(pdMS_TO_TICKS(200));
    ESP_LOGI(TAG, "starting BH1750 communication...");

    int ret;
    ret = bh1750_write_cmd(BH1750_CMD_POWER_ON);
    ESP_LOGI(TAG, "power on: %s", ret == 0 ? "OK" : "FAIL");
    vTaskDelay(pdMS_TO_TICKS(10));

    ret = bh1750_write_cmd(BH1750_CMD_CONT_HRES);
    ESP_LOGI(TAG, "set mode: %s", ret == 0 ? "OK" : "FAIL");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(180));

        uint16_t raw = 0;
        ret = bh1750_read_raw(&raw);
        if (ret == 0) {
            bh1750_lux = raw / 1.2f;
            bh1750_valid = 1;
            ESP_LOGI(TAG, "Lux: %.1f lx (raw: %u)", bh1750_lux, raw);
        } else {
            bh1750_valid = 0;
            ESP_LOGW(TAG, "read failed");
        }
    }
}

esp_err_t bh1750_init(int sda_gpio, int scl_gpio)
{
    s_sda = sda_gpio;
    s_scl = scl_gpio;

    gpio_config_t conf = {
        .pin_bit_mask = (1ULL << sda_gpio) | (1ULL << scl_gpio),
        .mode = GPIO_MODE_INPUT_OUTPUT_OD,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&conf);

    sda_high();
    scl_high();
    esp_rom_delay_us(10000);

    ESP_LOGI(TAG, "init SDA=%d SCL=%d addr=0x%02x", sda_gpio, scl_gpio, BH1750_ADDR_W >> 1);
    return ESP_OK;
}

void bh1750_start_reading_task(void)
{
    xTaskCreate(bh1750_task, "bh1750", 4096, NULL, 5, NULL);
}
