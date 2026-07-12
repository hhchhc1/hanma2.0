#include "gpio_sccb.h"
#include <string.h>
#include <stdio.h>
#include <esp_log.h>
#include <esp_rom_sys.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "esp_sccb_io_interface.h"

#define TAG "gpio_sccb"

#define I2C_DELAY_US 15

typedef struct {
    esp_sccb_io_t base;
    int sda_pin;
    int scl_pin;
    uint16_t dev_addr;
} gpio_sccb_t;

static void gpio_sccb_delay(void)
{
    esp_rom_delay_us(I2C_DELAY_US);
}

static void sda_out(gpio_sccb_t *sccb, int level)
{
    gpio_set_level(sccb->sda_pin, level);
}

static int sda_in(gpio_sccb_t *sccb)
{
    return gpio_get_level(sccb->sda_pin);
}

static void scl_out(gpio_sccb_t *sccb, int level)
{
    gpio_set_level(sccb->scl_pin, level);
}

static void i2c_start(gpio_sccb_t *sccb)
{
    sda_out(sccb, 1);
    scl_out(sccb, 1);
    gpio_sccb_delay();
    sda_out(sccb, 0);
    gpio_sccb_delay();
    scl_out(sccb, 0);
    gpio_sccb_delay();
}

static void i2c_stop(gpio_sccb_t *sccb)
{
    sda_out(sccb, 0);
    scl_out(sccb, 1);
    gpio_sccb_delay();
    sda_out(sccb, 1);
    gpio_sccb_delay();
}

static int i2c_write_byte(gpio_sccb_t *sccb, uint8_t data)
{
    for (int i = 7; i >= 0; i--) {
        sda_out(sccb, (data >> i) & 1);
        gpio_sccb_delay();
        scl_out(sccb, 1);
        gpio_sccb_delay();
        scl_out(sccb, 0);
        gpio_sccb_delay();
    }
    sda_out(sccb, 1);
    gpio_sccb_delay();
    scl_out(sccb, 1);
    gpio_sccb_delay();
    int ack = sda_in(sccb);
    scl_out(sccb, 0);
    gpio_sccb_delay();
    return ack;
}

static uint8_t i2c_read_byte(gpio_sccb_t *sccb, int ack)
{
    uint8_t data = 0;
    sda_out(sccb, 1);
    for (int i = 0; i < 8; i++) {
        data <<= 1;
        scl_out(sccb, 1);
        gpio_sccb_delay();
        data |= sda_in(sccb) ? 1 : 0;
        scl_out(sccb, 0);
        gpio_sccb_delay();
    }
    sda_out(sccb, ack ? 0 : 1);
    gpio_sccb_delay();
    scl_out(sccb, 1);
    gpio_sccb_delay();
    scl_out(sccb, 0);
    gpio_sccb_delay();
    sda_out(sccb, 1);
    return data;
}

static esp_err_t gpio_sccb_transmit(gpio_sccb_t *sccb, const uint8_t *data, size_t len)
{
    i2c_start(sccb);
    if (i2c_write_byte(sccb, (sccb->dev_addr << 1) | 0) != 0) {
        i2c_stop(sccb);
        return ESP_ERR_NOT_FOUND;
    }
    for (size_t i = 0; i < len; i++) {
        if (i2c_write_byte(sccb, data[i]) != 0) {
            i2c_stop(sccb);
            return ESP_ERR_NOT_FOUND;
        }
    }
    return ESP_OK;
}

static esp_err_t gpio_sccb_transmit_receive(gpio_sccb_t *sccb, const uint8_t *wdata, size_t wlen, uint8_t *rdata, size_t rlen)
{
    i2c_start(sccb);
    if (i2c_write_byte(sccb, (sccb->dev_addr << 1) | 0) != 0) {
        i2c_stop(sccb);
        return ESP_ERR_NOT_FOUND;
    }
    for (size_t i = 0; i < wlen; i++) {
        if (i2c_write_byte(sccb, wdata[i]) != 0) {
            i2c_stop(sccb);
            return ESP_ERR_NOT_FOUND;
        }
    }
    i2c_start(sccb);
    if (i2c_write_byte(sccb, (sccb->dev_addr << 1) | 1) != 0) {
        i2c_stop(sccb);
        return ESP_ERR_NOT_FOUND;
    }
    for (size_t i = 0; i < rlen; i++) {
        rdata[i] = i2c_read_byte(sccb, (i < rlen - 1) ? 1 : 0);
    }
    i2c_stop(sccb);
    return ESP_OK;
}

static esp_err_t s_transmit_reg_a8v8(esp_sccb_io_t *io, const uint8_t *wb, size_t ws, int tmo)
{
    (void)tmo;
    gpio_sccb_t *sccb = __containerof(io, gpio_sccb_t, base);
    return gpio_sccb_transmit(sccb, wb, ws);
}

static esp_err_t s_transmit_reg_a16v8(esp_sccb_io_t *io, const uint8_t *wb, size_t ws, int tmo)
{
    (void)tmo;
    gpio_sccb_t *sccb = __containerof(io, gpio_sccb_t, base);
    return gpio_sccb_transmit(sccb, wb, ws);
}

static esp_err_t s_transmit_reg_a8v16(esp_sccb_io_t *io, const uint8_t *wb, size_t ws, int tmo)
{
    (void)tmo;
    gpio_sccb_t *sccb = __containerof(io, gpio_sccb_t, base);
    return gpio_sccb_transmit(sccb, wb, ws);
}

static esp_err_t s_transmit_reg_a16v16(esp_sccb_io_t *io, const uint8_t *wb, size_t ws, int tmo)
{
    (void)tmo;
    gpio_sccb_t *sccb = __containerof(io, gpio_sccb_t, base);
    return gpio_sccb_transmit(sccb, wb, ws);
}

static esp_err_t s_transmit_receive_reg_a8v8(esp_sccb_io_t *io, const uint8_t *wb, size_t ws, uint8_t *rb, size_t rs, int tmo)
{
    (void)tmo;
    gpio_sccb_t *sccb = __containerof(io, gpio_sccb_t, base);
    return gpio_sccb_transmit_receive(sccb, wb, ws, rb, rs);
}

static esp_err_t s_transmit_receive_reg_a16v8(esp_sccb_io_t *io, const uint8_t *wb, size_t ws, uint8_t *rb, size_t rs, int tmo)
{
    (void)tmo;
    gpio_sccb_t *sccb = __containerof(io, gpio_sccb_t, base);
    return gpio_sccb_transmit_receive(sccb, wb, ws, rb, rs);
}

static esp_err_t s_transmit_receive_reg_a8v16(esp_sccb_io_t *io, const uint8_t *wb, size_t ws, uint8_t *rb, size_t rs, int tmo)
{
    (void)tmo;
    gpio_sccb_t *sccb = __containerof(io, gpio_sccb_t, base);
    return gpio_sccb_transmit_receive(sccb, wb, ws, rb, rs);
}

static esp_err_t s_transmit_receive_reg_a16v16(esp_sccb_io_t *io, const uint8_t *wb, size_t ws, uint8_t *rb, size_t rs, int tmo)
{
    (void)tmo;
    gpio_sccb_t *sccb = __containerof(io, gpio_sccb_t, base);
    return gpio_sccb_transmit_receive(sccb, wb, ws, rb, rs);
}

static esp_err_t s_destroy(esp_sccb_io_t *io)
{
    gpio_sccb_t *sccb = __containerof(io, gpio_sccb_t, base);
    free(sccb);
    return ESP_OK;
}

esp_err_t gpio_sccb_new_io(int sda_pin, int scl_pin, uint16_t dev_addr, esp_sccb_io_handle_t *io_handle)
{
    gpio_sccb_t *sccb = calloc(1, sizeof(gpio_sccb_t));
    if (!sccb) return ESP_ERR_NO_MEM;

    sccb->sda_pin = sda_pin;
    sccb->scl_pin = scl_pin;
    sccb->dev_addr = dev_addr;

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << sda_pin) | (1ULL << scl_pin),
        .mode = GPIO_MODE_INPUT_OUTPUT_OD,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    gpio_set_level(sda_pin, 1);
    gpio_set_level(scl_pin, 1);
    gpio_sccb_delay();

    sccb->base.transmit_reg_a8v8 = s_transmit_reg_a8v8;
    sccb->base.transmit_reg_a16v8 = s_transmit_reg_a16v8;
    sccb->base.transmit_reg_a8v16 = s_transmit_reg_a8v16;
    sccb->base.transmit_reg_a16v16 = s_transmit_reg_a16v16;
    sccb->base.transmit_receive_reg_a8v8 = s_transmit_receive_reg_a8v8;
    sccb->base.transmit_receive_reg_a16v8 = s_transmit_receive_reg_a16v8;
    sccb->base.transmit_receive_reg_a8v16 = s_transmit_receive_reg_a8v16;
    sccb->base.transmit_receive_reg_a16v16 = s_transmit_receive_reg_a16v16;
    sccb->base.del = s_destroy;

    *io_handle = &sccb->base;
    return ESP_OK;
}

esp_err_t gpio_sccb_scan(int sda_pin, int scl_pin)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << sda_pin) | (1ULL << scl_pin),
        .mode = GPIO_MODE_INPUT_OUTPUT_OD,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    gpio_set_level(sda_pin, 1);
    gpio_set_level(scl_pin, 1);
    gpio_sccb_delay();

    int found = 0;
    char log_buf[256];
    int pos = 0;
    for (uint8_t addr = 0x08; addr < 0x78; addr++) {
        uint8_t byte = (addr << 1) | 0;

        gpio_set_level(sda_pin, 1);
        gpio_set_level(scl_pin, 1);
        gpio_sccb_delay();
        gpio_set_level(sda_pin, 0);
        gpio_sccb_delay();
        gpio_set_level(scl_pin, 0);
        gpio_sccb_delay();

        for (int i = 7; i >= 0; i--) {
            gpio_set_level(sda_pin, (byte >> i) & 1);
            gpio_sccb_delay();
            gpio_set_level(scl_pin, 1);
            gpio_sccb_delay();
            gpio_set_level(scl_pin, 0);
            gpio_sccb_delay();
        }

        gpio_set_level(sda_pin, 1);
        gpio_sccb_delay();
        gpio_set_level(scl_pin, 1);
        gpio_sccb_delay();
        int ack = gpio_get_level(sda_pin) == 0;
        gpio_set_level(scl_pin, 0);
        gpio_sccb_delay();

        gpio_set_level(sda_pin, 0);
        gpio_set_level(scl_pin, 1);
        gpio_sccb_delay();
        gpio_set_level(sda_pin, 1);
        gpio_sccb_delay();

        if (ack) {
            pos += snprintf(log_buf + pos, sizeof(log_buf) - pos, "0x%02x ", addr);
            found++;
        }
        gpio_sccb_delay();
    }

    if (found) {
        ESP_LOGI(TAG, "SCCB scan found %d device(s): %s", found, log_buf);
    } else {
        ESP_LOGW(TAG, "SCCB scan found NO devices on SDA=%d SCL=%d", sda_pin, scl_pin);
    }
    return found > 0 ? ESP_OK : ESP_ERR_NOT_FOUND;
}


