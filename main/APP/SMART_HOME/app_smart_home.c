#include "app_smart_home.h"
#include "dht22.h"
#include "bh1750.h"
#include <stdbool.h>
#include "esp_log.h"

static const char *TAG = "smart_home";

#define FAN_INA_GPIO   GPIO_NUM_36
#define FAN_INB_GPIO   GPIO_NUM_7
#define BULB_GPIO      GPIO_NUM_12

static lv_obj_t *temp_label = NULL;
static lv_obj_t *humid_label = NULL;
static lv_obj_t *light_label = NULL;
static lv_obj_t *fan_btn = NULL;
static lv_obj_t *fan_txt = NULL;
static lv_obj_t *light_btn = NULL;
static lv_obj_t *light_txt = NULL;
static lv_obj_t *mode_btn = NULL;
static lv_obj_t *mode_txt = NULL;
static lv_timer_t *sensor_timer = NULL;

static bool s_fan_on = false;
static bool s_light_on = false;
static bool s_auto_mode = false;
static bool s_fan_gpio_init = false;
static bool s_bulb_gpio_init = false;

static void set_fan_hw(bool on)
{
    if (!s_fan_gpio_init) {
        gpio_set_direction(FAN_INA_GPIO, GPIO_MODE_OUTPUT);
        gpio_set_level(FAN_INA_GPIO, 0);
        gpio_set_direction(FAN_INB_GPIO, GPIO_MODE_OUTPUT);
        gpio_set_level(FAN_INB_GPIO, 0);
        s_fan_gpio_init = true;
    }
    gpio_set_level(FAN_INA_GPIO, 0);
    gpio_set_level(FAN_INB_GPIO, on ? 1 : 0);
}

static void set_light_hw(bool on)
{
    if (!s_bulb_gpio_init) {
        gpio_set_direction(BULB_GPIO, GPIO_MODE_OUTPUT);
        gpio_set_level(BULB_GPIO, 0);
        s_bulb_gpio_init = true;
    }
    gpio_set_level(BULB_GPIO, on ? 1 : 0);
}

static void update_fan_ui(void)
{
    set_fan_hw(s_fan_on);
    if (s_fan_on) {
        lv_obj_set_style_bg_opa(fan_btn, LV_OPA_COVER, LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(fan_btn, lv_color_hex(0x2196F3), LV_STATE_DEFAULT);
        lv_label_set_text(fan_txt, "FAN\nON");
        lv_obj_set_style_text_color(fan_txt, lv_color_hex(0xFFFFFF), LV_STATE_DEFAULT);
    } else {
        lv_obj_set_style_bg_opa(fan_btn, LV_OPA_COVER, LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(fan_btn, lv_color_hex(0x555555), LV_STATE_DEFAULT);
        lv_label_set_text(fan_txt, "FAN\nOFF");
        lv_obj_set_style_text_color(fan_txt, lv_color_hex(0xAAAAAA), LV_STATE_DEFAULT);
    }
}

static void update_light_ui(void)
{
    set_light_hw(s_light_on);
    if (s_light_on) {
        lv_obj_set_style_bg_opa(light_btn, LV_OPA_COVER, LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(light_btn, lv_color_hex(0xFFC107), LV_STATE_DEFAULT);
        lv_label_set_text(light_txt, "LIGHT\nON");
        lv_obj_set_style_text_color(light_txt, lv_color_hex(0x000000), LV_STATE_DEFAULT);
    } else {
        lv_obj_set_style_bg_opa(light_btn, LV_OPA_COVER, LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(light_btn, lv_color_hex(0x555555), LV_STATE_DEFAULT);
        lv_label_set_text(light_txt, "LIGHT\nOFF");
        lv_obj_set_style_text_color(light_txt, lv_color_hex(0xAAAAAA), LV_STATE_DEFAULT);
    }
}

static void update_mode_ui(void)
{
    if (s_auto_mode) {
        lv_obj_set_style_bg_color(mode_btn, lv_color_hex(0x4CAF50), LV_STATE_DEFAULT);
        lv_label_set_text(mode_txt, "AUTO MODE");
        lv_obj_set_style_text_color(mode_txt, lv_color_hex(0xFFFFFF), LV_STATE_DEFAULT);
    } else {
        lv_obj_set_style_bg_color(mode_btn, lv_color_hex(0xFF5722), LV_STATE_DEFAULT);
        lv_label_set_text(mode_txt, "MANUAL MODE");
        lv_obj_set_style_text_color(mode_txt, lv_color_hex(0xFFFFFF), LV_STATE_DEFAULT);
    }
}

static void auto_control(void)
{
    bool new_fan = (dht22_valid && dht22_temperature > 28.0f);
    bool new_light = (bh1750_valid && bh1750_lux < 10.0f);

    if (new_fan != s_fan_on) {
        s_fan_on = new_fan;
        update_fan_ui();
    }
    if (new_light != s_light_on) {
        s_light_on = new_light;
        update_light_ui();
    }
}

static void fan_btn_cb(lv_event_t *e)
{
    if (s_auto_mode) return;
    s_fan_on = !s_fan_on;
    update_fan_ui();
}

static void light_btn_cb(lv_event_t *e)
{
    if (s_auto_mode) return;
    s_light_on = !s_light_on;
    update_light_ui();
}

static void mode_btn_cb(lv_event_t *e)
{
    s_auto_mode = !s_auto_mode;
    update_mode_ui();
    if (s_auto_mode) {
        auto_control();
    }
}

static lv_obj_t* create_data_card(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
    lv_coord_t w, lv_coord_t h, lv_color_t icon_bg, const char *icon_str,
    const char *unit_str)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, w, h);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_style_radius(card, 12, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x555555), LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(card, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(card, 8, LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_color(card, lv_color_hex(0x000000), LV_STATE_DEFAULT);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *icon = lv_obj_create(card);
    lv_obj_set_size(icon, 48, 48);
    lv_obj_set_style_radius(icon, 24, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(icon, icon_bg, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(icon, 0, LV_STATE_DEFAULT);
    lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, 4);
    lv_obj_clear_flag(icon, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *ico_txt = lv_label_create(icon);
    lv_label_set_text(ico_txt, icon_str);
    lv_obj_set_style_text_font(ico_txt, &lv_font_montserrat_24, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ico_txt, lv_color_hex(0xFFFFFF), LV_STATE_DEFAULT);
    lv_obj_center(ico_txt);

    lv_obj_t *val = lv_label_create(card);
    lv_label_set_text(val, "--");
    lv_obj_set_style_text_font(val, &lv_font_montserrat_32, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(val, lv_color_hex(0xFFFFFF), LV_STATE_DEFAULT);
    lv_obj_align(val, LV_ALIGN_TOP_MID, 0, 50);

    lv_obj_t *unit_l = lv_label_create(card);
    lv_label_set_text(unit_l, unit_str);
    lv_obj_set_style_text_font(unit_l, &lv_font_montserrat_16, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(unit_l, lv_color_hex(0xAAAAAA), LV_STATE_DEFAULT);
    lv_obj_align(unit_l, LV_ALIGN_TOP_MID, 0, 82);

    return val;
}

static void sensor_timer_cb(lv_timer_t *timer)
{
    if (temp_label) {
        if (dht22_valid) {
            int whole = (int)dht22_temperature;
            int frac = (int)(dht22_temperature * 10) % 10;
            if (frac < 0) frac = -frac;
            lv_label_set_text_fmt(temp_label, "%d.%dC", whole, frac);
        } else {
            lv_label_set_text(temp_label, "--.-C");
        }
    }
    if (humid_label) {
        if (dht22_valid) {
            int whole = (int)dht22_humidity;
            int frac = (int)(dht22_humidity * 10) % 10;
            lv_label_set_text_fmt(humid_label, "%d.%d%%", whole, frac);
        } else {
            lv_label_set_text(humid_label, "--.-%");
        }
    }
    if (light_label) {
        if (bh1750_valid) {
            lv_label_set_text_fmt(light_label, "%d lux", (int)bh1750_lux);
        } else {
            lv_label_set_text(light_label, "-- lux");
        }
    }

    if (s_auto_mode) {
        auto_control();
    }
}

void app_smart_home_init(lv_obj_t *parent)
{
    ESP_LOGI(TAG, "initializing smart home...");

    dht22_init(GPIO_NUM_20);
    dht22_start_reading_task();
    bh1750_init(21, 33);
    bh1750_start_reading_task();

    lv_coord_t sw = lv_obj_get_width(parent);

    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(parent, lv_color_hex(0x55AB55), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(parent, lv_color_hex(0x55ABDA), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(parent, LV_GRAD_DIR_VER, LV_STATE_DEFAULT);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text(title, "Smart Home Control");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_32, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), LV_STATE_DEFAULT);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 15);

    /* ---- Sensor Data Cards ---- */
    lv_coord_t card_w = sw / 4;
    lv_coord_t card_h = 130;
    lv_coord_t card_gap = 20;
    lv_coord_t cards_total = 3 * card_w + 2 * card_gap;
    lv_coord_t card_y = 100;
    lv_coord_t card_start_x = (sw - cards_total) / 2;

    temp_label = create_data_card(parent, card_start_x, card_y, card_w, card_h,
        lv_color_hex(0xFF5722), "T", "TEMPERATURE");
    humid_label = create_data_card(parent, card_start_x + card_w + card_gap, card_y, card_w, card_h,
        lv_color_hex(0x2196F3), "H", "HUMIDITY");
    light_label = create_data_card(parent, card_start_x + 2 * (card_w + card_gap), card_y, card_w, card_h,
        lv_color_hex(0xFFC107), "L", "LIGHT");

    /* ---- Control Buttons ---- */
    lv_coord_t btn_w = sw / 4;
    lv_coord_t btn_h = 80;
    lv_coord_t btn_gap = 30;
    lv_coord_t btns_total = 2 * btn_w + btn_gap;
    lv_coord_t btn_y = card_y + card_h + 30;
    lv_coord_t btn_start_x = (sw - btns_total) / 2;

    fan_btn = lv_obj_create(parent);
    lv_obj_set_size(fan_btn, btn_w, btn_h);
    lv_obj_set_pos(fan_btn, btn_start_x, btn_y);
    lv_obj_set_style_radius(fan_btn, 12, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(fan_btn, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(fan_btn, 6, LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_color(fan_btn, lv_color_hex(0x000000), LV_STATE_DEFAULT);
    lv_obj_add_flag(fan_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(fan_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(fan_btn, fan_btn_cb, LV_EVENT_CLICKED, NULL);

    fan_txt = lv_label_create(fan_btn);
    lv_label_set_text(fan_txt, "FAN\nOFF");
    lv_obj_set_style_text_font(fan_txt, &lv_font_montserrat_24, LV_STATE_DEFAULT);
    lv_obj_center(fan_txt);
    update_fan_ui();

    light_btn = lv_obj_create(parent);
    lv_obj_set_size(light_btn, btn_w, btn_h);
    lv_obj_set_pos(light_btn, btn_start_x + btn_w + btn_gap, btn_y);
    lv_obj_set_style_radius(light_btn, 12, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(light_btn, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(light_btn, 6, LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_color(light_btn, lv_color_hex(0x000000), LV_STATE_DEFAULT);
    lv_obj_add_flag(light_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(light_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(light_btn, light_btn_cb, LV_EVENT_CLICKED, NULL);

    light_txt = lv_label_create(light_btn);
    lv_label_set_text(light_txt, "LIGHT\nOFF");
    lv_obj_set_style_text_font(light_txt, &lv_font_montserrat_24, LV_STATE_DEFAULT);
    lv_obj_center(light_txt);
    update_light_ui();

    /* ---- Mode Toggle ---- */
    lv_coord_t mode_w = sw / 3;
    lv_coord_t mode_h = 50;
    lv_coord_t mode_y = btn_y + btn_h + 25;

    mode_btn = lv_obj_create(parent);
    lv_obj_set_size(mode_btn, mode_w, mode_h);
    lv_obj_set_style_radius(mode_btn, 25, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(mode_btn, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(mode_btn, 6, LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_color(mode_btn, lv_color_hex(0x000000), LV_STATE_DEFAULT);
    lv_obj_add_flag(mode_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(mode_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(mode_btn, mode_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_align(mode_btn, LV_ALIGN_TOP_MID, 0, mode_y);

    mode_txt = lv_label_create(mode_btn);
    lv_obj_set_style_text_font(mode_txt, &lv_font_montserrat_24, LV_STATE_DEFAULT);
    lv_obj_center(mode_txt);
    update_mode_ui();

    sensor_timer = lv_timer_create(sensor_timer_cb, 500, NULL);

    ESP_LOGI(TAG, "smart home initialized");
}
