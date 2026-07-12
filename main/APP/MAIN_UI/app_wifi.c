/**
 ******************************************************************************
 * @file        app_wifi.c
 * @version     V1.0
 * @brief       Wifi APP
 ******************************************************************************
 * @attention   Waiken-Smart 慧勤智远
 * 
 * 实验平台:     慧勤智远 ESP32-P4 开发板
 ******************************************************************************
 */

#include "app_wifi.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "freertos/event_groups.h"

LV_IMG_DECLARE(wifi0)
LV_IMG_DECLARE(wifi1)
LV_IMG_DECLARE(wifi2)
LV_IMG_DECLARE(wifi3)

static const char *WIFI_TAG = "WIFI_APP";
wifi_ui_t wifi_ui;

/**************************** 全局变量定义 ******************************/
static volatile bool wifi_ui_active = false;
static bool wifi_initialized = false;
static bool event_handlers_registered = false;
static lv_obj_t *status_label = NULL;
static TimerHandle_t status_timer = NULL;
static bool status_shown = false;

/***************************** 样式定义 *******************************/
static lv_style_t box_style;
static lv_style_t btn_style;
static lv_style_t list_style;
static lv_style_t title_style;
static lv_style_t list_item_style;

/**-************************* WiFi配置参数 ******************************/
#define WIFI_SCAN_DONE_BIT BIT0
EventGroupHandle_t wifi_event_group;

/**************************** 函数声明 *****************************/
static void ap_free_event(lv_event_t *e);
static void async_update_status_label(const char *text);
static void clear_status_label(TimerHandle_t xTimer);
static void wifi_scan_handler(void);
static void style_init(void);
static void scan_btn_event(lv_event_t *e);
static const void* get_wifi_icon(int rssi);                     
static void async_update_status_label_cb(void *arg);
static void back_button_event(lv_event_t *e);
void wifi_module_init(void);

/**
 * @brief       AP信息释放事件
 */
static void ap_free_event(lv_event_t *e) 
{
    void *data = lv_event_get_user_data(e);
    if (data) 
    {
        free(data);
        ESP_LOGI(WIFI_TAG, "Freed AP record memory");
    }
}

/**
 * @brief       WiFi扫描处理
 */
static void wifi_scan_handler(void) 
{
    uint16_t ap_count = 0;
    uint16_t number = 20; // 最大AP数量
    wifi_ap_record_t *ap_list = NULL;
    
    esp_wifi_scan_get_ap_num(&ap_count);
    ESP_LOGI(WIFI_TAG, "Found %d APs", ap_count);
    
    if (ap_count == 0) 
    {
        ESP_LOGW(WIFI_TAG, "No AP found");
        async_update_status_label("No networks found");
        return;
    }
    
    // 限制最大数量
    number = (ap_count > number) ? number : ap_count;
    
    ap_list = malloc(sizeof(wifi_ap_record_t) * number);
    if (!ap_list) 
    {
        ESP_LOGE(WIFI_TAG, "AP list malloc failed");
        return;
    }
    
    esp_err_t ret = esp_wifi_scan_get_ap_records(&number, ap_list);
    if (ret != ESP_OK) 
    {
        ESP_LOGE(WIFI_TAG, "Get AP records failed: %s", esp_err_to_name(ret));
        free(ap_list);
        return;
    }
    
    // 清空列表
    lv_obj_clean(wifi_ui.list);
    
    // 按信号强度排序（简单的冒泡排序）
    for (int i = 0; i < number - 1; i++) {
        for (int j = i + 1; j < number; j++) {
            if (ap_list[j].rssi > ap_list[i].rssi) {
                wifi_ap_record_t temp = ap_list[i];
                ap_list[i] = ap_list[j];
                ap_list[j] = temp;
            }
        }
    }
    
    for (int i = 0; i < number; i++) {
        // 跳过隐藏的SSID
        if (strlen((const char*)ap_list[i].ssid) == 0) {
            continue;
        }
        
        wifi_ap_record_t *ap_copy = malloc(sizeof(wifi_ap_record_t));
        if (!ap_copy) {
            ESP_LOGE(WIFI_TAG, "Failed to allocate AP copy");
            continue;
        }
        memcpy(ap_copy, &ap_list[i], sizeof(wifi_ap_record_t));
        
        // 创建列表项容器
        lv_obj_t *container = lv_obj_create(wifi_ui.list);
        lv_obj_set_size(container, LV_PCT(100), 80);
        lv_obj_add_style(container, &list_item_style, 0);
        lv_obj_set_style_pad_all(container, 15, 0);

        // 添加信号强度图标
        lv_obj_t *icon = lv_img_create(container);
        lv_img_set_src(icon, get_wifi_icon(ap_copy->rssi));
        lv_obj_set_size(icon, 36, 36);
        lv_obj_set_align(icon, LV_ALIGN_LEFT_MID);

        // 添加SSID标签
        lv_obj_t *label = lv_label_create(container);
        lv_label_set_text(label, (const char *)ap_copy->ssid);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(label, lv_color_hex(0x333333), 0);
        lv_obj_set_style_pad_left(label, 20, 0);
        lv_obj_align_to(label, icon, LV_ALIGN_OUT_RIGHT_MID, 20, 0);

        // 添加信号强度显示
        lv_obj_t *rssi_label = lv_label_create(container);
        char rssi_text[20];
        snprintf(rssi_text, sizeof(rssi_text), "%ddBm", ap_copy->rssi);
        lv_label_set_text(rssi_label, rssi_text);
        lv_obj_set_style_text_font(rssi_label, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(rssi_label, lv_color_hex(0x666666), 0);
        lv_obj_align(rssi_label, LV_ALIGN_RIGHT_MID, -20, 0);

        // 添加认证方式显示
        lv_obj_t *auth_label = lv_label_create(container);
        const char *auth_text = "Open";
        switch (ap_copy->authmode) {
            case WIFI_AUTH_OPEN:
                auth_text = "Open";
                break;
            case WIFI_AUTH_WEP:
                auth_text = "WEP";
                break;
            case WIFI_AUTH_WPA_PSK:
                auth_text = "WPA-PSK";
                break;
            case WIFI_AUTH_WPA2_PSK:
                auth_text = "WPA2-PSK";
                break;
            case WIFI_AUTH_WPA_WPA2_PSK:
                auth_text = "WPA/WPA2";
                break;
            case WIFI_AUTH_WPA3_PSK:
                auth_text = "WPA3";
                break;
            default:
                auth_text = "Unknown";
                break;
        }
        lv_label_set_text(auth_label, auth_text);
        lv_obj_set_style_text_font(auth_label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(auth_label, lv_color_hex(0x888888), 0);
        lv_obj_align(auth_label, LV_ALIGN_BOTTOM_RIGHT, -20, 0);

        // 添加删除事件
        lv_obj_add_event_cb(container, ap_free_event, LV_EVENT_DELETE, ap_copy);
    }
    
    free(ap_list);
    async_update_status_label("");
}

/**
 * @brief   扫描按钮事件处理函数
 */
static void scan_btn_event(lv_event_t *e)
{
    if (!wifi_initialized) {
        wifi_module_init();
        esp_wifi_set_mode(WIFI_MODE_STA);
        esp_wifi_start();
        wifi_initialized = true;
    }

    // async_update_status_label("Scanning WiFi...");
    
    // 清除之前的扫描结果
    lv_obj_clean(wifi_ui.list);
    
    esp_err_t ret;
    int retry_count = 0;
    
    while (retry_count < 3) {
        ESP_LOGI(WIFI_TAG, "Starting WiFi scan, attempt %d/3", retry_count + 1);
        
        ret = esp_wifi_scan_start(NULL, true);
        if (ret != ESP_OK) {
            ESP_LOGE(WIFI_TAG, "Scan start failed: %s", esp_err_to_name(ret));
            retry_count++;
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        // 等待扫描完成
        EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
                                              WIFI_SCAN_DONE_BIT,
                                              pdFALSE,
                                              pdFALSE,
                                              pdMS_TO_TICKS(10000));
        
        if (bits & WIFI_SCAN_DONE_BIT) {
            ESP_LOGI(WIFI_TAG, "WiFi scan completed successfully");
            wifi_scan_handler();
            return;
        } else {
            ESP_LOGW(WIFI_TAG, "Scan timeout, retrying...");
            retry_count++;
        }
    }

    if (retry_count >= 3) {
        ESP_LOGE(WIFI_TAG, "WiFi scan failed after 3 attempts");
        async_update_status_label("Scan failed!");
    }
}

/**
 * @brief       获取信号强度对应图标
 */
static const void* get_wifi_icon(int rssi)
{
    if (rssi >= -50) return &wifi3;
    else if (rssi >= -60) return &wifi3;
    else if (rssi >= -70) return &wifi2;
    else if (rssi >= -80) return &wifi1;
    else return &wifi0;
} 

/**
 * @brief       异步更新状态标签
 */
static void async_update_status_label(const char *text) 
{
    if (text) {
        lv_async_call(async_update_status_label_cb, strdup(text));
    } else {
        lv_async_call(async_update_status_label_cb, strdup(""));
    }
}

/**
 * @brief       异步更新状态标签的回调函数
 */
static void async_update_status_label_cb(void *arg) 
{
    const char *text = (const char *)arg;
    if (status_label && lv_obj_is_valid(status_label)) 
    {
        lv_label_set_text(status_label, text);
    }
    free(arg);
}

/**
 * @brief       清除状态标签的回调函数
 */
static void clear_status_label(TimerHandle_t xTimer) 
{
    if (status_label && lv_obj_is_valid(status_label)) 
    {
        lv_label_set_text(status_label, "");
    }
    status_shown = false;
}

/**
 * @brief       WiFi事件处理
 */
static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) 
{
    if(!wifi_ui_active) return;

    if (event_base == WIFI_EVENT) 
    {
        switch (event_id) {
            case WIFI_EVENT_SCAN_DONE:
            {
                ESP_LOGI(WIFI_TAG, "WiFi scan done event received");
                xEventGroupSetBits(wifi_event_group, WIFI_SCAN_DONE_BIT);
                break;
            }
            case WIFI_EVENT_STA_START:
            {
                ESP_LOGI(WIFI_TAG, "WiFi station started");
                break;
            }
            case WIFI_EVENT_STA_STOP:
            {
                ESP_LOGI(WIFI_TAG, "WiFi station stopped");
                break;
            }
        }
    } 
}

/**
 * @brief       初始化WiFi模块
 */
void wifi_module_init(void) {
    if (!wifi_initialized) {
        esp_netif_init();  
        esp_event_loop_create_default();
        esp_netif_create_default_wifi_sta();

        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        esp_wifi_init(&cfg);
        esp_wifi_set_storage(WIFI_STORAGE_RAM);

        wifi_initialized = true;
        ESP_LOGI(WIFI_TAG, "WiFi module initialized");
    }

    if (!event_handlers_registered) {
        esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, NULL);
        event_handlers_registered = true;
        ESP_LOGI(WIFI_TAG, "WiFi event handlers registered");
    }

    if (wifi_event_group == NULL) {
        wifi_event_group = xEventGroupCreate();
    }
}

/**
 * @brief       初始化样式
 */
static void style_init(void) 
{
    /* 主容器样式 */
    lv_style_init(&box_style);
    lv_style_set_radius(&box_style, 20);
    lv_style_set_bg_color(&box_style, lv_color_hex(0xFFFFFF));
    lv_style_set_pad_all(&box_style, 15);
    lv_style_set_border_width(&box_style, 2);
    lv_style_set_border_color(&box_style, lv_color_hex(0xE0E0E0));

    /* 按钮样式 */
    lv_style_init(&btn_style);
    lv_style_set_radius(&btn_style, 15);
    lv_style_set_bg_color(&btn_style, lv_color_hex(0x007AFF));
    lv_style_set_text_color(&btn_style, lv_color_white());
    lv_style_set_pad_hor(&btn_style, 30);
    lv_style_set_pad_ver(&btn_style, 15);

    /* 列表样式 */
    lv_style_init(&list_style);
    lv_style_set_radius(&list_style, 20);
    lv_style_set_bg_color(&list_style, lv_color_hex(0xF8F8F8));
    lv_style_set_bg_opa(&list_style, LV_OPA_100);

    /* 列表项样式 */
    lv_style_init(&list_item_style);
    lv_style_set_radius(&list_item_style, 15);
    lv_style_set_bg_color(&list_item_style, lv_color_hex(0xFFFFFF));
    lv_style_set_border_width(&list_item_style, 1);
    lv_style_set_border_color(&list_item_style, lv_color_hex(0xE8E8E8));
    lv_style_set_pad_all(&list_item_style, 15);

    /* 标题样式 */
    lv_style_init(&title_style);
    lv_style_set_text_font(&title_style, &lv_font_montserrat_28);
    lv_style_set_text_color(&title_style, lv_color_hex(0x1C1C1E));
}

/**
 * @brief   返回按钮事件处理函数
 */
static void back_button_event(lv_event_t *e) 
{
    wifi_app_del();
}

/**
 * @brief   初始化WiFi应用界面
 */
void wifi_app_init(void) 
{
    style_init();

    wifi_ui_active = true;
    
    if(wifi_ui.wifi_main_ui && lv_obj_is_valid(wifi_ui.wifi_main_ui)) 
    {
        wifi_app_del();
    }

    /* 创建主容器 */
    wifi_ui.wifi_main_ui = lv_obj_create(lv_scr_act());
    lv_obj_set_style_bg_color(wifi_ui.wifi_main_ui, lv_color_hex(0x000000), LV_STATE_DEFAULT);
    lv_obj_set_size(wifi_ui.wifi_main_ui, lv_obj_get_width(lv_scr_act()), lv_obj_get_height(lv_scr_act()));
    lv_obj_set_style_radius(wifi_ui.wifi_main_ui, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(wifi_ui.wifi_main_ui, LV_OPA_0, LV_STATE_DEFAULT);
    lv_obj_set_pos(wifi_ui.wifi_main_ui, 0, 0);
    lv_obj_clear_flag(wifi_ui.wifi_main_ui, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_update_layout(wifi_ui.wifi_main_ui);

    /* 创建标题容器 */
    lv_obj_t *title_container = lv_obj_create(wifi_ui.wifi_main_ui);
    lv_obj_remove_style_all(title_container);
    lv_obj_set_size(title_container, 300, 50);
    lv_obj_align(title_container, LV_ALIGN_TOP_MID, -150, 30);
    lv_obj_set_style_bg_opa(title_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(title_container, 0, 0);
    lv_obj_set_flex_flow(title_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(title_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    /* 添加WIFI图标 */
    // lv_obj_t *wifi_icon = lv_img_create(title_container);
    // lv_img_set_src(wifi_icon, &wifi3);
    // lv_obj_set_size(wifi_icon, 36, 36);

    /* 添加标题文本 */
    lv_obj_t *title = lv_label_create(title_container);
    lv_label_set_text(title, "WLAN");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_pad_left(title, 15, 0);

    /* 创建扫描按钮 */
    lv_obj_t *scan_btn = lv_btn_create(wifi_ui.wifi_main_ui);
    lv_obj_set_size(scan_btn, 120, 50);
    lv_obj_set_style_bg_color(scan_btn, lv_color_hex(0x007AFF), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(scan_btn, lv_color_hex(0x0056CC), LV_STATE_PRESSED);
    lv_obj_set_style_radius(scan_btn, 12, 0);
    lv_obj_add_event_cb(scan_btn, scan_btn_event, LV_EVENT_RELEASED, NULL);
    lv_obj_align_to(scan_btn, title_container, LV_ALIGN_OUT_RIGHT_MID, 20, 0);

    lv_obj_t *btn_label = lv_label_create(scan_btn);
    lv_label_set_text(btn_label, "Scan");
    lv_obj_set_style_text_font(btn_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(btn_label, lv_color_white(), 0);
    lv_obj_center(btn_label);    

    /* 创建状态标签 */
    status_label = lv_label_create(wifi_ui.wifi_main_ui);
    lv_label_set_text(status_label, "");
    lv_obj_add_style(status_label, &title_style, 0);
    lv_obj_set_style_text_color(status_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(status_label, LV_ALIGN_TOP_MID, 0, 100);

    /* 创建AP列表 */
    wifi_ui.list = lv_list_create(wifi_ui.wifi_main_ui);
    lv_obj_set_size(wifi_ui.list, lv_obj_get_width(wifi_ui.wifi_main_ui) - 40, lv_obj_get_height(wifi_ui.wifi_main_ui) - 200);
    lv_obj_align(wifi_ui.list, LV_ALIGN_TOP_MID, 0, 150);
    lv_obj_add_style(wifi_ui.list, &list_style, 0);
    lv_obj_set_style_border_width(wifi_ui.list, 2, LV_STATE_DEFAULT);
    
    /* 创建返回按钮 */
    wifi_ui.back_btn = lv_btn_create(wifi_ui.wifi_main_ui);
    lv_obj_set_size(wifi_ui.back_btn, 120, 50);
    lv_obj_set_style_bg_color(wifi_ui.back_btn, lv_color_hex(0x8E8E93), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(wifi_ui.back_btn, lv_color_hex(0x6D6D70), LV_STATE_PRESSED);
    lv_obj_set_style_radius(wifi_ui.back_btn, 12, 0);
    lv_obj_align(wifi_ui.back_btn, LV_ALIGN_TOP_LEFT, 20, 30);
    lv_obj_add_event_cb(wifi_ui.back_btn, back_button_event, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *back_label = lv_label_create(wifi_ui.back_btn);
    lv_label_set_text(back_label, "Menu");
    lv_obj_set_style_text_font(back_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(back_label, lv_color_white(), 0);
    lv_obj_center(back_label);

	wifi_module_init();
	esp_wifi_set_mode(WIFI_MODE_STA);
	esp_wifi_start();
	wifi_initialized = true;
    
    lv_general.current_parent = wifi_ui.wifi_main_ui;
}

/**
 * @brief   清理WiFi应用资源
 */
void wifi_app_del(void) 
{
    wifi_ui_active = false;

    if (wifi_ui.wifi_main_ui && lv_obj_is_valid(wifi_ui.wifi_main_ui)) 
    {
        lv_obj_del(wifi_ui.wifi_main_ui);
        wifi_ui.wifi_main_ui = NULL;
    }

    esp_wifi_stop();

    async_update_status_label("");

    if (status_timer != NULL) 
    {
        xTimerDelete(status_timer, portMAX_DELAY);
        status_timer = NULL;
    }
    
    ESP_LOGI(WIFI_TAG, "WiFi app deleted successfully");
}