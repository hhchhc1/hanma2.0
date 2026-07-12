/**
 ******************************************************************************
 * @file        rtc.c
 * @version     V1.0
 * @brief       RTC驱动代码
 ******************************************************************************
 * @attention   Waiken-Smart 慧勤智远
 * 
 * 实验平台:     慧勤智远 ESP32-P4 开发板
 ******************************************************************************
 */

#include "esp_rtc.h"


_calendar_obj calendar;         /* 时间结构体 */

/**
 * @brief       RTC设置时间
 * @param       year    :年
 * @param       mon     :月
 * @param       mday    :日
 * @param       hour    :时
 * @param       min     :分
 * @param       sec     :秒
 * @retval      无
 */
void rtc_set_time(int year, int mon, int mday, int hour, int min, int sec)
{
    struct tm time_set; /* 存放时间的各个量(年月日时分秒)结构体 */
    time_t second;      /* 存放时间的总秒数 */

    time_set.tm_year  = year - 1900;
    time_set.tm_mon   = mon - 1;
    time_set.tm_mday  = mday;
    time_set.tm_hour  = hour;
    time_set.tm_min   = min;
    time_set.tm_sec   = sec;
    time_set.tm_isdst = -1;

    second = mktime(&time_set);     /* 将时间转换为自1970年1月1日以来持续时间的秒数 */

    struct timeval val = { 
        .tv_sec = second, 
        .tv_usec = 0 
    };

    settimeofday(&val, NULL);       /* 设置当前时间 */
}

/**
 * @brief       获取当前的时间
 * @param       无
 * @retval      无
 */
void rtc_get_time(void)
{
    struct tm *time_block;  /* 存放时间的各个量(年月日时分秒)结构体 */
    time_t second;          /* 存放时间的总秒数 */

    time(&second);                      /* 获取当前的系统时间(返回自(1970.1.1零点零分UTC)经过的时间(秒)) */
    time_block = localtime(&second);    /* 从1970-1-1零点零分到当前时间系统所偏移的秒数时间转换为本地时间 */

    /* 公历年 月 日 星期 时 分 秒 */
    calendar.year  = time_block->tm_year + 1900;    /* 年(从1900开始) */
    calendar.month = time_block->tm_mon  + 1;       /* 月(从0(1月)到11(12月)) */
    calendar.date  = time_block->tm_mday;           /* 日(从1到31) */
    calendar.week  = time_block->tm_wday;           /* 星期几(从0(周日)到6(周六)) */
    calendar.hour  = time_block->tm_hour;           /* 时(从0到23) */
    calendar.min   = time_block->tm_min;            /* 分(从0到59) */
    calendar.sec   = time_block->tm_sec;            /* 秒(从0到59) */
}

