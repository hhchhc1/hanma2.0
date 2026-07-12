/**
 ******************************************************************************
 * @file        main.c
 * @version     V1.0
 * @brief       LVGL综合实验
 ******************************************************************************
 * @attention   Waiken-Smart 慧勤智远
 * 
 * 实验平台:     慧勤智远 ESP32-P4 开发板
 ******************************************************************************
 */

#include "led.h"
#include "lcd.h"
#include "myiic.h"
#include "ledc.h"
#include "myes8311.h"
#include "esp_rtc.h"
#include "lvgl_demo.h"
#include "sdmmc.h"
#include <stdio.h>
#include "key.h"
#include "spilcd.h"
#include "driver/usb_serial_jtag.h"
#include "xiaozhi_adapter.h"
#include "nvs_flash.h"


const char* main_twai_tag = "main_twai";

/**
 * @brief       程序入口
 * @param       无
 * @retval      无
 */
void app_main(void)
{
    uint8_t x = 0;
	uint8_t key = 10;
	uint8_t lcd_id[12];                     /* 存放LCD ID字符串 */
	uint8_t *tx_buf = malloc(8);
    uint8_t *rx_buf = malloc(8);

	usb_serial_jtag_driver_config_t usb_serial_jtag_config = {
										.rx_buffer_size = 1024,
										.tx_buffer_size = 1024,
									};

    led_init();     /* LED初始化 */
    key_init();     /* 按键初始化 */

    key = key_scan(0);

	if(key != KEY0_PRES && key != BOOT_PRES)        /* MIPI屏幕*/
	{
		/*********************************** USB JTAG Test ************************************/
		ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&usb_serial_jtag_config));

		if (usb_serial_jtag_is_connected())
		{
			lv_general.usb_jtag_check_en = 1;
		}
		else
		{
			lv_general.usb_jtag_check_en = 0;
		}

		/************************************** SDIO PHY Power ***************************************/
		sd_dev_bsp_enable_phy_power();  /* SDIO PHY供电 (ESP-Hosted WiFi需要) */

        /************************************** Twai Test ***************************************/
		for (uint8_t i = 0; i < 8; i++)
		{
			tx_buf[i] = i;
		}

		twai_init(TWAI_MODE_NO_ACK);                           /* TWAI初始化 */
		ESP_ERROR_CHECK(twai_send_data(MSG_ID, tx_buf, 8));    /* ID = 0x12, 发送8个字节 */
        vTaskDelay(pdMS_TO_TICKS(20));
		twai_receive_data(MSG_ID, rx_buf);                     /* CAN ID = 0x12, 接收数据查询 */

        if (memcmp(tx_buf, rx_buf, 8) == 0)
        {
			for (uint8_t i = 0; i < 3; i++)
			{
				ESP_LOGI(main_twai_tag, "twai ok");
			}
        }
		else
		{
			for (uint8_t i = 0; i < 3; i++)
			{
				ESP_LOGI(main_twai_tag, "twai error");
			}
		}

		free(tx_buf);
        free(rx_buf);

		lcd_init();                                                      /* MIPI LCD初始化 */

		if (myi2s_init() != ESP_OK)                                      /* i2s初始化 */
		{
			lcd_show_string(30, 110, 200, 16, 16, "I2S Error", RED);
			while(1) { vTaskDelay(pdMS_TO_TICKS(1000)); }
		}
		
		while (myes8311_init())                                          /* ES8311初始化 */
		{
			lcd_show_string(30, 110, 200, 16, 16, "ES8311 Error", RED);
			vTaskDelay(pdMS_TO_TICKS(200));
			lcd_fill(30, 110, 239, 126, WHITE);
			vTaskDelay(pdMS_TO_TICKS(200));
		}

		gpio_set_level(GPIO_NUM_11, 1);   /* 打开喇叭 */
			/* ---- 初始化 xiaozhi AI 语音助手 ---- */
			xiaozhi_adapter_init("", "");  /* 首次 TALK 时自动 OTA 发现服务器 */
			/* -------------------------------------------------- */

		if (mipidev.id == 0x79007)        /* 7寸 1024*600 MIPI屏幕 */
		{
			rtc_set_time(2026, 1, 1, 8, 8, 00);   /* 设置RTC时间 */
			ledc_config_t *ledc_config   = malloc(sizeof(ledc_config_t));
			ledc_config->clk_cfg         = LEDC_USE_PLL_DIV_CLK;    /* 启动定时器时，根据给出的分辨率和占空率参数自动选择ledc源时钟 */
			ledc_config->timer_num       = LEDC_TIMER_0;            /* 选择哪个定时器计数（LEDC_TIMER_0~LEDC_TIMER_3） */
			ledc_config->freq_hz         = 5000;                    /* 1KHz（系统自动计算分配系数，并提供freq_hz频率给到定时器） */
			ledc_config->duty_resolution = LEDC_TIMER_10_BIT;       /* 设置定时器最大计数值（请看技术手册表32.4.1） */
			ledc_config->channel         = LEDC_CHANNEL_0;          /* 设置输出通道（LEDC_CHANNEL_0 ~ LEDC_CHANNEL_7） */
			ledc_config->duty            = 100;                     /* 一个周期内占高电平时间(占空比) */
			ledc_config->gpio_num        = GPIO_NUM_23;             /* PWM信号输出那个管脚 */
			ledc_init(ledc_config);

			lvgl_demo();
		}
	}
	else if(key == KEY0_PRES)                                   /* SPI接口屏幕 */
	{
		spilcd_init();                                          /* SPILCD初始化 */
	    sprintf((char *)lcd_id, "LCD ID:%04X", spilcddev.id);   /* 将LCD ID打印到lcd_id数组 */
		
		while(1)
		{
			switch (x)
			{
				case 0:
				{
					spilcd_clear(WHITE);
					break;
				}
				case 1:
				{
					spilcd_clear(BLACK);
					break;
				}
				case 2:
				{
					spilcd_clear(BLUE);
					break;
				}
				case 3:
				{
					spilcd_clear(RED);
					break;
				}
				case 4:
				{
					spilcd_clear(MAGENTA);
					break;
				}
				case 5:
				{
					spilcd_clear(GREEN);
					break;
				}
				case 6:
				{
					spilcd_clear(CYAN);
					break;
				}
				case 7:
				{
					spilcd_clear(YELLOW);
					break;
				}
				case 8:
				{
					spilcd_clear(BRRED);
					break;
				}
				case 9:
				{
					spilcd_clear(GRAY);
					break;
				}
				case 10:
				{
					spilcd_clear(LGRAY);
					break;
				}
				case 11:
				{
					spilcd_clear(BROWN);
					break;
				}
			}

			spilcd_show_string(10, 40,  200, 32, 32, "ESP32-P4", RED);
			spilcd_show_string(10, 80,  200, 24, 24, "SPILCD TEST", RED);
			spilcd_show_string(10, 110, 200, 16, 16, "WKS SMART", RED);
			spilcd_show_string(10, 130, 200, 16, 16, (char *)lcd_id, RED);       /* 显示LCD ID */
			x++;
			if (x == 12)
			{
				x = 0;
			}

			LED0_TOGGLE();
			vTaskDelay(pdMS_TO_TICKS(1000));
		}
	}
	else if(key == BOOT_PRES)     /*wifi APP*/
	{
		wifi_app_flag = 1;

		lcd_init();               /* MIPI LCD初始化 */

		if (mipidev.id == 0x79007)         /* 7寸 1024*600 MIPI屏幕 */
		{
			lvgl_demo();
		}
	}
}
