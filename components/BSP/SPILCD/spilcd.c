/**
 ******************************************************************************
 * @file        spilcd.c
 * @version     V1.0
 * @brief       SPILCD驱动代码
 ******************************************************************************
 * @attention   Waiken-Smart 慧勤智远
 * 
 * 实验平台:     慧勤智远 ESP32-P4 开发板
 ******************************************************************************
 */

#include "spilcd.h"
#include "spilcdfont.h"


esp_lcd_panel_io_handle_t io_handle = NULL;     /* LCD IO设备句柄 */
esp_lcd_panel_handle_t panel_handle = NULL;
_spilcd_dev spilcddev;

/* LCD的宽和高定义 */
uint16_t spilcd_width  = 0;
uint16_t spilcd_height = 0;

/**
 * @brief ST7796 LCD 初始化命令数组
 */
static const st7796_lcd_init_cmd_t st7796_init_cmds[] = {
    {0x11, NULL, 0, 120},
    {0x36, (uint8_t[]){0x48}, 1, 0},
    {0x3A, (uint8_t[]){0x55}, 1, 0},
    {0xF0, (uint8_t[]){0xC3}, 1, 0},
    {0xF0, (uint8_t[]){0x96}, 1, 0},
    {0xB4, (uint8_t[]){0x01}, 1, 0},
    {0xB6, (uint8_t[]){0x0A, 0xA2}, 2, 0},
    {0xB7, (uint8_t[]){0xC6}, 1, 0},
    {0xB9, (uint8_t[]){0x02, 0xE0}, 2, 0},
    {0xC0, (uint8_t[]){0x80, 0x16}, 2, 0},
    {0xC1, (uint8_t[]){0x19}, 1, 0},
    {0xC2, (uint8_t[]){0xA7}, 1, 0},
    {0xC5, (uint8_t[]){0x16}, 1, 0},
    {0xE8, (uint8_t[]){0x40, 0x8A, 0x00, 0x00, 0x29, 0x19, 0xA5, 0x33}, 8, 0},
    {0xE0, (uint8_t[]){0xF0, 0x07, 0x0D, 0x04, 0x05, 0x14, 0x36, 0x54, 0x4C, 0x38, 0x13, 0x14, 0x2E, 0x34}, 14, 0},
    {0xE1, (uint8_t[]){0xF0, 0x10, 0x14, 0x0E, 0x0C, 0x08, 0x35, 0x44, 0x4C, 0x26, 0x10, 0x12, 0x2C, 0x32}, 14, 0},
    {0xF0, (uint8_t[]){0x3C}, 1, 0},
    {0xF0, (uint8_t[]){0x69}, 1, 0},
    {0x00, NULL, 0, 120},
    {0x21, NULL, 0, 0},
    {0x29, NULL, 0, 0},
};

/**
 * @brief       发送ST7789初始化序列
 * @param       无
 * @retval      无
 */
void lcd_ex_st7789_reginit(void)
{
    esp_lcd_panel_io_tx_param(io_handle, 0x11, NULL, 0);
    vTaskDelay(pdMS_TO_TICKS(120)); 
    esp_lcd_panel_io_tx_param(io_handle, 0x36, (uint8_t[]) {0x00}, 1);
    esp_lcd_panel_io_tx_param(io_handle, 0x3A, (uint8_t[]) {0x55}, 1); 
    esp_lcd_panel_io_tx_param(io_handle, 0xB2, (uint8_t[]) {0x0C, 0x0C, 0x00, 0x33, 0x33}, 5);
    esp_lcd_panel_io_tx_param(io_handle, 0xB7, (uint8_t[]) {0x56}, 1);
    esp_lcd_panel_io_tx_param(io_handle, 0xBB, (uint8_t[]) {0x20}, 1);
    esp_lcd_panel_io_tx_param(io_handle, 0xC0, (uint8_t[]) {0x2C}, 1);
    esp_lcd_panel_io_tx_param(io_handle, 0xC2, (uint8_t[]) {0x01}, 1);
    esp_lcd_panel_io_tx_param(io_handle, 0xC3, (uint8_t[]) {0x0F}, 1);
    esp_lcd_panel_io_tx_param(io_handle, 0xC4, (uint8_t[]) {0x20}, 1);
    esp_lcd_panel_io_tx_param(io_handle, 0xC6, (uint8_t[]) {0x0F}, 1);
    esp_lcd_panel_io_tx_param(io_handle, 0xD0, (uint8_t[]) {0xA4, 0xA1}, 2);
    esp_lcd_panel_io_tx_param(io_handle, 0xE0, (uint8_t[]) {
        0xF0, 0x00, 0x06, 0x06, 0x07, 0x05, 0x30, 0x44,
        0x48, 0x38, 0x11, 0x10, 0x2E, 0x34
    }, 14);
    esp_lcd_panel_io_tx_param(io_handle, 0xE1, (uint8_t[]) {
        0xF0, 0x0A, 0x0E, 0x0D, 0x0B, 0x27, 0x2F, 0x44,
        0x47, 0x35, 0x12, 0x12, 0x2C, 0x32
    }, 14);
    esp_lcd_panel_io_tx_param(io_handle, 0x35, (uint8_t[]) {0x00}, 1);
    esp_lcd_panel_io_tx_param(io_handle, 0x21, NULL, 0);
    esp_lcd_panel_io_tx_param(io_handle, 0x29, NULL, 0);
    esp_lcd_panel_io_tx_param(io_handle, 0x2A, (uint8_t[]) {0x00, 0x00, 0x01, 0x3F}, 4);
    esp_lcd_panel_io_tx_param(io_handle, 0x2B, (uint8_t[]) {0x00, 0x00, 0x00, 0xEF}, 4);
    esp_lcd_panel_io_tx_param(io_handle, 0x2C, NULL, 0);
}

/**
 * @brief       读取LCD驱动IC的ID
 * @param       pclk_hz: SPI时钟频率
 * @retval      读取到的ID
 */
static uint16_t spilcd_read_id(uint32_t pclk_hz)
{
    uint8_t id_data[3] = {0};
    
    /* 配置SPI为读取ID的频率 */
    esp_lcd_panel_io_spi_config_t temp_io_config = {
        .dc_gpio_num         = LCD_DC_PIN,
        .cs_gpio_num         = LCD_CS_PIN,
        .pclk_hz             = pclk_hz,
        .lcd_cmd_bits        = 8,
        .lcd_param_bits      = 8,
        .spi_mode            = 0,
        .trans_queue_depth   = 7,
    };
    
    /* 创建临时IO句柄用于读取ID */
    esp_lcd_panel_io_handle_t temp_io_handle = NULL;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &temp_io_config, &temp_io_handle));
    
    /* 发送读ID命令(0x04)，读取3字节ID */
    esp_lcd_panel_io_rx_param(temp_io_handle, 0x04, id_data, 3);
    
    /* 删除临时IO句柄 */
    esp_lcd_panel_io_del(temp_io_handle);
    
    return (id_data[0] << 8) | id_data[1];
}

/**
 * @brief       spilcd初始化
 * @param       无
 * @retval      ESP_OK:初始化成功
 */
esp_err_t spilcd_init(void)
{
	gpio_config_t gpio_init_struct = {0};

    gpio_init_struct.intr_type = GPIO_INTR_DISABLE;         /* 失能引脚中断 */
    gpio_init_struct.mode = GPIO_MODE_INPUT_OUTPUT;         /* 输入输出模式 */
    gpio_init_struct.pull_up_en = GPIO_PULLUP_DISABLE;      /* 失能上拉 */
    gpio_init_struct.pull_down_en = GPIO_PULLDOWN_DISABLE;  /* 失能下拉 */
	gpio_init_struct.pin_bit_mask = (1ull << LCD_RST_PIN) | (1ull << LCD_PWR_PIN);  /* 设置的引脚的位掩码 */
    ESP_ERROR_CHECK(gpio_config(&gpio_init_struct));        /* 配置GPIO */

    spi_bus_config_t buscfg = {
        .sclk_io_num     = LCD_SCLK_PIN,    /* 时钟引脚 */
        .mosi_io_num     = LCD_MOSI_PIN,    /* 主机输出从机输入引脚 */
        .miso_io_num     = LCD_MISO_PIN,    /* 主机输入从机输出引脚 */
        .quadwp_io_num   = -1,              /* 用于Quad模式的WP引脚,未使用时设置为-1 */
        .quadhd_io_num   = -1,              /* 用于Quad模式的HD引脚,未使用时设置为-1 */
        .max_transfer_sz = spilcd_width * spilcd_height * sizeof(uint16_t),   /* 最大传输大小(整屏(RGB565格式)) */
    };
    /* 初始化SPI总线 */
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    /* 复位LCD */
    gpio_set_level(LCD_RST_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(LCD_RST_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(100));

    /* 以10MHz读取LCD ID */
    spilcddev.id = spilcd_read_id(10 * 1000 * 1000);

	if(spilcddev.id == 0x42C2)
	{
		spilcddev.id = 0x7789;
	}
	else if(spilcddev.id == 0xffff)
	{
		spilcddev.id = 0x7796;
	}

	ESP_LOGI("LCD", "Read LCD ID: 0x%X", spilcddev.id);

	/* spi配置 */
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num         = LCD_DC_PIN,          /* DC IO */
        .cs_gpio_num         = LCD_CS_PIN,          /* CS IO */
        .pclk_hz             = 80 * 1000 * 1000,    /* PCLK为80MHz */
        .lcd_cmd_bits        = 8,                   /* 命令位宽 */
        .lcd_param_bits      = 8,                   /* LCD参数位宽 */
        .spi_mode            = 0,                   /* SPI模式 */
        .trans_queue_depth   = 7,                   /* 传输队列 */
    };

    /* 将LCD设备挂载至SPI总线上 */
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle));

    /* 根据ID判断屏幕类型并配置参数 */
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = LCD_RST_PIN,                  /* RST IO */
        .bits_per_pixel = 16,                           /* 颜色深度 */
        .data_endian    = LCD_RGB_DATA_ENDIAN_LITTLE,   /* 小端顺序 */
        .vendor_config = NULL,
    };

    if (spilcddev.id == 0x7796) 
	{
        spilcd_width  = 320;
        spilcd_height = 480;
        
        st7796_vendor_config_t vendor_config = {
            .init_cmds = st7796_init_cmds,
            .init_cmds_size = sizeof(st7796_init_cmds) / sizeof(st7796_lcd_init_cmd_t),
        };
        panel_config.vendor_config = &vendor_config;
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7796(io_handle, &panel_config, &panel_handle));
    } 
	else if (spilcddev.id == 0x7789) 
	{
        spilcd_width  = 240;
        spilcd_height = 320;
        
        panel_config.rgb_ele_order = COLOR_RGB_ELEMENT_ORDER_RGB;  /* RGB颜色格式 */
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle));
        lcd_ex_st7789_reginit();  /* 执行ST7789初始化序列 */
    } else {
        ESP_LOGE("LCD", "Unsupported LCD ID: 0x%X", spilcddev.id);
        return ESP_FAIL;
    }

    spilcddev.pheight = spilcd_height;  /* 高度 */
    spilcddev.pwidth  = spilcd_width;   /* 宽度 */

    /* 复位LCD */
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    /* 反显 */
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true));
    /* 初始化LCD句柄 */
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    /* 打开屏幕 */
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    LCD_PWR(1);                 /* 打开背光 */
    spilcd_display_dir(0);      /* 竖屏显示 */
    spilcd_clear(WHITE);        /* 清屏 */

    return ESP_OK;
}

/**
 * @brief       设置屏幕方向
 * @param       dir: 0为竖屏，1为横屏
 * @retval      无
 */
void spilcd_display_dir(uint8_t dir)
{
    spilcddev.dir = dir;

    if (spilcddev.dir == 0)         /* 竖屏 */
    {
        spilcddev.width = spilcddev.pwidth;
        spilcddev.height = spilcddev.pheight;
        esp_lcd_panel_swap_xy(panel_handle, false);
        esp_lcd_panel_mirror(panel_handle, false, false);
    }
    else if (spilcddev.dir == 1)    /* 横屏 */
    {
        spilcddev.width = spilcddev.pheight;
        spilcddev.height = spilcddev.pwidth;
        esp_lcd_panel_swap_xy(panel_handle, true);
        esp_lcd_panel_mirror(panel_handle, true, false);
    }
}

/**
 * @brief       清屏
 * @param       color: 颜色值
 * @retval      无
 */
void spilcd_clear(uint16_t color)
{
	if (spilcddev.id == 0x7796) 
	{
        color = (color << 8) | (color >> 8);
    }

    /* 以40行作为缓冲,提高速率,若出现内存不足,可以减少缓冲行数 */
    uint16_t *buffer = heap_caps_malloc(spilcddev.width * sizeof(uint16_t) * 40, MALLOC_CAP_DMA);

    if (NULL == buffer)
    {
        ESP_LOGE("TAG", "Memory for bitmap is not enough");
    }
    else
    {
        for (uint32_t i = 0; i < spilcddev.width * 40; i++)
        {
            buffer[i] = color;
        }
        
        for (uint16_t y = 0; y < spilcddev.height; y+=40)
        {
            esp_lcd_panel_draw_bitmap(panel_handle, 0, y, spilcddev.width, y + 40, buffer);
        }
    }

    heap_caps_free(buffer);
}

/**
 * @brief       在指定区域内填充单个颜色
 * @param       (sx,sy),(ex,ey):填充矩形对角坐标,区域大小为:(ex - sx + 1) * (ey - sy + 1)
 * @param       color:要填充的颜色
 * @retval      无
 */
void spilcd_fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t color)
{
    uint16_t width = ex - sx + 1;
    uint16_t height = ey - sy + 1;

	uint16_t buffer_size = width * 4; 
    if (buffer_size > spilcddev.width * 4) 
	{
        buffer_size = spilcddev.width * 4;
    }

	if (spilcddev.id == 0x7796) 
	{
        color = (color << 8) | (color >> 8);
    }

    uint16_t *buffer = heap_caps_malloc(spilcddev.width * sizeof(uint16_t), MALLOC_CAP_DMA);
    if (NULL == buffer)
    {
        ESP_LOGE("TAG", "Memory for bitmap is not enough");
    }
    else
    {
        for (uint16_t i = 0; i < buffer_size; i++)
        {
            buffer[i] = color;
        }

        for (uint16_t y = sy; y <= ey; y += 4)
    {
        uint16_t block_height = (ey - y + 1) < 4 ? (ey - y + 1) : 4;
        esp_lcd_panel_draw_bitmap(panel_handle, sx, y, sx + width, y + block_height, buffer);
    }
    }

    heap_caps_free(buffer);
}

/**
 * @brief       绘画一个像素点
 * @param       x    : x轴坐标
 * @param       y    : y轴坐标
 * @param       color: 颜色值
 * @retval      无
 */
void spilcd_draw_point(uint16_t x, uint16_t y, uint16_t color)
{
	if (spilcddev.id == 0x7796) 
	{
        color = (color << 8) | (color >> 8);
    }
    esp_lcd_panel_draw_bitmap(panel_handle, x, y, x + 1, y + 1, &color);
}

/**
 * @brief       画线函数(直线、斜线)
 * @param       x1,y1   起点坐标
 * @param       x2,y2   终点坐标
 * @param       color 填充颜色
 * @retval      无
 */
void spilcd_draw_line(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    uint16_t t;
    int xerr = 0, yerr = 0, delta_x, delta_y, distance;
    int incx, incy, row, col;
    delta_x = x2 - x1;
    delta_y = y2 - y1;
    row = x1;
    col = y1;

    if (delta_x > 0)
    {
        incx = 1;
    }
    else if (delta_x == 0)
    {
        incx = 0;
    }
    else
    {
        incx = -1;
        delta_x = -delta_x;
    }

    if (delta_y > 0)
    {
        incy = 1;
    }
    else if (delta_y == 0)
    {
        incy = 0;
    }
    else
    {
        incy = -1;
        delta_y = -delta_y;
    }

    if ( delta_x > delta_y)
    {
        distance = delta_x;
    }
    else
    {
        distance = delta_y;
    }

    for (t = 0; t <= distance + 1; t++)
    {
        spilcd_draw_point(row, col, color);
        xerr += delta_x;
        yerr += delta_y;

        if (xerr > distance)
        {
            xerr -= distance;
            row += incx;
        }

        if (yerr > distance)
        {
            yerr -= distance;
            col += incy;
        }
    }
}

/**
 * @brief       画水平线
 * @param       x0,y0: 起点坐标
 * @param       len  : 线长度
 * @param       color: 矩形的颜色
 * @retval      无
 */
void spilcd_draw_hline(uint16_t x, uint16_t y, uint16_t len, uint16_t color)
{
    if ((len == 0) || (x > spilcddev.width) || (y > spilcddev.height)) return;

    spilcd_fill(x, y, x + len - 1, y, color);
}

/**
 * @brief       画一个矩形
 * @param       x1,y1   起点坐标
 * @param       x2,y2   终点坐标
 * @param       color 填充颜色
 * @retval      无
 */
void spilcd_draw_rectangle(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,uint16_t color)
{
    spilcd_draw_line(x0, y0, x1, y0,color);
    spilcd_draw_line(x0, y0, x0, y1,color);
    spilcd_draw_line(x0, y1, x1, y1,color);
    spilcd_draw_line(x1, y0, x1, y1,color);
}

/**
 * @brief       画一个圆
 * @param       x0,y0   圆心坐标
 * @param       r   圆半径
 * @param       color 填充颜色
 * @retval      无
 */
void spilcd_draw_circle(uint16_t x0, uint16_t y0, uint16_t r, uint16_t color)
{
    int a, b;
    int di;
    a = 0;
    b = r;
    di = 3 - (r << 1);

    while (a <= b)
    {
        spilcd_draw_point(x0 - b, y0 - a, color);
        spilcd_draw_point(x0 + b, y0 - a, color);
        spilcd_draw_point(x0 - a, y0 + b, color);
        spilcd_draw_point(x0 - b, y0 - a, color);
        spilcd_draw_point(x0 - a, y0 - b, color);
        spilcd_draw_point(x0 + b, y0 + a, color);
        spilcd_draw_point(x0 + a, y0 - b, color);
        spilcd_draw_point(x0 + a, y0 + b, color);
        spilcd_draw_point(x0 - b, y0 + a, color);
        a++;

        if (di < 0)
        {
            di += 4 * a + 6;
        }
        else
        {
            di += 10 + 4 * (a - b);
            b--;
        }

        spilcd_draw_point(x0 + a, y0 + b, color);
    }
}

/**
 * @brief       在指定位置显示一个字符
 * @param       x,y  : 坐标
 * @param       chr  : 要显示的字符:" "--->"~"
 * @param       size : 字体大小 12/16/24/32
 * @param       mode : 叠加方式(1); 非叠加方式(0);
 * @param       color : 字符的颜色;
 * @retval      无
 */
void spilcd_show_char(uint16_t x, uint16_t y, uint8_t chr, uint8_t size, uint8_t mode, uint16_t color)
{
    const uint8_t *ch_code;     /* 存放chr字符对应数组的首地址 */
    uint8_t ch_width;           /* 字符的宽度 */
    uint8_t ch_height;          /* 字符的高度 */
    uint8_t ch_size;            /* 字符的大小(字节) */
    uint8_t ch_offset;          /* 字符在字库的相对位置 */
    uint8_t byte_index;         /* 字符对应数据的索引值 */
    uint8_t byte_code;          /* 字符对应数据 */
    uint8_t bit_index;          /* 字符对应字节数据的位索引 */
    uint16_t colortemp = 0;     /* 颜色数据 */
    uint16_t pix_index = 0;
    uint16_t *pcolor = NULL;

	if (spilcddev.id == 0x7796) 
	{
        color = (color << 8) | (color >> 8);
    }

    /* 字体大小(字节) =       字体宽度占用字体大小              * 字体高度 */
    ch_size = ((size / 2) / 8 +  (((size / 2) % 8) ? 1 : 0)) * size;        /* 得到字体一个字符对应点阵集所占的字节数 */

    pcolor = heap_caps_malloc(size * size * 2, MALLOC_CAP_INTERNAL);        /* 申请大小 */
    if (NULL == pcolor)
    {
        ESP_LOGE("TAG", "Memory for bitmap is not enough");
    }

    ch_offset = chr - ' ';                                          /* 得到偏移后的值（ASCII字库是从空格开始取模，所以-' '就是对应字符的字库） */
        
    switch (size)
    {
        case 12:
            ch_code = (uint8_t *)asc2_1206_SPI[ch_offset];              /* 调用1206字体 */
            ch_width = 6;
            ch_height = 12;
            break;

        case 16:
            ch_code = (uint8_t *)asc2_1608_SPI[ch_offset];              /* 调用1608字体 */
            ch_width = 8;
            ch_height = 16;
            break;

        case 24:
            ch_code = (uint8_t *)asc2_2412_SPI[ch_offset];              /* 调用2412字体 */
            ch_width = 12;
            ch_height = 24;
            break;

        case 32:
            ch_code = (uint8_t *)asc2_3216_SPI[ch_offset];              /* 调用3216字体 */
            ch_width = 16;
            ch_height = 32;
            break;

        default:
            return ;
    }

    if ((x + ch_width > spilcddev.width) || (y + ch_height > spilcddev.height))
    {
        return;
    }

    for (byte_index = 0; byte_index < ch_size; byte_index++)
    {   
        byte_code = ch_code[byte_index];  /* 获取字符的点阵数据 */

        for (bit_index = 0; bit_index < 8; bit_index++)   /* 一个字节8个点 */
        {
            if ((byte_code & 0x80) != 0)                  /* 有效点,需要显示 */
            {   
                colortemp = color;    
            }
            else if (mode == 0)
            {
                colortemp = 0xFFFF;
            }
            
            pcolor[pix_index] = colortemp;
            pix_index++;

            if ((size == 24) && (byte_index % 2))   /* 24号字体比较特殊,奇数字节只有四位有效 */
            {
                if (bit_index == 3)
                {
                    break;
                }
            }

            byte_code <<= 1;                    /* 移位, 以便获取下一个位的状态 */
        }
    }   

    esp_lcd_panel_draw_bitmap(panel_handle, x, y, x + ch_width, y + ch_height, (uint16_t *)pcolor);

    heap_caps_free(pcolor);
}

/**
 * @brief       m^n函数
 * @param       m,n: 输入参数
 * @retval      m^n次方
 */
uint32_t lcd_pow(uint8_t m, uint8_t n)
{
    uint32_t result = 1;

    while(n--) result *= m;

    return result;
}

/**
 * @brief       显示len个数字
 * @param       x,y : 起始坐标
 * @param       num : 数值(0 ~ 2^32)
 * @param       len : 显示数字的位数
 * @param       size: 选择字体 12/16/24/32
 * @retval      无
 */
void spilcd_show_num(uint16_t x, uint16_t y, uint32_t num, uint8_t len, uint8_t size, uint16_t color)
{
    uint8_t t, temp;
    uint8_t enshow = 0;

    for (t = 0; t < len; t++)                                               /* 按总显示位数循环 */
    {
        temp = (num / lcd_pow(10, len - t - 1)) % 10;                       /* 获取对应位的数字 */

        if (enshow == 0 && t < (len - 1))                                   /* 没有使能显示,且还有位要显示 */
        {
            if (temp == 0)
            {
                spilcd_show_char(x + (size / 2)*t, y, ' ', size, 0, color);    /* 显示空格,占位 */
                continue;                                                   /* 继续下个一位 */
            }
            else
            {
                enshow = 1;                                                 /* 使能显示 */
            }

        }

        spilcd_show_char(x + (size / 2)*t, y, temp + '0', size, 0, color);     /* 显示字符 */
    }
}

/**
 * @brief       扩展显示len个数字(高位是0也显示)
 * @param       x,y : 起始坐标
 * @param       num : 数值(0 ~ 2^32)
 * @param       len : 显示数字的位数
 * @param       size: 选择字体 12/16/24/32
 * @param       mode: 显示模式
 *              [7]:0,不填充;1,填充0.
 *              [6:1]:保留
 *              [0]:0,非叠加显示;1,叠加显示.
 * @param       color : 数字的颜色;
 * @retval      无
 */
void spilcd_show_xnum(uint16_t x, uint16_t y, uint32_t num, uint8_t len, uint8_t size, uint8_t mode, uint16_t color)
{
    uint8_t t, temp;
    uint8_t enshow = 0;

    for (t = 0; t < len; t++)                                                           /* 按总显示位数循环 */
    {
        temp = (num / lcd_pow(10, len - t - 1)) % 10;                                   /* 获取对应位的数字 */

        if (enshow == 0 && t < (len - 1))                                               /* 没有使能显示,且还有位要显示 */
        {
            if (temp == 0)
            {
                if (mode & 0X80)                                                        /* 高位需要填充0 */
                {
                    spilcd_show_char(x + (size / 2)*t, y, '0', size, mode & 0X01, color);  /* 用0占位 */
                }
                else
                {
                    spilcd_show_char(x + (size / 2)*t, y, ' ', size, mode & 0X01, color);  /* 用空格占位 */
                }
                continue;
            }
            else
            {
                enshow = 1;                                                             /* 使能显示 */
            }
        }
        spilcd_show_char(x + (size / 2)*t, y, temp + '0', size, mode & 0X01, color);
    }
}

/**
 * @brief       显示字符串
 * @param       x,y         : 起始坐标
 * @param       width,height: 区域大小
 * @param       size        : 选择字体 12/16/24/32
 * @param       p           : 字符串首地址
 * @retval      无
 */
void spilcd_show_string(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t size, char *p, uint16_t color)
{
    uint8_t x0 = x;
    width += x;
    height += y;

    while ((*p <= '~') && (*p >= ' '))   /* 判断是不是非法字符! */
    {
        if (x >= width)
        {
            x = x0;
            y += size;
        }

        if (y >= height) break;  /* 退出 */

        spilcd_show_char(x, y, *p, size, 0, color);
        x += size / 2;
        p++;
    }
}

