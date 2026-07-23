#include "extra_screens.h"
#include "application.h"
#include "camera/camera_display.h"
#include "display/gomoku.h"
#include "sensors/dht22.h"
#include "sensors/bh1750.h"
#include "driver/gpio.h"

#include <cstring>
#include <esp_log.h>
#include <esp_timer.h>
#include <esp_http_client.h>
#include <wifi_station.h>

#include "mcp_server.h"
#include "smart_home_web_server.h"
#include "matter_device/smart_home_matter_init.h"

#define TAG "ExtraScreens"

#define FAN_INA_GPIO   GPIO_NUM_36
#define FAN_INB_GPIO   GPIO_NUM_7

static const char *WEEKDAYS[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

// 状态变量移到 smart_home_state 命名空间，供 Matter 框架访问
namespace smart_home_state {
    bool fan_on = false;
    bool light_on = false;
    bool auto_mode = false;
} // namespace smart_home_state

// 兼容别名：原代码继续用 s_ 前缀
#define s_fan_on    smart_home_state::fan_on
#define s_light_on  smart_home_state::light_on
#define s_auto_mode smart_home_state::auto_mode

static bool s_fan_gpio_init = false;

static lv_obj_t *s_temp_label = NULL;
static lv_obj_t *s_humid_label = NULL;
static lv_obj_t *s_light_label = NULL;
static lv_obj_t *s_r2_temp_label = NULL;
static lv_obj_t *s_r2_humid_label = NULL;
static lv_obj_t *s_r2_light_label = NULL;
static lv_obj_t *s_fan_btn = NULL;
static lv_obj_t *s_fan_txt = NULL;
static lv_obj_t *s_light_btn = NULL;
static lv_obj_t *s_light_txt = NULL;
static lv_obj_t *s_mode_btn = NULL;
static lv_obj_t *s_mode_txt = NULL;

static lv_obj_t *s_master_date = NULL;
static lv_obj_t *s_master_time = NULL;
static lv_obj_t *s_weather_cont = NULL;
static lv_obj_t *s_ip_label = NULL;
static char s_weather_data[128] = "Loading...";

static lv_obj_t *s_timer_label = NULL;
static lv_obj_t *s_timer_cont = NULL;

const char* get_weather_data(void)
{
    return s_weather_data;
}

static const int HISTORY_POINTS = 120;
static lv_obj_t *s_temp_chart = NULL;
static lv_obj_t *s_humid_chart = NULL;
static lv_obj_t *s_light_chart = NULL;
static lv_chart_series_t *s_temp_ser = NULL;
static lv_chart_series_t *s_humid_ser = NULL;
static lv_chart_series_t *s_light_ser = NULL;
static lv_coord_t *s_temp_data = NULL;
static lv_coord_t *s_humid_data = NULL;
static lv_coord_t *s_light_data = NULL;

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

// 灯泡控制 GPIO12（与摄像头电源共用同一引脚，原项目设计如此）
static void set_light_hw_impl(bool on)
{
    gpio_set_level(GPIO_NUM_12, on ? 1 : 0);
}

// smart_home_state 命名空间的公开接口（供 Matter 框架和 Web API 调用）
namespace smart_home_state {
    void set_fan_hw(bool on) {
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
    void set_light_hw(bool on) { set_light_hw_impl(on); }
}

static void update_fan_ui(void)
{
    set_fan_hw(s_fan_on);
    if (s_fan_on) {
        lv_obj_set_style_bg_color(s_fan_btn, lv_color_hex(0x2196F3), LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(s_fan_btn, LV_OPA_COVER, LV_STATE_DEFAULT);
        lv_label_set_text(s_fan_txt, "FAN\nON");
        lv_obj_set_style_text_color(s_fan_txt, lv_color_hex(0xFFFFFF), LV_STATE_DEFAULT);
    } else {
        lv_obj_set_style_bg_color(s_fan_btn, lv_color_hex(0x555555), LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(s_fan_btn, LV_OPA_COVER, LV_STATE_DEFAULT);
        lv_label_set_text(s_fan_txt, "FAN\nOFF");
        lv_obj_set_style_text_color(s_fan_txt, lv_color_hex(0xBBBBBB), LV_STATE_DEFAULT);
    }
}

static void update_light_ui(void)
{
    set_light_hw_impl(s_light_on);
    if (s_light_on) {
        lv_obj_set_style_bg_color(s_light_btn, lv_color_hex(0xFFC107), LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(s_light_btn, LV_OPA_COVER, LV_STATE_DEFAULT);
        lv_label_set_text(s_light_txt, "LIGHT\nON");
        lv_obj_set_style_text_color(s_light_txt, lv_color_hex(0x000000), LV_STATE_DEFAULT);
    } else {
        lv_obj_set_style_bg_color(s_light_btn, lv_color_hex(0x555555), LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(s_light_btn, LV_OPA_COVER, LV_STATE_DEFAULT);
        lv_label_set_text(s_light_txt, "LIGHT\nOFF");
        lv_obj_set_style_text_color(s_light_txt, lv_color_hex(0xBBBBBB), LV_STATE_DEFAULT);
    }
}

static void update_mode_ui(void)
{
    if (s_auto_mode) {
        lv_obj_set_style_bg_color(s_mode_btn, lv_color_hex(0x4CAF50), LV_STATE_DEFAULT);
        lv_label_set_text(s_mode_txt, "AUTO MODE");
        lv_obj_set_style_text_color(s_mode_txt, lv_color_hex(0xFFFFFF), LV_STATE_DEFAULT);
    } else {
        lv_obj_set_style_bg_color(s_mode_btn, lv_color_hex(0xFF5722), LV_STATE_DEFAULT);
        lv_label_set_text(s_mode_txt, "MANUAL MODE");
        lv_obj_set_style_text_color(s_mode_txt, lv_color_hex(0xFFFFFF), LV_STATE_DEFAULT);
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

static void register_smart_home_mcp_tools(void)
{
    auto &mcp = McpServer::GetInstance();

    mcp.AddTool("self.smart_home.bulb.get_state",
        "Get the current state of the bulb (on/off)",
        PropertyList(),
        [](const PropertyList &) -> ReturnValue {
            return s_light_on ? "{\"power\": true}" : "{\"power\": false}";
        });

    mcp.AddTool("self.smart_home.bulb.turn_on",
        "Turn on the bulb",
        PropertyList(),
        [](const PropertyList &) -> ReturnValue {
            s_light_on = true;
            update_light_ui();
            return true;
        });

    mcp.AddTool("self.smart_home.bulb.turn_off",
        "Turn off the bulb",
        PropertyList(),
        [](const PropertyList &) -> ReturnValue {
            s_light_on = false;
            update_light_ui();
            return true;
        });

    mcp.AddTool("self.smart_home.fan.get_state",
        "Get the current state of the fan (on/off)",
        PropertyList(),
        [](const PropertyList &) -> ReturnValue {
            return s_fan_on ? "{\"power\": true}" : "{\"power\": false}";
        });

    mcp.AddTool("self.smart_home.fan.turn_on",
        "Turn on the fan",
        PropertyList(),
        [](const PropertyList &) -> ReturnValue {
            s_fan_on = true;
            update_fan_ui();
            return true;
        });

    mcp.AddTool("self.smart_home.fan.turn_off",
        "Turn off the fan",
        PropertyList(),
        [](const PropertyList &) -> ReturnValue {
            s_fan_on = false;
            update_fan_ui();
            return true;
        });
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
    const char *init_val, const char *unit_str)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, w, h);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_style_radius(card, 12, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x3A3A3A), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_STATE_DEFAULT);
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
    lv_label_set_text(val, init_val);
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

static void weather_fetch_task(void *arg)
{
    char buf[128];
    while (1) {
        while (!WifiStation::GetInstance().IsConnected()) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
        esp_http_client_config_t cfg = {
            .url = "http://wttr.in/Taiyuan?format=%C|%t&lang=en",
            .timeout_ms = 10000,
        };
        esp_http_client_handle_t client = esp_http_client_init(&cfg);
        if (esp_http_client_open(client, 0) == ESP_OK) {
            esp_http_client_fetch_headers(client);
            int len = esp_http_client_read(client, buf, sizeof(buf) - 1);
            if (len > 0) {
                buf[len] = '\0';
                while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r' || buf[len-1] == ' '))
                    buf[--len] = '\0';
                char *p = buf;
                while (*p == ' ' || *p == '\n' || *p == '\r') p++;
                if (*p) {
                    lv_label_set_text_fmt(s_weather_cont, "Taiyuan Weather: %s", p);
                    strncpy(s_weather_data, p, sizeof(s_weather_data) - 1);
                    s_weather_data[sizeof(s_weather_data) - 1] = '\0';
                }
            }
        }
        esp_http_client_cleanup(client);

        vTaskDelay(pdMS_TO_TICKS(600000));
    }
    vTaskDelete(NULL);
}

static void history_timer_cb(lv_timer_t *timer)
{
    float t = dht22_valid ? dht22_temperature : 0;
    float h = dht22_valid ? dht22_humidity : 0;
    float l = bh1750_valid ? bh1750_lux : 0;

    int32_t tv = (int32_t)(t + 0.5f);
    if (tv < 0) tv = 0;
    if (tv > 40) tv = 40;

    int32_t hv = (int32_t)(h + 0.5f);
    if (hv < 0) hv = 0;
    if (hv > 100) hv = 100;

    int32_t lv = (int32_t)l;
    if (lv < 0) lv = 0;
    if (lv > 1500) lv = 1500;

    memmove(&s_temp_data[1], &s_temp_data[0], (HISTORY_POINTS - 1) * sizeof(lv_coord_t));
    s_temp_data[0] = (lv_coord_t)tv;
    memmove(&s_humid_data[1], &s_humid_data[0], (HISTORY_POINTS - 1) * sizeof(lv_coord_t));
    s_humid_data[0] = (lv_coord_t)hv;
    memmove(&s_light_data[1], &s_light_data[0], (HISTORY_POINTS - 1) * sizeof(lv_coord_t));
    s_light_data[0] = (lv_coord_t)lv;

    if (s_temp_chart) lv_chart_refresh(s_temp_chart);
    if (s_humid_chart) lv_chart_refresh(s_humid_chart);
    if (s_light_chart) lv_chart_refresh(s_light_chart);
}

static lv_obj_t *create_single_chart(lv_obj_t *parent, const char *title,
    lv_coord_t y_max, lv_color_t line_color,
    lv_chart_series_t **out_ser, lv_coord_t *data_arr,
    lv_obj_t **out_chart, const char *unit)
{
    lv_coord_t sw = lv_disp_get_hor_res(NULL);
    lv_coord_t sh = lv_disp_get_ver_res(NULL);

    lv_obj_t *bg = lv_obj_create(parent);
    lv_obj_set_size(bg, sw, sh);
    lv_obj_set_pos(bg, 0, 0);
    lv_obj_set_style_radius(bg, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(bg, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(bg, lv_color_hex(0x2B3A4A), LV_STATE_DEFAULT);
    lv_obj_clear_flag(bg, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title_lbl = lv_label_create(bg);
    lv_label_set_text(title_lbl, title);
    lv_obj_set_style_text_font(title_lbl, &lv_font_montserrat_32, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(title_lbl, lv_color_hex(0xFFFFFF), LV_STATE_DEFAULT);
    lv_obj_align(title_lbl, LV_ALIGN_TOP_MID, 0, 15);

    lv_obj_t *chart = lv_chart_create(bg);
    lv_obj_set_size(chart, sw - 80, sh - 150);
    lv_obj_align(chart, LV_ALIGN_TOP_MID, 0, 65);
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 0, y_max);
    lv_chart_set_point_count(chart, HISTORY_POINTS);
    lv_obj_set_style_bg_opa(chart, LV_OPA_20, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(chart, lv_color_hex(0x000000), LV_STATE_DEFAULT);
    lv_obj_set_style_radius(chart, 12, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(chart, 0, LV_STATE_DEFAULT);
    lv_chart_set_div_line_count(chart, 4, 6);

    lv_obj_set_style_text_color(chart, lv_color_hex(0xFFFFFF), LV_STATE_DEFAULT);

    *out_ser = lv_chart_add_series(chart, line_color, LV_CHART_AXIS_PRIMARY_Y);
    lv_chart_set_ext_y_array(chart, *out_ser, data_arr);

    { // Y-axis labels
        lv_coord_t chart_top = 65;
        lv_coord_t chart_h = sh - 150;
        lv_coord_t pad = 8;
        lv_coord_t content_top = chart_top + pad;
        lv_coord_t content_bot = chart_top + chart_h - pad;
        lv_coord_t content_h = content_bot - content_top;
        for (int i = 0; i < 5; i++) {
            lv_obj_t *yl = lv_label_create(bg);
            int32_t val = y_max * (5 - 1 - i) / (5 - 1);
            lv_label_set_text_fmt(yl, "%d%s", (int)val, unit);
            lv_obj_set_style_text_font(yl, &lv_font_montserrat_16, LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(yl, lv_color_hex(0xAAAAAA), LV_STATE_DEFAULT);
            lv_coord_t y_pos = content_top + content_h * i / (5 - 1) - 8;
            lv_obj_set_pos(yl, 8, y_pos);
        }
    }

    if (out_chart) *out_chart = chart;
    return chart;
}


void extra_screens_create(lv_obj_t *tv)
{
    // 上电立即初始化风扇 GPIO 为低电平，防止引脚浮空导致风扇误转
    if (!s_fan_gpio_init) {
        gpio_set_direction(FAN_INA_GPIO, GPIO_MODE_OUTPUT);
        gpio_set_level(FAN_INA_GPIO, 0);
        gpio_set_direction(FAN_INB_GPIO, GPIO_MODE_OUTPUT);
        gpio_set_level(FAN_INB_GPIO, 0);
        s_fan_gpio_init = true;
    }

    lv_coord_t sw = lv_disp_get_hor_res(NULL);

    /* ========== Tile 0: Master page (clock/date/weather) ========== */
    lv_obj_t *tile0 = lv_tileview_add_tile(tv, 1, 0, LV_DIR_HOR);
    lv_obj_set_style_bg_opa(tile0, LV_OPA_COVER, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(tile0, lv_color_hex(0x55AB55), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(tile0, lv_color_hex(0x55ABDA), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(tile0, LV_GRAD_DIR_VER, LV_STATE_DEFAULT);

    lv_obj_t *title = lv_label_create(tile0);
    lv_label_set_text(title, "Hello master");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_32, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), LV_STATE_DEFAULT);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 25);

    lv_obj_t *dt_cont = lv_obj_create(tile0);
    lv_obj_set_size(dt_cont, sw - 100, 110);
    lv_obj_set_style_radius(dt_cont, 16, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(dt_cont, lv_color_hex(0xFFFFFF), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(dt_cont, LV_OPA_10, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(dt_cont, 0, LV_STATE_DEFAULT);
    lv_obj_clear_flag(dt_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(dt_cont, LV_ALIGN_TOP_MID, 0, 75);

    s_master_date = lv_label_create(dt_cont);
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    lv_label_set_text_fmt(s_master_date, "%04d AD  %02d/%02d  %s",
        tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, WEEKDAYS[tm->tm_wday]);
    lv_obj_set_style_text_font(s_master_date, &lv_font_montserrat_28, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(s_master_date, lv_color_hex(0xFFFFFF), LV_STATE_DEFAULT);
    lv_obj_align(s_master_date, LV_ALIGN_TOP_MID, 0, 8);

    s_master_time = lv_label_create(dt_cont);
    lv_label_set_text_fmt(s_master_time, "%02d:%02d:%02d", tm->tm_hour, tm->tm_min, tm->tm_sec);
    lv_obj_set_style_text_font(s_master_time, &lv_font_montserrat_36, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(s_master_time, lv_color_hex(0xFFFFFF), LV_STATE_DEFAULT);
    lv_obj_align(s_master_time, LV_ALIGN_TOP_MID, 0, 55);

    s_weather_cont = lv_label_create(tile0);
    lv_label_set_text(s_weather_cont, "Loading weather...");
    lv_obj_set_style_text_font(s_weather_cont, &lv_font_montserrat_28, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(s_weather_cont, lv_color_hex(0xFFFFFF), LV_STATE_DEFAULT);
    lv_obj_align(s_weather_cont, LV_ALIGN_TOP_MID, 0, 220);

    s_ip_label = lv_label_create(tile0);
    lv_label_set_text(s_ip_label, "IP: ...");
    lv_obj_set_style_text_font(s_ip_label, &lv_font_montserrat_24, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(s_ip_label, lv_color_hex(0xCCCCCC), LV_STATE_DEFAULT);
    lv_obj_align(s_ip_label, LV_ALIGN_TOP_MID, 0, 260);

    static bool weather_task_started = false;
    if (!weather_task_started) {
        weather_task_started = true;
        xTaskCreate(weather_fetch_task, "weather_fetch", 4096, NULL, 5, NULL);
    }

    /* ========== Reminder Timer (bottom of master page) ========== */
    {
    s_timer_cont = lv_obj_create(tile0);
    lv_obj_set_size(s_timer_cont, sw - 40, 100);
    lv_obj_set_style_radius(s_timer_cont, 16, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(s_timer_cont, lv_color_hex(0xFFFFFF), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(s_timer_cont, LV_OPA_10, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(s_timer_cont, 0, LV_STATE_DEFAULT);
    lv_obj_clear_flag(s_timer_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_timer_cont, LV_OBJ_FLAG_SCROLL_CHAIN_HOR);
    lv_obj_align(s_timer_cont, LV_ALIGN_BOTTOM_MID, 0, -10);

    lv_obj_t *timer_title = lv_label_create(s_timer_cont);
    lv_label_set_text(timer_title, "Reminder");
    lv_obj_set_style_text_font(timer_title, &lv_font_montserrat_28, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(timer_title, lv_color_hex(0xFFFFFF), LV_STATE_DEFAULT);
    lv_obj_align(timer_title, LV_ALIGN_TOP_LEFT, 10, 6);

    struct { const char *label; int seconds; } presets[] = {
        {"1m", 60}, {"3m", 180}, {"5m", 300}, {"10m", 600}
    };
    lv_coord_t b_w = 70;
    lv_coord_t b_h = 34;
    lv_coord_t b_y = 42;
    for (int i = 0; i < 4; i++) {
        lv_obj_t *btn = lv_btn_create(s_timer_cont);
        lv_obj_set_size(btn, b_w, b_h);
        lv_obj_set_pos(btn, 10 + i * (b_w + 8), b_y);
        lv_obj_set_style_radius(btn, 10, LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x2196F3), LV_STATE_DEFAULT);

        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text(lbl, presets[i].label);
        lv_obj_center(lbl);

        int secs = presets[i].seconds;
        lv_obj_add_event_cb(btn, [](lv_event_t *e) {
            int s = (int)(intptr_t)lv_event_get_user_data(e);
            Application::GetInstance().SetReminder(s);
        }, LV_EVENT_CLICKED, (void*)(intptr_t)secs);
    }

    lv_obj_t *cancel_btn = lv_btn_create(s_timer_cont);
    lv_obj_set_size(cancel_btn, b_w, b_h);
    lv_obj_set_pos(cancel_btn, 10 + 4 * (b_w + 8), b_y);
    lv_obj_set_style_radius(cancel_btn, 10, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(cancel_btn, lv_color_hex(0xE53935), LV_STATE_DEFAULT);

    lv_obj_t *cancel_lbl = lv_label_create(cancel_btn);
    lv_label_set_text(cancel_lbl, "Clear");
    lv_obj_center(cancel_lbl);

    lv_obj_add_event_cb(cancel_btn, [](lv_event_t *) {
        Application::GetInstance().SetReminder(0);
    }, LV_EVENT_CLICKED, NULL);

    s_timer_label = lv_label_create(s_timer_cont);
    lv_label_set_text(s_timer_label, "");
    lv_obj_set_style_text_font(s_timer_label, &lv_font_montserrat_28, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(s_timer_label, lv_color_hex(0xFFD54F), LV_STATE_DEFAULT);
    lv_obj_align(s_timer_label, LV_ALIGN_BOTTOM_MID, 0, -4);
    }

    /* ========== Tile 1: Smart Home ========== */
    lv_obj_t *tile1 = lv_tileview_add_tile(tv, 2, 0, LV_DIR_HOR);

    dht22_init(GPIO_NUM_20);
    dht22_start_reading_task();
    bh1750_init(GPIO_NUM_21, GPIO_NUM_33);
    bh1750_start_reading_task();

    lv_obj_set_style_bg_opa(tile1, LV_OPA_COVER, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(tile1, lv_color_hex(0x55AB55), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(tile1, lv_color_hex(0x55ABDA), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(tile1, LV_GRAD_DIR_VER, LV_STATE_DEFAULT);

    lv_obj_t *s_title = lv_label_create(tile1);
    lv_label_set_text(s_title, "Smart Home Control");
    lv_obj_set_style_text_font(s_title, &lv_font_montserrat_32, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(s_title, lv_color_hex(0xFFFFFF), LV_STATE_DEFAULT);
    lv_obj_align(s_title, LV_ALIGN_TOP_MID, 0, 15);

    lv_coord_t card_w = sw / 4;
    lv_coord_t card_h = 130;
    lv_coord_t card_gap = 20;
    lv_coord_t cards_total = 3 * card_w + 2 * card_gap;
    lv_coord_t card_y = 100;
    lv_coord_t card_start_x = (sw - cards_total) / 2;

    s_temp_label = create_data_card(tile1, card_start_x, card_y, card_w, card_h,
        lv_color_hex(0xFF5722), "T", "--.-C", "TEMPERATURE");
    s_humid_label = create_data_card(tile1, card_start_x + card_w + card_gap, card_y, card_w, card_h,
        lv_color_hex(0x2196F3), "H", "--.-%", "HUMIDITY");
    s_light_label = create_data_card(tile1, card_start_x + 2 * (card_w + card_gap), card_y, card_w, card_h,
        lv_color_hex(0xFFC107), "L", "-- lux", "LIGHT");

    lv_coord_t btn_w = sw / 4;
    lv_coord_t btn_h = 80;
    lv_coord_t btn_gap = 30;
    lv_coord_t btns_total = 2 * btn_w + btn_gap;
    lv_coord_t btn_y = card_y + card_h + 30;
    lv_coord_t btn_start_x = (sw - btns_total) / 2;

    s_fan_btn = lv_obj_create(tile1);
    lv_obj_set_size(s_fan_btn, btn_w, btn_h);
    lv_obj_set_pos(s_fan_btn, btn_start_x, btn_y);
    lv_obj_set_style_radius(s_fan_btn, 12, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(s_fan_btn, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(s_fan_btn, 6, LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_color(s_fan_btn, lv_color_hex(0x000000), LV_STATE_DEFAULT);
    lv_obj_add_flag(s_fan_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(s_fan_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_fan_btn, fan_btn_cb, LV_EVENT_CLICKED, NULL);

    s_fan_txt = lv_label_create(s_fan_btn);
    lv_label_set_text(s_fan_txt, "FAN\nOFF");
    lv_obj_set_style_text_font(s_fan_txt, &lv_font_montserrat_24, LV_STATE_DEFAULT);
    lv_obj_center(s_fan_txt);
    update_fan_ui();

    s_light_btn = lv_obj_create(tile1);
    lv_obj_set_size(s_light_btn, btn_w, btn_h);
    lv_obj_set_pos(s_light_btn, btn_start_x + btn_w + btn_gap, btn_y);
    lv_obj_set_style_radius(s_light_btn, 12, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(s_light_btn, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(s_light_btn, 6, LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_color(s_light_btn, lv_color_hex(0x000000), LV_STATE_DEFAULT);
    lv_obj_add_flag(s_light_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(s_light_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_light_btn, light_btn_cb, LV_EVENT_CLICKED, NULL);

    s_light_txt = lv_label_create(s_light_btn);
    lv_label_set_text(s_light_txt, "LIGHT\nOFF");
    lv_obj_set_style_text_font(s_light_txt, &lv_font_montserrat_24, LV_STATE_DEFAULT);
    lv_obj_center(s_light_txt);
    update_light_ui();

    lv_coord_t mode_w = sw / 3;
    lv_coord_t mode_h = 50;
    lv_coord_t mode_y = btn_y + btn_h + 25;

    s_mode_btn = lv_obj_create(tile1);
    lv_obj_set_size(s_mode_btn, mode_w, mode_h);
    lv_obj_set_style_radius(s_mode_btn, 25, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(s_mode_btn, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(s_mode_btn, 6, LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_color(s_mode_btn, lv_color_hex(0x000000), LV_STATE_DEFAULT);
    lv_obj_add_flag(s_mode_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(s_mode_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_mode_btn, mode_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_align(s_mode_btn, LV_ALIGN_TOP_MID, 0, mode_y);

    s_mode_txt = lv_label_create(s_mode_btn);
    lv_obj_set_style_text_font(s_mode_txt, &lv_font_montserrat_24, LV_STATE_DEFAULT);
    lv_obj_center(s_mode_txt);
    update_mode_ui();

    /* ========== Room 2 (Remote Sensor) ========== */
    lv_coord_t r2_y = mode_y + mode_h + 15;
    lv_obj_t *r2_title = lv_label_create(tile1);
    lv_label_set_text(r2_title, "ROOM 2 (Remote)");
    lv_obj_set_style_text_font(r2_title, &lv_font_montserrat_24, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(r2_title, lv_color_hex(0xCCCCCC), LV_STATE_DEFAULT);
    lv_obj_align(r2_title, LV_ALIGN_TOP_MID, 0, r2_y);

    lv_coord_t r2_card_w = sw / 5;
    lv_coord_t r2_card_h = 100;
    lv_coord_t r2_gap = 12;
    lv_coord_t r2_total = 3 * r2_card_w + 2 * r2_gap;
    lv_coord_t r2_card_y = r2_y + 30;
    lv_coord_t r2_start_x = (sw - r2_total) / 2;

    s_r2_temp_label = create_data_card(tile1, r2_start_x, r2_card_y, r2_card_w, r2_card_h,
        lv_color_hex(0x8C2608), "T", "--.-C", "TEMP(R2)");
    lv_obj_set_style_bg_color(lv_obj_get_parent(s_r2_temp_label), lv_color_hex(0x3A1A3A), LV_STATE_DEFAULT);
    s_r2_humid_label = create_data_card(tile1, r2_start_x + r2_card_w + r2_gap, r2_card_y, r2_card_w, r2_card_h,
        lv_color_hex(0x003366), "H", "--.-%", "HUMI(R2)");
    lv_obj_set_style_bg_color(lv_obj_get_parent(s_r2_humid_label), lv_color_hex(0x3A1A3A), LV_STATE_DEFAULT);
    s_r2_light_label = create_data_card(tile1, r2_start_x + 2 * (r2_card_w + r2_gap), r2_card_y, r2_card_w, r2_card_h,
        lv_color_hex(0xB8630E), "L", "-- lux", "LUX(R2)");
    lv_obj_set_style_bg_color(lv_obj_get_parent(s_r2_light_label), lv_color_hex(0x3A1A3A), LV_STATE_DEFAULT);

    static bool mcp_registered = false;
    if (!mcp_registered) {
        mcp_registered = true;
        register_smart_home_mcp_tools();
    }

    /* ========== Tile 2: Camera ========== */
    lv_obj_t *tile2 = lv_tileview_add_tile(tv, 3, 0, LV_DIR_HOR);
    camera_display_create(tile2);

    lv_obj_t *thermal_btn = lv_btn_create(tile2);
    lv_obj_set_size(thermal_btn, 120, 36);
    lv_obj_set_style_radius(thermal_btn, 18, LV_STATE_DEFAULT);
    lv_obj_align(thermal_btn, LV_ALIGN_TOP_RIGHT, -8, 8);
    lv_obj_set_style_bg_color(thermal_btn, lv_color_hex(0x333333), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(thermal_btn, 180, LV_STATE_DEFAULT);
    lv_obj_clear_flag(thermal_btn, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *enhance_label = lv_label_create(thermal_btn);
    lv_label_set_text(enhance_label, "🔧 增强 OFF");
    lv_obj_center(enhance_label);
    lv_obj_set_style_text_color(enhance_label, lv_color_hex(0xFFFFFF), LV_STATE_DEFAULT);

    lv_obj_add_event_cb(thermal_btn, [](lv_event_t *e) {
        bool on = !camera_display_get_thermal_effect();
        camera_display_set_thermal_effect(on);
        lv_obj_t *t = (lv_obj_t *)lv_event_get_target(e);
        lv_obj_t *lbl = lv_obj_get_child(t, 0);
        if (lbl) {
            lv_label_set_text(lbl, on ? "🔧 增强 ON" : "🔧 增强 OFF");
        }
    }, LV_EVENT_CLICKED, NULL);

    /* ========== Tile 3: Temperature History ========== */
    lv_obj_t *tile3 = lv_tileview_add_tile(tv, 4, 0, LV_DIR_HOR);
    s_temp_data = (lv_coord_t*)heap_caps_malloc(HISTORY_POINTS * sizeof(lv_coord_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_temp_data) s_temp_data = (lv_coord_t*)malloc(HISTORY_POINTS * sizeof(lv_coord_t));
    for (int i = 0; i < HISTORY_POINTS; i++) s_temp_data[i] = LV_CHART_POINT_NONE;
    create_single_chart(tile3, "Temperature History", 40, lv_color_hex(0xFF5722), &s_temp_ser, s_temp_data, &s_temp_chart, "C");

    /* ========== Tile 4: Humidity History ========== */
    lv_obj_t *tile4 = lv_tileview_add_tile(tv, 5, 0, LV_DIR_HOR);
    s_humid_data = (lv_coord_t*)heap_caps_malloc(HISTORY_POINTS * sizeof(lv_coord_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_humid_data) s_humid_data = (lv_coord_t*)malloc(HISTORY_POINTS * sizeof(lv_coord_t));
    for (int i = 0; i < HISTORY_POINTS; i++) s_humid_data[i] = LV_CHART_POINT_NONE;
    create_single_chart(tile4, "Humidity History", 100, lv_color_hex(0x2196F3), &s_humid_ser, s_humid_data, &s_humid_chart, "%");

    /* ========== Tile 5: Light History ========== */
    lv_obj_t *tile5 = lv_tileview_add_tile(tv, 6, 0, LV_DIR_HOR);
    s_light_data = (lv_coord_t*)heap_caps_malloc(HISTORY_POINTS * sizeof(lv_coord_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_light_data) s_light_data = (lv_coord_t*)malloc(HISTORY_POINTS * sizeof(lv_coord_t));
    for (int i = 0; i < HISTORY_POINTS; i++) s_light_data[i] = LV_CHART_POINT_NONE;
    create_single_chart(tile5, "Light History", 1500, lv_color_hex(0x4CAF50), &s_light_ser, s_light_data, &s_light_chart, "lux");

    static bool hist_timer_created = false;
    if (!hist_timer_created) {
        hist_timer_created = true;
        lv_timer_create(history_timer_cb, 1000, NULL);
    }

    static bool camera_started = false;
    if (!camera_started) {
        camera_started = true;
        camera_display_start();
    }

    /* ========== Tile 6: Gomoku ========== */
    lv_obj_t *tile6 = lv_tileview_add_tile(tv, 7, 0, LV_DIR_HOR);
    lv_obj_set_style_bg_color(tile6, lv_color_hex(0x1a1a2e), LV_STATE_DEFAULT);
    gomoku_create(tile6);
}

void extra_screens_update_time(void)
{
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);

    if (s_master_date) {
        lv_label_set_text_fmt(s_master_date, "%04d AD  %02d/%02d  %s",
            tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, WEEKDAYS[tm->tm_wday]);
    }
    if (s_master_time) {
        lv_label_set_text_fmt(s_master_time, "%02d:%02d:%02d",
            tm->tm_hour, tm->tm_min, tm->tm_sec);
    }
    if (s_ip_label) {
        static bool ip_resolved = false;
        if (!ip_resolved) {
            auto& sta = WifiStation::GetInstance();
            std::string ip = sta.GetIpAddress();
            if (!ip.empty() && ip != "0.0.0.0") {
                lv_label_set_text_fmt(s_ip_label, "IP: %s", ip.c_str());
                ip_resolved = true;
            }
        }
    }
}

void extra_screens_update_sensor_data(void)
{
    if (s_temp_label) {
        if (dht22_valid) {
            int whole = (int)dht22_temperature;
            int frac = (int)(dht22_temperature * 10) % 10;
            if (frac < 0) frac = -frac;
            lv_label_set_text_fmt(s_temp_label, "%d.%dC", whole, frac);
        } else {
            lv_label_set_text(s_temp_label, "--.-C");
        }
    }
    if (s_humid_label) {
        if (dht22_valid) {
            int whole = (int)dht22_humidity;
            int frac = (int)(dht22_humidity * 10) % 10;
            lv_label_set_text_fmt(s_humid_label, "%d.%d%%", whole, frac);
        } else {
            lv_label_set_text(s_humid_label, "--.-%");
        }
    }
    if (s_light_label) {
        if (bh1750_valid) {
            lv_label_set_text_fmt(s_light_label, "%d lux", (int)bh1750_lux);
        } else {
            lv_label_set_text(s_light_label, "-- lux");
        }
    }

    // Update Room 2 sensor data
    float r2_temp, r2_humid, r2_light;
    int64_t r2_ts;
    smart_home_get_room2_sensors(&r2_temp, &r2_humid, &r2_light, &r2_ts);
    int64_t now = esp_timer_get_time() / 1000000;
    bool r2_valid = (r2_ts > 0 && (now - r2_ts) < 120);

    if (s_r2_temp_label) {
        if (r2_valid) {
            int whole = (int)r2_temp;
            int frac = (int)(r2_temp * 10) % 10;
            if (frac < 0) frac = -frac;
            lv_label_set_text_fmt(s_r2_temp_label, "%d.%dC", whole, frac);
        } else {
            lv_label_set_text(s_r2_temp_label, "--.-C");
        }
    }
    if (s_r2_humid_label) {
        if (r2_valid) {
            int whole = (int)r2_humid;
            int frac = (int)(r2_humid * 10) % 10;
            lv_label_set_text_fmt(s_r2_humid_label, "%d.%d%%", whole, frac);
        } else {
            lv_label_set_text(s_r2_humid_label, "--.-%");
        }
    }
    if (s_r2_light_label) {
        if (r2_valid) {
            lv_label_set_text_fmt(s_r2_light_label, "%d lux", (int)r2_light);
        } else {
            lv_label_set_text(s_r2_light_label, "-- lux");
        }
    }

    if (s_auto_mode) {
        auto_control();
    }
}

void extra_screens_update_timer(void)
{
    if (!s_timer_label || !s_timer_cont) return;
    static int last_remain = -1;
    int remain = Application::GetInstance().GetRemaining();
    if (remain == last_remain) return;
    last_remain = remain;

    if (remain > 0) {
        int mins = remain / 60;
        int secs = remain % 60;
        lv_label_set_text_fmt(s_timer_label, "%02d:%02d", mins, secs);
        lv_obj_clear_flag(s_timer_label, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_timer_label, LV_OBJ_FLAG_HIDDEN);
    }
}

bool smart_home_get_fan_state(void) { return s_fan_on; }
bool smart_home_get_light_state(void) { return s_light_on; }
bool smart_home_get_auto_mode(void) { return s_auto_mode; }

bool smart_home_set_fan_state(bool on)
{
    if (s_auto_mode) return false;  // auto_mode 阻止手动操作
    s_fan_on = on;
    update_fan_ui();
    return true;
}

bool smart_home_set_light_state(bool on)
{
    if (s_auto_mode) return false;
    s_light_on = on;
    update_light_ui();
    return true;
}

bool smart_home_set_auto_mode(bool auto_mode)
{
    s_auto_mode = auto_mode;
    update_mode_ui();
    if (s_auto_mode) {
        auto_control();
    }
    return true;
}
