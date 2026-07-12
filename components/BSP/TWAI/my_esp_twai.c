/**
 ******************************************************************************
 * @file        my_esp_twai.c
 * @version     V1.0
 * @brief       TWAI驱动代码
 ******************************************************************************
 * @attention   Waiken-Smart 慧勤智远
 * 
 * 实验平台:     慧勤智远 ESP32-P4 开发板
 ******************************************************************************
 */

#include "my_esp_twai.h"


const char *twai_tag = "twai";
uint8_t tawi_state = 0x00;
twai_mode_t tawi_mode = 0x00;

/**
 * @brief       TWAI初始化
 * @param       mode: TWAI控制器操作模式
 * @note        TWAI_MODE_NORMAL:正常模式,支持TWAI控制器参与总线活动,传输和接收报文/错误帧.发送报文需要另外一个节点应答
 * @note        TWAI_MODE_NO_ACK:无应答模式,与正常模式类似,但不需要接收方发送应答信号,即使没有应答信号也会被视为传输成功,常用于自测
 * @retval      ESP_OK:表示初始化成功
 */
esp_err_t twai_init(twai_mode_t mode)
{
    if (tawi_state == 0xFF)
    {
        ESP_ERROR_CHECK(twai_stop());
        ESP_ERROR_CHECK(twai_driver_uninstall());
        tawi_state = 0x00;
    }

    tawi_mode = mode;
    /* TWAI接口基本配置 */
    twai_general_config_t twai_config = {
        .controller_id  = 0,                    /* TWAI控制器编号(非0时,初始化函数不一样) */
        .mode           = mode,                 /* TWAI控制器操作模式 */
        .tx_io          = TWAI_TX_GPIO_PIN,     /* 发送引脚 */
        .rx_io          = TWAI_RX_GPIO_PIN,     /* 接收引脚 */
        .clkout_io      = TWAI_IO_UNUSED,       /* 时钟输出引脚 */
        .bus_off_io     = TWAI_IO_UNUSED,       /* 总线离线引脚 */
        .tx_queue_len   = 5,                    /* 发送队列长度 */
        .rx_queue_len   = 5,                    /* 接收队列长度 */
        .alerts_enabled = TWAI_ALERT_NONE,      /* 警告标志 */
        .clkout_divider = 0,                    /* 时钟分频,1to14,0不用 */
        .intr_flags     = ESP_INTR_FLAG_LEVEL1, /* 中断优先级 */
    };

    /* TWAI接口位速率配置(官方提供1K到1Mbps常用配置宏) */
    /* Note 波特率计算公式: baud = Ftwai / (tseg_1 + tseg_2 + tss) / brp */
    twai_timing_config_t timing_config = {              /* 也可用TWAI_TIMING_CONFIG_500KBITS()宏代替以下赋值 */
        .clk_src                = TWAI_CLK_SRC_DEFAULT, /* 时钟源(默认来自XTAL时钟40MHz) */   
        .quanta_resolution_hz   = 10000000,             /* 时间单元的分辨率(单位Hz),此处有设置值,brp可计算获得;否则不设置该值,brp需手动设置 */
        .brp                    = 0,                    /* 时钟分频器(跟quanta_resolution_hz相关,(fclk_src/quanta_resolution_hz)) */
        .tseg_1                 = 15,                   /* 时间段1(1~16个时间单元) */
        .tseg_2                 = 4,                    /* 时间段2(1~8个时间单元) */
        .sjw                    = 3,                    /* 再次同步跳跃宽度(1~4个时间单元) */
        .triple_sampling        = false,                /* 三重采样(较低波特率下建议使用,有利于过滤总线尖峰信号;若波特率较高,达到500Kbps时建议关闭) */
    };

    /* 过滤器配置 */
    twai_filter_config_t filter_config = {
        .acceptance_code = 0,                   /* 接收码 */
        .acceptance_mask = 0xFFFFFFFF,          /* 接收掩码 0xFFFFFFFF表示全部接收 */
        .single_filter   = true,                /* true:单过滤器模式;false:双过滤器模式 */
    };
    ESP_ERROR_CHECK(twai_driver_install(&twai_config, &timing_config, &filter_config));     /* 安装TWAI驱动 */

    ESP_ERROR_CHECK(twai_start());              /* 启动TWAI驱动 */

    tawi_state = 0xFF;
    return ESP_OK;
}

/**
 * @brief       TWAI发送一帧数据
 * @note        发送格式固定为：标准ID,数据帧
 * @param       id:标准ID (11位)
 * @param       msg:数据指针,最大8个字节
 * @param       len:数据长度(最大8)
 * @retval      ESP_OK成功; ESP_FAIL失败
 */
esp_err_t twai_send_data(uint32_t id, uint8_t msg[], uint8_t len)
{
    twai_message_t tx_msg = {   /* 设置报文类型及格式 */
        .extd   = 0,            /* 0标准帧; 1扩展帧 */
        .rtr    = 0,            /* 0数据帧; 1远程帧 */
        .ss     = 1,            /* 0错误重发; 1单次发送(仲裁或丢失时消息不会被重发) */
        .self   = 0,            /* 报文是否为自收发(回环) */
    };

    if (tawi_mode == TWAI_MODE_NO_ACK)     /* 无应答模式 + (self = 1) 实现自发自收功能 */
    {
        tx_msg.self = 1;    /* 1把消息发送到总线上,且接收自己发送的消息,也接收总线上的消息 */
    }  
    
    /* 正常模式 + (self = 0) 实现 把消息发送到总线上,但不接收自己发送的消息,且不接收总线上的消息 */
 
    tx_msg.dlc_non_comp     = 0;                    /* 0数据长度不大于8(ISO 11898-1); 1数据长度大于8(非标) */
    tx_msg.identifier       = id;                   /* 标准帧格式(11位ID)/扩展帧格式(29位ID) */
    tx_msg.data_length_code = len;                  /* 数据长度代码(字节为单位),数据帧数据符号大小 */
    memset(&tx_msg.data, 0, sizeof(tx_msg.data));   /* 发送数据，对远程帧无效 */

    for (int i = 0; i < len; i++)                   /* 复制数据到消息结构体 */
    {
        tx_msg.data[i] = msg[i];                    /* 数据帧数据(最多8个字节,跟data_length_code匹配) */
    }
    
    /* 发送消息 */
    ESP_ERROR_CHECK(twai_transmit(&tx_msg, pdMS_TO_TICKS(1000)));

    return ESP_OK;
}

/**
 * @brief       TWAI接收数据查询
 * @note        接收数据格式固定为:标准ID,数据帧
 * @param       id:要查询的 标准ID(11位)
 * @param       buf:数据缓存区
 * @retval      ESP_OK成功; ESP_FAIL失败
 */
esp_err_t twai_receive_data(uint32_t id, uint8_t buf[])
{
    twai_message_t rx_message;
    ESP_ERROR_CHECK(twai_receive(&rx_message, portMAX_DELAY));
  
    if (rx_message.identifier == id)
    {
        for (int i = 0; i < rx_message.data_length_code; i++)
        {
            buf[i] = rx_message.data[i];
        }
    }

    return ESP_OK;
}