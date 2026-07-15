/**
 ****************************************************************************************************
 * @file        usart.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V2.0
 * @date        2024-01-20
 * @brief       串口初始化代码(一般是串口0) - 增强版雷达协议支持
 * @license     Copyright (c) 2020-2032, 广州市星翼电子科技有限公司
 ****************************************************************************************************
 * @attention
 *
 * 实验平台:正点原子 ESP32-S3 开发板
 * 在线视频:www.yuanzige.com
 * 技术论坛:www.openedv.com
 * 公司网址:www.alientek.com
 * 购买地址:openedv.taobao.com
 * 
 ****************************************************************************************************
 */

#ifndef _USART_H
#define _USART_H

#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/uart.h"
#include "driver/uart_select.h"
#include "driver/gpio.h"
#include "lcd.h"

/* 引脚和串口定义 */
#define USART_UX            UART_NUM_1
#define USART_TX_GPIO_PIN   GPIO_NUM_4
#define USART_RX_GPIO_PIN   GPIO_NUM_5

/* 串口接收相关定义 */
#define RX_BUF_SIZE         1024    /* 环形缓冲区大小 */

/* 雷达协议相关定义 */
#define FRAME_HEADER1       0x53    /* 帧头1 'S' */
#define FRAME_HEADER2       0x59    /* 帧头2 'Y' */
#define FRAME_TAIL1         0x54    /* 帧尾1 'T' */
#define FRAME_TAIL2         0x43    /* 帧尾2 'C' */

/* 系统功能控制字 */
#define CTRL_WORD_HEARTBEAT     0x01    /* 心跳包 */
#define CTRL_WORD_PRODUCT_INFO  0x02    /* 产品信息 */
#define CTRL_WORD_OTA           0x03    /* OTA升级 */
#define CTRL_WORD_WORK_STATUS   0x05    /* 工作状态 */
#define CTRL_WORD_RANGE_INFO    0x07    /* 雷达探测范围信息 */

/* 人体存在监测控制字 */
#define CTRL_WORD_PRESENCE      0x80    /* 人体存在 */
#define CTRL_WORD_BREATH        0x81    /* 呼吸检测 */
#define CTRL_WORD_DISTANCE      0x82    /* 人体距离 */
#define CTRL_WORD_MOTION        0x83    /* 体动幅度 */
#define CTRL_WORD_SLEEP         0x84    /* 睡眠监测 */
#define CTRL_WORD_HEART_RATE    0x85    /* 心率监测 */

/* 命令字定义 - 人体存在 */
#define CMD_PRESENCE_SWITCH     0x00    /* 人体存在开关 */
#define CMD_PRESENCE_STATUS     0x01    /* 存在状态 */
#define CMD_MOTION_STATUS       0x02    /* 运动状态 */
#define CMD_MOTION_PARAM        0x03    /* 体动参数 */
#define CMD_DISTANCE            0x04    /* 人体距离 */
#define CMD_POSITION            0x05    /* 人体方位 */

/* 命令字定义 - 呼吸监测 */
#define CMD_BREATH_SWITCH       0x00    /* 呼吸开关 */
#define CMD_BREATH_INFO         0x01    /* 呼吸信息 */
#define CMD_BREATH_VALUE        0x02    /* 呼吸数值 */
#define CMD_BREATH_WAVE         0x05    /* 呼吸波形 */

/* 命令字定义 - 心率监测 */
#define CMD_HEART_SWITCH        0x00    /* 心率开关 */
#define CMD_HEART_VALUE         0x02    /* 心率数值 */
#define CMD_HEART_WAVE          0x05    /* 心率波形 */

/* 命令字定义 - 睡眠监测 */
#define CMD_SLEEP_SWITCH        0x00    /* 睡眠开关 */
#define CMD_SLEEP_MODE          0x0F    /* 上报模式 */
#define CMD_SLEEP_STATUS        0x02    /* 睡眠状态 */
#define CMD_AWAKE_TIME          0x03    /* 清醒时长 */
#define CMD_LIGHT_SLEEP_TIME    0x04    /* 浅睡时长 */
#define CMD_DEEP_SLEEP_TIME     0x05    /* 深睡时长 */
#define CMD_SLEEP_SCORE         0x06    /* 睡眠评分 */
#define CMD_SLEEP_COMPRE        0x0C    /* 睡眠综合状态 */
#define CMD_SLEEP_ANALYSIS      0x0D    /* 睡眠质量分析 */
#define CMD_SLEEP_ABNORMAL      0x0E    /* 睡眠异常 */
#define CMD_SLEEP_RATING        0x10    /* 睡眠质量评级 */
#define CMD_STRUGGLE_STATUS     0x11    /* 异常挣扎状态 */
#define CMD_NOPEOPLE_TIMER      0x12    /* 无人计时状态 */

/* 雷达数据结构体 - 用于队列传递 */
typedef struct {
    uint8_t data_type;          /* 数据类型 (对应控制字+命令字组合) */
    union {
        uint8_t presence;       /* 存在状态: 0-无人, 1-有人 */
        uint8_t breath_rate;    /* 呼吸频率: 0-35次/分钟 */
        uint8_t heart_rate;     /* 心率: 60-120次/分钟 */
        uint8_t sleep_state;    /* 睡眠状态: 0-深睡,1-浅睡,2-清醒,3-离床 */
        uint16_t distance;      /* 人体距离: cm */
        uint8_t motion_amp;     /* 体动幅度: 0-100 */
        uint8_t motion_state;   /* 运动状态: 0-无,1-静止,2-活跃 */
        struct {
            int16_t x;
            int16_t y;
            int16_t z;
        } position;             /* 人体方位: cm, 有正负 */
        uint16_t awake_time;    /* 清醒时长: 分钟 */
        uint16_t light_sleep_time;  /* 浅睡时长: 分钟 */
        uint16_t deep_sleep_time;   /* 深睡时长: 分钟 */
        uint8_t sleep_score;    /* 睡眠评分: 0-100 */
        uint8_t abnormal_type;  /* 异常类型 */
        uint8_t struggle_status;/* 挣扎状态: 0-无,1-正常,2-异常 */
        uint8_t nopeople_status;/* 无人计时: 0-无,1-正常,2-异常 */
        uint8_t breath_info;    /* 呼吸信息: 0-正常,1-过高,2-过低,3-无 */
        uint8_t bed_state;      /* 入床/离床: 0-离床,1-入床 */
        uint8_t sleep_rating;   /* 睡眠评级: 0-无,1-良好,2-一般,3-较差 */
    } data;
    uint32_t timestamp;         /* 时间戳 */
} radar_data_t;

/* 睡眠综合状态结构体 */
typedef struct {
    uint8_t presence;           /* 存在: 0-无人, 1-有人 */
    uint8_t sleep_state;        /* 睡眠状态: 3-离床,2-清醒,1-浅睡,0-深睡 */
    uint8_t avg_breath;         /* 平均呼吸: 0-25 */
    uint8_t avg_heart;          /* 平均心跳: 0-100 */
    uint8_t turn_over_count;    /* 翻身次数: 0-255 */
    uint8_t large_motion_ratio; /* 大幅度体动占比: 0-100 */
    uint8_t small_motion_ratio; /* 小幅度体动占比: 0-100 */
    uint8_t apnea_count;        /* 呼吸暂停次数: 0-10 */
} sleep_compre_t;

/* 睡眠质量分析结构体 */
typedef struct {
    uint8_t score;              /* 睡眠质量评分: 0-100 */
    uint16_t total_time;        /* 睡眠总时长: 分钟 */
    uint8_t awake_ratio;        /* 清醒时长占比: 0-100 */
    uint8_t light_sleep_ratio;  /* 浅睡时长占比: 0-100 */
    uint8_t deep_sleep_ratio;   /* 深睡时长占比: 0-100 */
    uint8_t out_bed_time;       /* 离床时长: 0-255 */
    uint8_t out_bed_count;      /* 离床次数: 0-255 */
    uint8_t turn_over_count;    /* 翻身次数: 0-255 */
    uint8_t avg_breath;         /* 平均呼吸: 0-25 */
    uint8_t avg_heart;          /* 平均心跳: 0-100 */
    uint8_t apnea_count;        /* 呼吸暂停次数: 0-10 */
} sleep_analysis_t;

/* 函数声明 */
void usart_init(uint32_t baudrate);                         /* 初始化串口 */
void radar_data_parse_task(void *pvParameters);             /* 雷达数据解析任务 */
QueueHandle_t get_radar_data_queue(void);                   /* 获取雷达数据队列句柄 */
QueueHandle_t get_mqtt_data_queue(void); 				  /* 获取MQTT数据队列句柄 */
void radar_display_task(void *pvParameters);                 /* 雷达数据展示任务 */

#endif