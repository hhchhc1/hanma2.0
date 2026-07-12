#include "wifi_board.h"
#include "codecs/es8311_audio_codec.h"
#include "application.h"
#include "display/lcd_display.h"
#include "esp_lcd_ili9881c.h"
#include "button.h"
#include "config.h"
#include <wifi_station.h>
#include <esp_log.h>
#include <driver/i2c_master.h>

#include "esp_lcd_panel_ops.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_ldo_regulator.h"
#include <esp_lcd_panel_vendor.h>

#include <esp_lvgl_port.h>
#include "esp_lcd_touch_gt911.h"
#include "camera/camera_display.h"
#include "input/ec11.h"
#include "p4_camera.h"
#include "config.h"
#include <esp_rom_sys.h>

#define TAG "WKS_ESP32P4CB"

LV_FONT_DECLARE(font_puhui_30_4);
LV_FONT_DECLARE(font_awesome_30_4);

class WKS_ESP32P4CB : public WifiBoard {
private:
    i2c_master_bus_handle_t codec_i2c_bus_;
    i2c_master_bus_handle_t touch_i2c_bus_;
    LcdDisplay* display_;
    Button boot_button_;

    void InitializeCodecI2c() {
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port         = I2C_NUM_1,
            .sda_io_num       = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num       = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source       = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt= 7,
            .intr_priority    = 0,
            .trans_queue_depth= 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &codec_i2c_bus_));
    }

    void InitializeMipiDisplay() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t   panel     = nullptr;

        /* ── LDO for MIPI DPHY ─────────────────────────────────────────── */
        ESP_LOGI(TAG, "Enable MIPI DSI PHY power (LDO ch%d, %dmV)",
                 MIPI_DSI_PHY_PWR_LDO_CHAN, MIPI_DSI_PHY_PWR_LDO_VOLTAGE_MV);
        static esp_ldo_channel_handle_t phy_pwr_chan = NULL;
        esp_ldo_channel_config_t ldo_cfg = {
            .chan_id    = MIPI_DSI_PHY_PWR_LDO_CHAN,
            .voltage_mv = MIPI_DSI_PHY_PWR_LDO_VOLTAGE_MV,
        };
        ESP_ERROR_CHECK(esp_ldo_acquire_channel(&ldo_cfg, &phy_pwr_chan));

        /* ── DSI bus ───────────────────────────────────────────────────── */
        ESP_LOGI(TAG, "Create MIPI DSI bus");
        esp_lcd_dsi_bus_handle_t mipi_dsi_bus = NULL;
        esp_lcd_dsi_bus_config_t bus_config = {
            .bus_id             = 0,
            .num_data_lanes     = LCD_MIPI_DSI_LANE_NUM,
            .phy_clk_src        = MIPI_DSI_PHY_CLK_SRC_DEFAULT,
            .lane_bit_rate_mbps = MIPI_DSI_LANE_BITRATE_MBPS,
        };
        ESP_ERROR_CHECK(esp_lcd_new_dsi_bus(&bus_config, &mipi_dsi_bus));

        /* ── Panel IO (DBI) ────────────────────────────────────────────── */
        ESP_LOGI(TAG, "Create panel IO");
        esp_lcd_dbi_io_config_t dbi_config = {
            .virtual_channel = 0,
            .lcd_cmd_bits    = 8,
            .lcd_param_bits  = 8,
        };
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_dbi(mipi_dsi_bus, &dbi_config, &panel_io));

        /* DPI 时序 */
        esp_lcd_dpi_panel_config_t dpi_config = {
            .virtual_channel    = 0,
            .dpi_clk_src        = MIPI_DSI_DPI_CLK_SRC_DEFAULT,
            .dpi_clock_freq_mhz = LCD_DPI_CLK_MHZ,
            .pixel_format       = LCD_COLOR_PIXEL_FORMAT_RGB565,
            .num_fbs            = 1,
            .video_timing = {
                .h_size            = LCD_H_SIZE,
                .v_size            = LCD_V_SIZE,
                .hsync_pulse_width = LCD_HSYNC_PULSE_WIDTH,
                .hsync_back_porch  = LCD_HSYNC_BACK_PORCH,
                .hsync_front_porch = LCD_HSYNC_FRONT_PORCH,
                .vsync_pulse_width = LCD_VSYNC_PULSE_WIDTH,
                .vsync_back_porch  = LCD_VSYNC_BACK_PORCH,
                .vsync_front_porch = LCD_VSYNC_FRONT_PORCH,
            },
            .flags = {
                .use_dma2d = false,
            },
        };

        /* ── 面板驱动（四款屏幕统一使用 ILI9881C 通用 MIPI 驱动）───── */
#if MIPI_LCD_SCREEN == 5
        ESP_LOGI(TAG, "Install LCD driver: ILI9881D (5\" 720x1280)");
#elif MIPI_LCD_SCREEN == 7
        ESP_LOGI(TAG, "Install LCD driver: EK79007 via ili9881c path (7\" 1024x600)");
#elif MIPI_LCD_SCREEN == 8
        ESP_LOGI(TAG, "Install LCD driver: ILI9881C (8\" 800x1280)");
#elif MIPI_LCD_SCREEN == 101
        ESP_LOGI(TAG, "Install LCD driver: ILI9881C (10.1\" 800x1280)");
#endif

        ili9881c_vendor_config_t vendor_config = {
            .init_cmds      = lcd_init_cmds,
            .init_cmds_size = LCD_INIT_CMDS_SIZE,
            .mipi_config = {
                .dsi_bus    = mipi_dsi_bus,
                .dpi_config = &dpi_config,
                .lane_num   = LCD_MIPI_DSI_LANE_NUM,
            },
        };

        esp_lcd_panel_dev_config_t lcd_dev_config = {};
        lcd_dev_config.rgb_ele_order  = LCD_RGB_ELEMENT_ORDER_RGB;
        lcd_dev_config.reset_gpio_num = PIN_NUM_LCD_RST;
        lcd_dev_config.bits_per_pixel = LCD_BIT_PER_PIXEL;
        lcd_dev_config.vendor_config  = &vendor_config;

        ESP_ERROR_CHECK(esp_lcd_new_panel_ili9881c(panel_io, &lcd_dev_config, &panel));

        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));

        lv_init();

        display_ = new MipiLcdDisplay(panel_io, panel,
                                      DISPLAY_WIDTH, DISPLAY_HEIGHT,
                                      DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y,
                                      DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y,
                                      DISPLAY_SWAP_XY,
                                      {
                                          .text_font  = &font_puhui_30_4,
                                          .icon_font  = &font_awesome_30_4,
                                          .emoji_font = font_emoji_64_init(),
                                      });
    }

    void InitializeTouchI2c() {
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port         = TP_I2C_NUM,
            .sda_io_num       = TP_I2C_SDA_PIN,
            .scl_io_num       = TP_I2C_SCL_PIN,
            .clk_source       = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt= 7,
            .intr_priority    = 0,
            .trans_queue_depth= 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &touch_i2c_bus_));
    }

    void InitializeTouch() {
        esp_lcd_touch_handle_t tp;
        esp_lcd_touch_config_t tp_cfg = {
            .x_max = DISPLAY_WIDTH,
            .y_max = DISPLAY_HEIGHT,
            .rst_gpio_num = TP_PIN_NUM_RST,
            .int_gpio_num = TP_PIN_NUM_INT,
            .levels = {
                .reset = 0,
                .interrupt = 0,
            },
            .flags = {
                .swap_xy = 0,
                .mirror_x = 0,
                .mirror_y = 0,
            },
        };
        esp_lcd_panel_io_handle_t tp_io_handle = NULL;
        esp_lcd_panel_io_i2c_config_t tp_io_config = {
            .dev_addr = ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS,
            .control_phase_bytes = 1,
            .lcd_cmd_bits = 16,
            .flags = {
                .disable_control_phase = 1,
            },
            .scl_speed_hz = 400 * 1000,
        };
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(touch_i2c_bus_, &tp_io_config, &tp_io_handle));
        ESP_LOGI(TAG, "Initialize touch controller");
        ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_gt911(tp_io_handle, &tp_cfg, &tp));
        const lvgl_port_touch_cfg_t touch_cfg = {
            .disp = lv_disp_get_default(),
            .handle = tp,
        };
        lvgl_port_add_touch(&touch_cfg);
        ESP_LOGI(TAG, "Touch panel initialized successfully");
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting &&
                !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();
            }
            app.ToggleChatState();
        });
    }

    void InitializeEc11() {
        auto& ec11 = Ec11::GetInstance();
        ec11.Init(EC11_S1_GPIO, EC11_S2_GPIO, EC11_KEY_GPIO);

        ec11.OnRotate([this](int direction) {
            Application::GetInstance().Schedule([this, direction]() {
                if (display_ != nullptr) {
                    display_->HandleEc11Rotate(direction);
                }
            });
        });

        ec11.OnKeyEvent([this](bool is_press) {
            Application::GetInstance().Schedule([this, is_press]() {
                if (display_ != nullptr) {
                    display_->HandleEc11KeyEvent(is_press);
                }
            });
        });
    }

public:
    WKS_ESP32P4CB() : boot_button_(BOOT_BUTTON_GPIO) {
        gpio_config_t io_conf = {
            .pin_bit_mask = (1ULL << GPIO_NUM_12),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_ENABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        gpio_config(&io_conf);
		gpio_set_level(GPIO_NUM_12, 1);

		// Camera power-up: PWDN(GPIO27)=LOW, RST(GPIO26)=HIGH
		{
		    gpio_config_t cam_pwr = {
		        .pin_bit_mask = (1ULL << GPIO_NUM_26) | (1ULL << GPIO_NUM_27),
		        .mode = GPIO_MODE_OUTPUT,
		        .pull_up_en = GPIO_PULLUP_DISABLE,
		        .pull_down_en = GPIO_PULLDOWN_DISABLE,
		        .intr_type = GPIO_INTR_DISABLE,
		    };
		    gpio_config(&cam_pwr);
		    gpio_set_level(GPIO_NUM_27, 0);
		    gpio_set_level(GPIO_NUM_26, 1);
		    esp_rom_delay_us(1000);
		}

		InitializeCodecI2c();
		camera_display_set_i2c_bus(codec_i2c_bus_);
		InitializeTouchI2c();
		InitializeMipiDisplay();
		InitializeTouch();
		InitializeEc11();
		InitializeButtons();
		GetBacklight()->SetBrightness(100);
		GetCamera(); // init camera singleton + start motion detection task
    }

    virtual AudioCodec* GetAudioCodec() override {
        static Es8311AudioCodec audio_codec(
            codec_i2c_bus_, I2C_NUM_1,
            AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK,
            AUDIO_I2S_GPIO_WS,   AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN,  AUDIO_CODEC_ES8311_ADDR);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }

    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, false);
        return &backlight;
    }

    virtual Camera* GetCamera() override {
        static P4Camera camera;
        return &camera;
    }
};

DECLARE_BOARD(WKS_ESP32P4CB);