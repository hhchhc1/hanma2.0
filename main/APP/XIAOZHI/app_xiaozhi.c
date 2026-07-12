#include "app_xiaozhi.h"
#include "xiaozhi_adapter.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#define MAX_CHAT_MSG 40

/* ---- Dark theme ---- */
#define BG_COLOR         lv_color_hex(0x121212)
#define USER_BUBBLE      lv_color_hex(0x1A6C37)
#define ASSISTANT_BUBBLE lv_color_hex(0x333333)
#define SYSTEM_BUBBLE    lv_color_hex(0x2A2A2A)
#define SYSTEM_TEXT      lv_color_hex(0xAAAAAA)

/* Event types for the UI queue */
typedef enum { XZ_EVT_MSG, XZ_EVT_STATUS, XZ_EVT_WIFI } xz_evt_type_t;

typedef struct {
    xz_evt_type_t type;
    char text[256];
    char role[16];
} xz_ui_evt_t;

static lv_obj_t *s_chat_content = NULL;
static lv_obj_t *s_status_label = NULL;
static lv_obj_t *s_wifi_label = NULL;
static lv_obj_t *s_talk_btn = NULL;
static lv_obj_t *s_talk_label = NULL;
static int s_msg_count = 0;
static QueueHandle_t s_ui_queue = NULL;
static bool s_talking = false;

/* ------------- Bubble (called ONLY from LVGL task) ------------- */
static lv_obj_t* create_bubble(lv_obj_t *parent, const char *text, const char *role)
{
    lv_obj_t *container = lv_obj_create(parent);
    lv_obj_set_width(container, lv_pct(100));
    lv_obj_set_height(container, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(container, 0, 0);
    lv_obj_set_style_pad_all(container, 0, 0);
    lv_obj_set_style_pad_top(container, 2, 0);

    lv_obj_t *bubble = lv_obj_create(container);
    lv_obj_set_style_radius(bubble, 10, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(bubble, 1, LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(bubble, lv_color_hex(0x333333), LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(bubble, 8, LV_STATE_DEFAULT);

    lv_obj_t *label = lv_label_create(bubble);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_20, LV_STATE_DEFAULT);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);

    lv_coord_t max_w = 1024 * 85 / 100 - 16;
    lv_coord_t txt_w = lv_txt_get_width(text, strlen(text), &lv_font_montserrat_20, 0, LV_TEXT_FLAG_NONE);
    if (txt_w < 20) txt_w = 20;
    if (txt_w > max_w) txt_w = max_w;
    lv_obj_set_width(label, txt_w);
    lv_obj_set_width(bubble, txt_w + 16);
    lv_obj_set_height(bubble, LV_SIZE_CONTENT);

    if (strcmp(role, "user") == 0) {
        lv_obj_set_style_bg_color(bubble, USER_BUBBLE, LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(bubble, LV_OPA_COVER, LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(label, lv_color_white(), LV_STATE_DEFAULT);
        lv_obj_align(bubble, LV_ALIGN_RIGHT_MID, -15, 0);
    } else if (strcmp(role, "assistant") == 0) {
        lv_obj_set_style_bg_color(bubble, ASSISTANT_BUBBLE, LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(bubble, LV_OPA_COVER, LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(label, lv_color_white(), LV_STATE_DEFAULT);
        lv_obj_align(bubble, LV_ALIGN_LEFT_MID, 15, 0);
    } else {
        lv_obj_set_style_bg_color(bubble, SYSTEM_BUBBLE, LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(bubble, LV_OPA_COVER, LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(label, SYSTEM_TEXT, LV_STATE_DEFAULT);
        lv_obj_align(bubble, LV_ALIGN_CENTER, 0, 0);
    }

    if (strcmp(role, "system") == 0) {
        lv_obj_set_user_data(bubble, (void*)"system");
    }

    return container;
}

/* ------------- Timer: ALL UI updates in LVGL task ----------- */
static void ui_timer_cb(lv_timer_t *timer)
{
    xz_ui_evt_t evt;
    while (xQueueReceive(s_ui_queue, &evt, 0) == pdTRUE) {
        switch (evt.type) {
        case XZ_EVT_MSG: {
            if (s_chat_content == NULL) break;

            if (strcmp(evt.role, "system") == 0 && s_msg_count > 0) {
                lv_obj_t *last = lv_obj_get_child(s_chat_content,
                    lv_obj_get_child_cnt(s_chat_content) - 1);
                if (last) {
                    lv_obj_t *bub = lv_obj_get_child(last, 0);
                    if (bub) {
                        void *tag = lv_obj_get_user_data(bub);
                        if (tag && strcmp((const char*)tag, "system") == 0) {
                            lv_obj_del(last);
                            s_msg_count--;
                        }
                    }
                }
            }

            if (s_msg_count >= MAX_CHAT_MSG) {
                lv_obj_t *first = lv_obj_get_child(s_chat_content, 0);
                if (first) { lv_obj_del(first); s_msg_count--; }
            }

            create_bubble(s_chat_content, evt.text, evt.role);
            s_msg_count++;

            lv_obj_t *last = lv_obj_get_child(s_chat_content,
                lv_obj_get_child_cnt(s_chat_content) - 1);
            if (last) lv_obj_scroll_to_view_recursive(last, LV_ANIM_ON);
            break;
        }
        case XZ_EVT_STATUS: {
            if (s_status_label) lv_label_set_text(s_status_label, evt.text);

            if (s_talk_label && s_talk_btn) {
                const char *st = evt.text;
                if (strcmp(st, "Idle") == 0 || strstr(st, "Disconnected")) {
                    lv_label_set_text(s_talk_label, "TALK");
                    lv_obj_set_style_bg_color(s_talk_btn, lv_color_hex(0x4CAF50), LV_STATE_DEFAULT);
                    s_talking = false;
                } else if (strcmp(st, "Connecting...") == 0 || strstr(st, "retry")) {
                    lv_label_set_text(s_talk_label, "...");
                    lv_obj_set_style_bg_color(s_talk_btn, lv_color_hex(0xFF9800), LV_STATE_DEFAULT);
                } else {
                    lv_label_set_text(s_talk_label, "STOP");
                    lv_obj_set_style_bg_color(s_talk_btn, lv_color_hex(0xF44336), LV_STATE_DEFAULT);
                    s_talking = true;
                }
            }
            break;
        }
        case XZ_EVT_WIFI: {
            if (s_wifi_label) lv_label_set_text(s_wifi_label, evt.text);
            break;
        }
        }
    }
}

/* ---- Thread-safe public API ---- */
void xiaozhi_add_message(const char *role, const char *text)
{
    if (s_ui_queue == NULL) return;
    xz_ui_evt_t evt;
    memset(&evt, 0, sizeof(evt));
    evt.type = XZ_EVT_MSG;
    strncpy(evt.role, role, sizeof(evt.role) - 1);
    strncpy(evt.text, text, sizeof(evt.text) - 1);
    xQueueSend(s_ui_queue, &evt, 0);
}

void xiaozhi_set_status(const char *status)
{
    if (s_ui_queue == NULL) return;
    xz_ui_evt_t evt;
    memset(&evt, 0, sizeof(evt));
    evt.type = XZ_EVT_STATUS;
    strncpy(evt.text, status, sizeof(evt.text) - 1);
    xQueueSend(s_ui_queue, &evt, 0);
}

void xiaozhi_set_wifi(const char *ssid)
{
    if (s_ui_queue == NULL) return;
    xz_ui_evt_t evt;
    memset(&evt, 0, sizeof(evt));
    evt.type = XZ_EVT_WIFI;
    strncpy(evt.text, ssid ? ssid : "", sizeof(evt.text) - 1);
    xQueueSend(s_ui_queue, &evt, 0);
}

/* ---- Bridge callbacks (called from bridge tasks - thread safe) ---- */
static void on_bridge_message(const char *role, const char *text)
{
    xiaozhi_add_message(role, text);
}

static void on_bridge_status(const char *status)
{
    xiaozhi_set_status(status);
}

static void on_bridge_wifi(const char *ssid)
{
    xiaozhi_set_wifi(ssid);
}

/* ---- TALK button: deferred to background task ---- */
static void toggle_task_cb(void *arg)
{
    xiaozhi_adapter_toggle_chat();
    vTaskDelete(NULL);
}

static void talk_btn_cb(lv_event_t *e)
{
    /* Run Toggle() in a background task so LVGL task doesn't block */
    xTaskCreate(toggle_task_cb, "xz_toggle", 12288, NULL, 1, NULL);
}

/* ---- Initialize chat UI ---- */
void app_xiaozhi_init(lv_obj_t *parent)
{
    lv_coord_t sw = lv_obj_get_width(parent);
    lv_coord_t sh = lv_obj_get_height(parent);

    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(parent, BG_COLOR, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(parent, 0, LV_STATE_DEFAULT);

    /* Status bar */
    lv_obj_t *bar = lv_obj_create(parent);
    lv_obj_set_size(bar, sw, 28);
    lv_obj_set_pos(bar, 0, 0);
    lv_obj_set_style_radius(bar, 0, 0);
    lv_obj_set_style_bg_color(bar, BG_COLOR, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_pad_all(bar, 0, 0);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_left(bar, 12, 0);
    lv_obj_set_style_pad_right(bar, 12, 0);

    /* WiFi SSID (left) */
    s_wifi_label = lv_label_create(bar);
    lv_label_set_text(s_wifi_label, "");
    lv_obj_set_style_text_font(s_wifi_label, &lv_font_montserrat_14, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(s_wifi_label, lv_color_hex(0x888888), LV_STATE_DEFAULT);

    /* Status (center) */
    s_status_label = lv_label_create(bar);
    lv_label_set_text(s_status_label, "Idle");
    lv_obj_set_style_text_font(s_status_label, &lv_font_montserrat_14, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(s_status_label, lv_color_hex(0xAAAAAA), LV_STATE_DEFAULT);
    lv_label_set_long_mode(s_status_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_flex_grow(s_status_label, 1);
    lv_obj_set_style_text_align(s_status_label, LV_TEXT_ALIGN_CENTER, 0);

    /* Spacer (right) */
    lv_obj_t *spacer = lv_label_create(bar);
    lv_label_set_text(spacer, "");

    /* Chat area */
    s_chat_content = lv_obj_create(parent);
    lv_obj_set_size(s_chat_content, sw - 10, sh - 128);
    lv_obj_set_pos(s_chat_content, 5, 32);
    lv_obj_set_style_bg_opa(s_chat_content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_chat_content, 0, 0);
    lv_obj_set_style_radius(s_chat_content, 0, 0);
    lv_obj_set_flex_flow(s_chat_content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scrollbar_mode(s_chat_content, LV_SCROLLBAR_MODE_OFF);

    /* TALK button */
    s_talk_btn = lv_btn_create(parent);
    lv_obj_set_size(s_talk_btn, 140, 50);
    lv_obj_align(s_talk_btn, LV_ALIGN_BOTTOM_MID, 0, -25);
    lv_obj_set_style_radius(s_talk_btn, 25, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(s_talk_btn, lv_color_hex(0x4CAF50), LV_STATE_DEFAULT);
    lv_obj_add_event_cb(s_talk_btn, talk_btn_cb, LV_EVENT_CLICKED, NULL);

    s_talk_label = lv_label_create(s_talk_btn);
    lv_label_set_text(s_talk_label, "TALK");
    lv_obj_set_style_text_font(s_talk_label, &lv_font_montserrat_20, LV_STATE_DEFAULT);
    lv_obj_center(s_talk_label);

    /* UI event queue */
    s_ui_queue = xQueueCreate(20, sizeof(xz_ui_evt_t));
    lv_timer_create(ui_timer_cb, 150, NULL);

    /* Register bridge callbacks */
    xiaozhi_adapter_set_message_callback(on_bridge_message);
    xiaozhi_adapter_set_status_callback(on_bridge_status);
    xiaozhi_adapter_set_wifi_callback(on_bridge_wifi);

    xiaozhi_add_message("system", "Welcome! Press TALK to connect and start AI chat.");
}
