#include "wifi_settings.h"
#include "board.h"
#include "display.h"
#include <lvgl.h>
#include <font_awesome.h>
#include <ssid_manager.h>
#include <cstring>
#include <string>
#include <vector>
#include <mutex>
#include <algorithm>
#include <esp_log.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define TAG "WifiSettings"

namespace {

const lv_font_t *g_text_font = nullptr;

std::mutex scan_mutex_;
std::vector<wifi_ap_record_t> scan_results_;
bool scan_running_ = false;
bool need_scan_ = false;

lv_obj_t *overlay_ = nullptr;
lv_obj_t *panel_ = nullptr;
lv_obj_t *list_cont_ = nullptr;
lv_obj_t *status_label_ = nullptr;
lv_timer_t *poll_timer_ = nullptr;
TaskHandle_t scan_task_ = nullptr;

// Password dialog
lv_obj_t *pw_overlay_ = nullptr;
lv_obj_t *pw_textarea_ = nullptr;
char pending_ssid_[33] = {};

bool popup_open_ = false;

void request_scan() { need_scan_ = true; }

void close_password_dialog();
void do_connect(const char *ssid, const char *password);
void show_password_dialog(const char *ssid);

// ── Background scan ──
void scan_task_fn(void *arg) {
    while (popup_open_) {
        if (!need_scan_) { vTaskDelay(pdMS_TO_TICKS(500)); continue; }
        need_scan_ = false; scan_running_ = true;
        ESP_LOGI(TAG, "Starting blocking WiFi scan...");
        esp_err_t err = esp_wifi_scan_start(nullptr, true);
        ESP_LOGI(TAG, "Scan returned: %s", esp_err_to_name(err));
        if (err == ESP_OK) {
            uint16_t count = 0;
            esp_wifi_scan_get_ap_num(&count);
            std::vector<wifi_ap_record_t> aps;
            if (count > 0) {
                aps.resize(count);
                esp_wifi_scan_get_ap_records(&count, aps.data());
                std::sort(aps.begin(), aps.end(), [](auto &a, auto &b) { return a.rssi > b.rssi; });
                std::vector<wifi_ap_record_t> deduped;
                for (auto &ap : aps) {
                    if (ap.ssid[0] == '\0') continue;
                    bool seen = false;
                    for (auto &d : deduped)
                        if (strcmp((const char *)d.ssid, (const char *)ap.ssid) == 0) { seen = true; break; }
                    if (!seen) deduped.push_back(ap);
                }
                std::lock_guard<std::mutex> lock(scan_mutex_);
                scan_results_ = std::move(deduped);
            } else {
                std::lock_guard<std::mutex> lock(scan_mutex_);
                scan_results_.clear();
            }
        }
        scan_running_ = false;
        vTaskDelay(pdMS_TO_TICKS(4000));
    }
    scan_task_ = nullptr;
    vTaskDelete(NULL);
}

// ── LVGL poll ──
void poll_timer_cb(lv_timer_t *t) {
    if (!popup_open_ || !list_cont_) return;
    static size_t last_size = (size_t)-1;
    std::vector<wifi_ap_record_t> aps;
    { std::lock_guard<std::mutex> lock(scan_mutex_); aps = scan_results_; }
    if (aps.size() == last_size) return;
    last_size = aps.size();
    lv_obj_clean(list_cont_);
    if (aps.empty()) {
        lv_label_set_text(status_label_, scan_running_ ? "Scanning..." : "No networks found");
    } else {
        char buf[64];
        snprintf(buf, sizeof(buf), "%d network%s found", (int)aps.size(), aps.size() == 1 ? "" : "s");
        lv_label_set_text(status_label_, buf);
    }
    lv_coord_t card_w = lv_obj_get_width(panel_) - 16;
    for (size_t i = 0; i < aps.size(); i++) {
        auto &ap = aps[i];
        lv_obj_t *row = lv_obj_create(list_cont_);
        lv_obj_set_size(row, card_w - 12, 52);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_bg_color(row, lv_color_hex(0x181830), 0);
        lv_obj_set_style_pad_all(row, 6, 0);
        lv_obj_set_style_pad_left(row, 10, 0);
        lv_obj_set_style_radius(row, 8, 0);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t *sig = lv_label_create(row);
        if (ap.rssi >= -55) lv_label_set_text(sig, FONT_AWESOME_WIFI);
        else if (ap.rssi >= -65) lv_label_set_text(sig, FONT_AWESOME_WIFI_FAIR);
        else lv_label_set_text(sig, FONT_AWESOME_WIFI_WEAK);
        lv_obj_set_style_text_font(sig, LV_FONT_DEFAULT, 0);
        lv_obj_set_style_text_color(sig, lv_color_hex(0x4fc3f7), 0);

        lv_obj_t *ssid_lbl = lv_label_create(row);
        lv_obj_set_flex_grow(ssid_lbl, 1);
        lv_label_set_long_mode(ssid_lbl, LV_LABEL_LONG_DOT);
        lv_obj_set_width(ssid_lbl, card_w - 110);
        lv_label_set_text(ssid_lbl, (const char *)ap.ssid);
        if (g_text_font) lv_obj_set_style_text_font(ssid_lbl, g_text_font, 0);
        lv_obj_set_style_text_color(ssid_lbl, lv_color_hex(0xFFFFFF), 0);

        lv_obj_t *lk = lv_label_create(row);
        lv_label_set_text(lk, ap.authmode != WIFI_AUTH_OPEN ? FONT_AWESOME_LOCK : FONT_AWESOME_UNLOCK);
        lv_obj_set_style_text_font(lk, LV_FONT_DEFAULT, 0);
        lv_obj_set_style_text_color(lk, lv_color_hex(0x6868a0), 0);

        char rbuf[8]; snprintf(rbuf, sizeof(rbuf), "%d", ap.rssi);
        lv_obj_t *rdb = lv_label_create(row);
        lv_label_set_text(rdb, rbuf);
        lv_obj_set_style_text_color(rdb, lv_color_hex(0x6868a0), 0);

        struct ApInfo { char ssid[33]; wifi_auth_mode_t authmode; };
        ApInfo *info = new ApInfo;
        strcpy(info->ssid, (const char *)ap.ssid);
        info->authmode = ap.authmode;
        lv_obj_add_event_cb(row, [](lv_event_t *e) {
            auto *d = (ApInfo *)e->user_data;
            ESP_LOGI(TAG, "Clicked: %s auth=%d", d->ssid, (int)d->authmode);
            if (d->authmode == WIFI_AUTH_OPEN) {
                SsidManager::GetInstance().AddSsid(d->ssid, "");
                wifi_settings_close();
                esp_restart();
            } else {
                show_password_dialog(d->ssid);
            }
        }, LV_EVENT_CLICKED, info);
    }
}

// ── Password dialog ──
void close_password_dialog() {
    if (pw_overlay_) { lv_obj_del(pw_overlay_); pw_overlay_ = nullptr; }
    pw_textarea_ = nullptr;
}

void do_connect(const char *ssid, const char *password) {
    SsidManager::GetInstance().AddSsid(ssid, password);
    wifi_settings_close();
    // 保存凭据后直接重启，开机自动连新网
    esp_restart();
}

void show_password_dialog(const char *ssid) {
    close_password_dialog();
    memcpy(pending_ssid_, ssid, 33);

    pw_overlay_ = lv_obj_create(lv_layer_top());
    lv_obj_set_size(pw_overlay_, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(pw_overlay_, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(pw_overlay_, LV_OPA_50, 0);
    lv_obj_set_style_border_width(pw_overlay_, 0, 0);
    lv_obj_add_flag(pw_overlay_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(pw_overlay_, [](lv_event_t *e) {
        if (lv_event_get_target(e) == pw_overlay_) close_password_dialog();
    }, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *card = lv_obj_create(pw_overlay_);
    lv_obj_set_size(card, 360, 180);
    lv_obj_align(card, LV_ALIGN_TOP_MID, 0, 60);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x14142a), 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0x333388), 0);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_pad_all(card, 12, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_CLICKABLE);

    char buf[80];
    snprintf(buf, sizeof(buf), "Password for %s", ssid);
    lv_obj_t *title = lv_label_create(card);
    lv_label_set_text(title, buf);
    if (g_text_font) lv_obj_set_style_text_font(title, g_text_font, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);

    pw_textarea_ = lv_textarea_create(card);
    lv_obj_set_width(pw_textarea_, 320);
    lv_textarea_set_one_line(pw_textarea_, true);
    lv_textarea_set_password_mode(pw_textarea_, true);
    lv_textarea_set_max_length(pw_textarea_, 63);
    lv_textarea_set_placeholder_text(pw_textarea_, "Enter password...");
    lv_obj_set_style_bg_color(pw_textarea_, lv_color_hex(0x0a0a14), 0);
    lv_obj_set_style_text_color(pw_textarea_, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_color(pw_textarea_, lv_color_hex(0x4fc3f7), 0);
    lv_obj_set_style_pad_all(pw_textarea_, 10, 0);
    if (g_text_font) lv_obj_set_style_text_font(pw_textarea_, g_text_font, 0);

    // Buttons
    lv_obj_t *btn_row = lv_obj_create(card);
    lv_obj_set_size(btn_row, 320, 42);
    lv_obj_set_style_border_width(btn_row, 0, 0);
    lv_obj_set_style_bg_opa(btn_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(btn_row, 0, 0);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *cancel_btn = lv_btn_create(btn_row);
    lv_obj_set_size(cancel_btn, 130, 38);
    lv_obj_set_style_radius(cancel_btn, 8, 0);
    lv_obj_set_style_bg_color(cancel_btn, lv_color_hex(0x42426a), 0);
    lv_obj_t *cl = lv_label_create(cancel_btn);
    lv_label_set_text(cl, LV_SYMBOL_CLOSE " Cancel");
    if (g_text_font) lv_obj_set_style_text_font(cl, g_text_font, 0);
    lv_obj_center(cl);
    lv_obj_add_event_cb(cancel_btn, [](lv_event_t *e) { close_password_dialog(); }, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *connect_btn = lv_btn_create(btn_row);
    lv_obj_set_size(connect_btn, 130, 38);
    lv_obj_set_style_radius(connect_btn, 8, 0);
    lv_obj_set_style_bg_color(connect_btn, lv_color_hex(0x1565c0), 0);
    lv_obj_t *cb = lv_label_create(connect_btn);
    lv_label_set_text(cb, LV_SYMBOL_WIFI " Connect");
    lv_obj_set_style_text_color(cb, lv_color_hex(0xFFFFFF), 0);
    if (g_text_font) lv_obj_set_style_text_font(cb, g_text_font, 0);
    lv_obj_center(cb);
    lv_obj_add_event_cb(connect_btn, [](lv_event_t *e) {
        const char *pwd = lv_textarea_get_text(pw_textarea_);
        if (!pwd || strlen(pwd) == 0) return;
        do_connect(pending_ssid_, pwd);
    }, LV_EVENT_CLICKED, nullptr);

    // Keyboard
    lv_obj_t *kb = lv_keyboard_create(pw_overlay_);
    lv_keyboard_set_textarea(kb, pw_textarea_);
    if (g_text_font) lv_obj_set_style_text_font(kb, g_text_font, 0);
}

} // anonymous namespace

// ── Public API ──
void wifi_settings_open(const lv_font_t *text_font) {
    if (popup_open_) return;
    popup_open_ = true;
    g_text_font = text_font;

    overlay_ = lv_obj_create(lv_layer_top());
    lv_obj_set_size(overlay_, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(overlay_, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(overlay_, LV_OPA_50, 0);
    lv_obj_set_style_border_width(overlay_, 0, 0);
    lv_obj_add_flag(overlay_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(overlay_, [](lv_event_t *e) {
        if (lv_event_get_target(e) == overlay_) wifi_settings_close();
    }, LV_EVENT_CLICKED, nullptr);

    panel_ = lv_obj_create(overlay_);
    lv_obj_set_size(panel_, LV_HOR_RES - 40, LV_VER_RES - 80);
    lv_obj_center(panel_);
    lv_obj_set_style_bg_color(panel_, lv_color_hex(0x0a0a14), 0);
    lv_obj_set_style_border_width(panel_, 1, 0);
    lv_obj_set_style_border_color(panel_, lv_color_hex(0x2a2a4a), 0);
    lv_obj_set_style_radius(panel_, 12, 0);
    lv_obj_set_style_pad_all(panel_, 10, 0);
    lv_obj_set_flex_flow(panel_, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(panel_, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *title_row = lv_obj_create(panel_);
    lv_obj_set_size(title_row, lv_pct(100), 32);
    lv_obj_set_style_border_width(title_row, 0, 0);
    lv_obj_set_style_bg_opa(title_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(title_row, 0, 0);
    lv_obj_set_flex_flow(title_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(title_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *title = lv_label_create(title_row);
    lv_label_set_text(title, "WiFi Networks");
    lv_obj_set_style_text_font(title, text_font, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);

    lv_obj_t *close_btn = lv_label_create(title_row);
    lv_label_set_text(close_btn, FONT_AWESOME_XMARK);
    lv_obj_set_style_text_font(close_btn, LV_FONT_DEFAULT, 0);
    lv_obj_set_style_text_color(close_btn, lv_color_hex(0x6868a0), 0);
    lv_obj_add_flag(close_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(close_btn, [](lv_event_t *e) { wifi_settings_close(); }, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *refresh_btn = lv_btn_create(title_row);
    lv_obj_set_size(refresh_btn, 32, 32);
    lv_obj_set_style_radius(refresh_btn, 16, 0);
    lv_obj_set_style_bg_color(refresh_btn, lv_color_hex(0x1565c0), 0);
    lv_obj_t *rf = lv_label_create(refresh_btn);
    lv_label_set_text(rf, FONT_AWESOME_ARROW_RIGHT);
    lv_obj_center(rf);
    lv_obj_add_event_cb(refresh_btn, [](lv_event_t *e) {
        lv_label_set_text(status_label_, "Scanning...");
        request_scan();
    }, LV_EVENT_CLICKED, nullptr);

    status_label_ = lv_label_create(panel_);
    lv_label_set_text(status_label_, "Scanning...");
    lv_obj_set_style_text_color(status_label_, lv_color_hex(0x6868a0), 0);

    list_cont_ = lv_obj_create(panel_);
    lv_obj_set_size(list_cont_, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_grow(list_cont_, 1);
    lv_obj_set_style_bg_color(list_cont_, lv_color_hex(0x0a0a14), 0);
    lv_obj_set_style_border_width(list_cont_, 0, 0);
    lv_obj_set_style_pad_all(list_cont_, 4, 0);
    lv_obj_set_flex_flow(list_cont_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scrollbar_mode(list_cont_, LV_SCROLLBAR_MODE_AUTO);

    xTaskCreate(scan_task_fn, "wifiscan", 4096, nullptr, 2, &scan_task_);
    request_scan();
    poll_timer_ = lv_timer_create(poll_timer_cb, 500, nullptr);
}

void wifi_settings_close() {
    popup_open_ = false;
    if (poll_timer_) { lv_timer_del(poll_timer_); poll_timer_ = nullptr; }
    for (int i = 0; i < 20 && scan_task_; i++) vTaskDelay(pdMS_TO_TICKS(100));
    close_password_dialog();
    if (overlay_) { lv_obj_del(overlay_); overlay_ = nullptr; }
    panel_ = nullptr; list_cont_ = nullptr; status_label_ = nullptr;
    { std::lock_guard<std::mutex> lock(scan_mutex_); scan_results_.clear(); }
}

bool wifi_settings_is_open() { return popup_open_; }
