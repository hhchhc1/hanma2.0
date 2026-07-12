/**
 ******************************************************************************
 * @file        app_calculator.c
 * @version     V1.0
 * @brief       计算器 APP
 ******************************************************************************
 * @attention   Waiken-Smart 慧勤智远
 * 
 * 实验平台:     慧勤智远 ESP32-P4 开发板
 ******************************************************************************
 */

#include "app_calculator.h"

calculator_ui_t calculator_ui;
/* 计算器参数 */
double lv_math_x1 = 0;           // 第一个操作数
double lv_math_x2 = 0;           // 第二个操作数
/* 计算结果 */
double lv_math_result = 0;       // 计算结果
/* 存储计算结果字符串 */
char str[100];
/* 计算器运算符类型 */
uint8_t lv_math_flag = 0;        // 当前操作符（+ - * /）
/* 正负数值互换 */
double cbp_and_negative = 0;
/* 逻辑标志位 */
uint8_t logical_flag_bit = 0x01;    // 标志位用于区分不同的状态

static const char * btnm_map[] = {"AC", "+/-", "%", "/","\n",
                                  "7", "8", "9", "*","\n",
                                  "4", "5", "6", "-","\n",
                                  "1", "2", "3", "+","\n",
                                  "0", ".", "=",""
                                 };

/**
  * @brief  等于=按键计算
  * @param  x1:获取第一个数值
  * @param  x2:获取第二个数值
  * @param  ctype:计算的类型
  * @retval 返回计算数值
  */
double lv_math_calc(double x1, double x2, uint8_t ctype)
{
    switch(ctype)
    {
        case 0: /* 加法 */
          return x1 + x2;
        case 1: /* 减法 */
          return x1 - x2;
        case 2: /* 乘法 */
          return x1 * x2;
        case 3: /* 除法 */
          return x1 / x2;
        case 4: /* 没有任何运算符 */
          return x1 = fmod(x1, x2);  // 使用 fmod 函数计算余数;
        default:
          return 0; // 默认返回
    }
}

const char * lv_math_flag_to_operator(uint8_t lv_math_flag)
{
    switch(lv_math_flag)
    {
        case 4:  // %
            return "%";
        case 3:  // /
            return "/";
        case 2:  // *
            return "*";
        case 1:  // -
            return "-";
        case 0:  // +
            return "+";
        default:
            return "";  // Invalid flag or no operator
    }
}

static void event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * obj = lv_event_get_target(e);

    /* When the button matrix draws the buttons... */
    if(code == LV_EVENT_DRAW_PART_BEGIN)
    {
        lv_obj_draw_part_dsc_t * dsc = lv_event_get_draw_part_dsc(e);
        if(dsc->class_p == &lv_btnmatrix_class && dsc->type == LV_BTNMATRIX_DRAW_PART_BTN)
        {
            bool pressed = false;
            if (lv_btnmatrix_get_selected_btn(obj) == dsc->id && lv_obj_has_state(obj, LV_STATE_CHECKED))
            {
                pressed = true;
            }

            dsc->label_dsc->color = lv_color_make(0, 0, 0);
            dsc->rect_dsc->bg_color = lv_color_make(168, 168, 168);
            dsc->rect_dsc->radius = 100;
  
            if (pressed)
            {
                dsc->rect_dsc->bg_opa = LV_OPA_50;
                dsc->rect_dsc->bg_color = lv_palette_darken(LV_PALETTE_BLUE, 3);
            }

            dsc->rect_dsc->shadow_color = lv_color_make(0, 0, 0);

            // 操作符按键样式
            if (dsc->id == 3 || dsc->id == 7 || dsc->id == 11 || dsc->id == 15 || dsc->id == 18)
            {
                dsc->rect_dsc->bg_color = lv_color_make(253, 159, 11);
                if (pressed)
                {
                    dsc->rect_dsc->bg_opa = LV_OPA_50;
                    dsc->rect_dsc->bg_color = lv_palette_darken(LV_PALETTE_BLUE, 3);
                }

                dsc->label_dsc->color = lv_color_make(255, 255, 255);
            }
            else if (dsc->id == 4 || dsc->id == 5 || dsc->id == 6
                    || dsc->id == 8 || dsc->id == 9 || dsc->id == 10
                    || dsc->id == 12 || dsc->id == 13 || dsc->id == 14
                    || dsc->id == 16 || dsc->id == 17)
            {
                dsc->rect_dsc->bg_color = lv_color_make(54, 54, 54);
                if (pressed)
                {
                    dsc->rect_dsc->bg_opa = LV_OPA_50;
                    dsc->rect_dsc->bg_color = lv_palette_darken(LV_PALETTE_BLUE, 3);
                }

                dsc->label_dsc->color = lv_color_make(255, 255, 255);

                if (dsc->id == 16)
                {
                    dsc->label_dsc->ofs_x = -85;
                }
            }
        }
    }
    else if (code == LV_EVENT_VALUE_CHANGED)
    {
        uint32_t id = lv_btnmatrix_get_selected_btn(obj);
        const char * txt =  lv_btnmatrix_get_btn_text(obj, id);

        if (id == 0) /* AC 按键 */
        {
            lv_label_set_text(calculator_ui.calculator_dec,"");
            lv_textarea_set_text(calculator_ui.calculator_text_area, "0");
            lv_label_set_text(calculator_ui.calculator_result,"DEG");
            lv_obj_set_style_text_color(calculator_ui.calculator_result,lv_color_make(0, 0, 0),LV_STATE_DEFAULT);
            lv_math_x1 = 0;
            lv_math_x2 = 0;
            lv_math_result = 0;
            lv_math_flag = 0;
            logical_flag_bit = 0x01;
        }
        else if (id == 18) /* "=" 按键 */
        {
            if (logical_flag_bit == 0x02)  // 如果是连续按下等号
            {
                lv_math_result = lv_math_calc(lv_math_result, lv_math_x2, lv_math_flag);
            }
            else // 标志位：如果是第一次按下等号
            {
                lv_math_x2 = atof(lv_textarea_get_text(calculator_ui.calculator_text_area));  // 获取当前显示的值
                lv_math_result = lv_math_calc(lv_math_x1, lv_math_x2, lv_math_flag); // 使用前一次的结果
            }

            sprintf(str, "%g", lv_math_result);
            lv_textarea_set_text(calculator_ui.calculator_text_area, str);
            lv_label_set_text(calculator_ui.calculator_result,"RES");
            
            sprintf(str, "%.2f %s %.2f = %.2f",lv_math_x1,lv_math_flag_to_operator(lv_math_flag),lv_math_x2,lv_math_result);
            lv_label_set_text(calculator_ui.calculator_dec,str);

            lv_obj_set_style_text_color(calculator_ui.calculator_result,lv_color_make(255, 0, 0),LV_STATE_DEFAULT);
            lv_math_x1 = lv_math_result;    // 保存结果供下次使用
            logical_flag_bit = 0x02;       // 标记计算完成
        }
        else if (id == 2 || id == 3 || id == 7 || id == 11 || id == 15) /* 运算符 按键 */
        {
            // 如果lv_math_x1尚未设置，才从文本框获取并设置
            if (logical_flag_bit == 0x03)  // 如果标志位为0x01，说明lv_math_x1已被设定过
            {
                // 如果标志位为0x01，表示第一次输入已经完成，不需要再设置lv_math_x1
                // 避免重复赋值
            }
            else
            {
                lv_math_x1 = atof(lv_textarea_get_text(calculator_ui.calculator_text_area));
            }

            // 根据id选择运算符
            if (id == 2) lv_math_flag = 4;  /* % */
            else if (id == 3) lv_math_flag = 3;  /* / */
            else if (id == 7) lv_math_flag = 2;  /* * */
            else if (id == 11) lv_math_flag = 1; /* - */
            else lv_math_flag = 0; /* + */

            // 显示选择的运算符
            lv_label_set_text(calculator_ui.calculator_result, lv_btnmatrix_get_btn_text(obj, id));
            lv_obj_set_style_text_color(calculator_ui.calculator_result, lv_color_make(0, 0, 0), LV_STATE_DEFAULT);
            
            // 重置文本框
            lv_textarea_set_text(calculator_ui.calculator_text_area, "0");

            // 标记运算符已选择
            logical_flag_bit = 0x03;
        }
        else if (id == 1) /* +/- 按键 */
        {
            cbp_and_negative = atof(lv_textarea_get_text(calculator_ui.calculator_text_area));
            cbp_and_negative = -cbp_and_negative;
            sprintf(str, "%g", cbp_and_negative);
            lv_textarea_set_text(calculator_ui.calculator_text_area, str);
        }
        else if ((id >= 4 && id <= 6) || (id >= 8 && id <= 10) || 
                (id >= 12 && id <= 14) || id == 16 || id == 17)
        {
            if (id == 17)
            {
                if (logical_flag_bit == 0x01 || logical_flag_bit == 0x03 || logical_flag_bit == 0x02)
                {
                    lv_textarea_set_text(calculator_ui.calculator_text_area, "0.");
                    logical_flag_bit &= ~logical_flag_bit;
                } 
            }

            if (logical_flag_bit == 0x01 || logical_flag_bit == 0x02 || logical_flag_bit == 0x03) // 计算完成后清空输入框
            {
                lv_textarea_set_text(calculator_ui.calculator_text_area, "");
                logical_flag_bit &= ~logical_flag_bit;
            }

            if (txt == btnm_map[21] && strstr(lv_textarea_get_text(calculator_ui.calculator_text_area), ".")) 
            {
                return;
            }

            if (txt[0] != NULL)
            {
                lv_textarea_add_char(calculator_ui.calculator_text_area, txt[0]);
            }
        }
    }
}

void lv_app_calculator_init(void)
{
    calculator_ui.calculator_main_ui = lv_obj_create(lv_scr_act());
    lv_obj_set_style_bg_color(calculator_ui.calculator_main_ui, lv_color_hex(0x000000), LV_STATE_DEFAULT);                  /* 设置背景颜色 */
    lv_obj_set_size(calculator_ui.calculator_main_ui, lv_obj_get_width(lv_scr_act()), lv_obj_get_height(lv_scr_act()) - lv_obj_get_height(lv_scr_act()) / 20);  
    lv_obj_set_style_radius(calculator_ui.calculator_main_ui, 0, LV_STATE_DEFAULT);                                         /* 无圆角 */
    lv_obj_set_style_border_opa(calculator_ui.calculator_main_ui, LV_OPA_0, LV_STATE_DEFAULT);                              /* 边界透明 */
    lv_obj_set_pos(calculator_ui.calculator_main_ui, 0, lv_obj_get_height(lv_scr_act()) / 20);                              /* 设置位置 */
    lv_obj_clear_flag(calculator_ui.calculator_main_ui, LV_OBJ_FLAG_SCROLLABLE);                                            /* 禁止滚动 */
    lv_obj_update_layout(calculator_ui.calculator_main_ui);

    calculator_ui.calculator_btnm = lv_btnmatrix_create(calculator_ui.calculator_main_ui);
    lv_obj_set_style_text_font(calculator_ui.calculator_btnm, &lv_font_montserrat_32, LV_STATE_DEFAULT);                  
    lv_obj_set_size(calculator_ui.calculator_btnm,lv_obj_get_width(calculator_ui.calculator_main_ui),lv_obj_get_height(calculator_ui.calculator_main_ui) - 200); 
    lv_obj_set_style_bg_color(calculator_ui.calculator_btnm,lv_color_make(2,2,2),LV_STATE_DEFAULT);
    lv_obj_set_style_radius(calculator_ui.calculator_btnm, 0, LV_STATE_DEFAULT);                                         /* 无圆角 */
    lv_obj_set_style_border_opa(calculator_ui.calculator_btnm, LV_OPA_0, LV_STATE_DEFAULT);                              /* 边界透明 */
    lv_btnmatrix_set_map(calculator_ui.calculator_btnm, btnm_map);
    lv_obj_align(calculator_ui.calculator_btnm, LV_ALIGN_BOTTOM_MID, 0, 20);                                             /* 底部间距 */
    lv_obj_add_event_cb(calculator_ui.calculator_btnm, event_cb, LV_EVENT_ALL, NULL); /* 添加事件回调函数 */
    lv_btnmatrix_set_btn_width(calculator_ui.calculator_btnm, 16, 2);
    lv_obj_update_layout(calculator_ui.calculator_btnm);

    calculator_ui.calculator_text_area = lv_textarea_create(calculator_ui.calculator_main_ui);
    lv_textarea_set_text(calculator_ui.calculator_text_area, "0");
    lv_textarea_set_one_line(calculator_ui.calculator_text_area, true);          
    lv_textarea_set_cursor_click_pos(calculator_ui.calculator_text_area,false);  /* 隐藏光标 */
    lv_obj_set_size(calculator_ui.calculator_text_area,lv_obj_get_width(calculator_ui.calculator_main_ui) - 40,lv_obj_get_height(calculator_ui.calculator_main_ui) / 6 + 40); 
    lv_obj_align_to(calculator_ui.calculator_text_area, calculator_ui.calculator_btnm, LV_ALIGN_OUT_TOP_MID, 0, 10); 
    lv_obj_set_style_text_font(calculator_ui.calculator_text_area,&lv_font_montserrat_40,0);                         
    lv_obj_set_style_text_align(calculator_ui.calculator_text_area,LV_TEXT_ALIGN_RIGHT,LV_PART_MAIN);
    lv_obj_clear_flag(calculator_ui.calculator_text_area, LV_OBJ_FLAG_CLICKABLE);

    calculator_ui.calculator_result = lv_label_create(calculator_ui.calculator_main_ui);
    lv_label_set_text(calculator_ui.calculator_result,"DEG");
    lv_obj_set_style_text_font(calculator_ui.calculator_result,&lv_font_montserrat_32,0);                         
    lv_obj_align_to(calculator_ui.calculator_result,calculator_ui.calculator_text_area,LV_ALIGN_BOTTOM_RIGHT,0,0);
    lv_obj_update_layout(calculator_ui.calculator_result);

    calculator_ui.calculator_dec = lv_label_create(calculator_ui.calculator_main_ui);
    lv_obj_set_width(calculator_ui.calculator_dec,lv_obj_get_width(calculator_ui.calculator_btnm));
    lv_label_set_text(calculator_ui.calculator_dec,"");
    lv_obj_set_style_text_font(calculator_ui.calculator_dec,&lv_font_montserrat_24,0);                          
    lv_obj_set_style_text_opa(calculator_ui.calculator_dec,LV_OPA_50,LV_STATE_DEFAULT);
    lv_obj_align_to(calculator_ui.calculator_dec,calculator_ui.calculator_text_area,LV_ALIGN_BOTTOM_RIGHT,0,- lv_obj_get_height(calculator_ui.calculator_result) - 5); 
    lv_obj_set_style_text_align(calculator_ui.calculator_dec,LV_TEXT_ALIGN_RIGHT,LV_PART_MAIN);

    lv_general.current_parent = calculator_ui.calculator_main_ui;
}
