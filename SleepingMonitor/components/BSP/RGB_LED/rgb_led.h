/**
 * @file        rgb_led.h
 * @brief       RGB LED驱动模块
 * @note        支持多个RGB灯，每个RGB灯使用3个PWM通道
 *              GPIO: R=37, G=38, B=39
 */

#ifndef __RGB_LED_H
#define __RGB_LED_H

#include <stdint.h>
#include "driver/ledc.h"
#include "string.h"

/* RGB颜色结构体 */
typedef struct {
    uint8_t r;  /* 红色分量 0-255 */
    uint8_t g;  /* 绿色分量 0-255 */
    uint8_t b;  /* 蓝色分量 0-255 */
} rgb_color_t;

/* RGB LED配置结构体 */
typedef struct {
    int gpio_r;     /* 红色引脚 */
    int gpio_g;     /* 绿色引脚 */
    int gpio_b;     /* 蓝色引脚 */
    const char *name; /* LED名称 */
} rgb_led_config_t;

/* RGB LED句柄 */
typedef struct rgb_led_handle_t* rgb_led_handle_t;

/* 预定义颜色 */
extern const rgb_color_t RGB_RED;
extern const rgb_color_t RGB_GREEN;
extern const rgb_color_t RGB_BLUE;
extern const rgb_color_t RGB_YELLOW;
extern const rgb_color_t RGB_CYAN;
extern const rgb_color_t RGB_MAGENTA;
extern const rgb_color_t RGB_WHITE;
extern const rgb_color_t RGB_BLACK;
extern const rgb_color_t RGB_ORANGE;
extern const rgb_color_t RGB_PURPLE;

/* 黄昏光预设颜色 */
extern const rgb_color_t RGB_DUSK_START;      /* 黄昏开始 - 橙红色 */
extern const rgb_color_t RGB_DUSK_MIDDLE;     /* 黄昏中期 - 深橙色 */
extern const rgb_color_t RGB_DUSK_END;       /* 黄昏结束 - 暗红紫色 */
extern const rgb_color_t RGB_NIGHT;           /* 夜晚 - 深紫色 */
extern const rgb_color_t RGB_SUNRISE_START;  /* 黎明开始 - 暖黄色 */
extern const rgb_color_t RGB_SUNRISE_END;     /* 黎明结束 - 淡黄色 */

/**
 * @brief 初始化RGB LED驱动
 * @param ledc_timer_num LEDC定时器编号
 * @param ledc_speed_mode LEDC速度模式
 * @return ESP_OK成功，其他失败
 */
esp_err_t rgb_led_driver_init(ledc_timer_t ledc_timer_num, ledc_mode_t ledc_speed_mode);

/**
 * @brief 创建RGB LED实例
 * @param config RGB LED配置
 * @return RGB LED句柄，失败返回NULL
 */
rgb_led_handle_t rgb_led_create(const rgb_led_config_t *config);

/**
 * @brief 设置RGB LED颜色
 * @param handle RGB LED句柄
 * @param color 颜色值
 * @return ESP_OK成功，其他失败
 */
esp_err_t rgb_led_set_color(rgb_led_handle_t handle, rgb_color_t color);

/**
 * @brief 设置RGB LED亮度（缩放所有通道）
 * @param handle RGB LED句柄
 * @param brightness 亮度 0-255
 * @return ESP_OK成功，其他失败
 */
esp_err_t rgb_led_set_brightness(rgb_led_handle_t handle, uint8_t brightness);

/**
 * @brief 获取RGB LED当前颜色
 * @param handle RGB LED句柄
 * @param color 存储颜色的指针
 * @return ESP_OK成功，其他失败
 */
esp_err_t rgb_led_get_color(rgb_led_handle_t handle, rgb_color_t *color);

/**
 * @brief RGB LED呼吸效果（异步）
 * @param handle RGB LED句柄
 * @param target_color 目标颜色
 * @param duration_ms 渐变持续时间(ms)
 * @return ESP_OK成功，其他失败
 */
esp_err_t rgb_led_breath(rgb_led_handle_t handle, rgb_color_t target_color, uint32_t duration_ms);

/**
 * @brief RGB LED闪烁效果
 * @param handle RGB LED句柄
 * @param color 闪烁颜色
 * @param interval_ms 闪烁间隔(ms)
 * @param count 闪烁次数（0表示无限）
 * @return ESP_OK成功，其他失败
 */
esp_err_t rgb_led_blink(rgb_led_handle_t handle, rgb_color_t color, uint32_t interval_ms, uint32_t count);

/**
 * @brief 停止所有特效
 * @param handle RGB LED句柄
 * @return ESP_OK成功，其他失败
 */
esp_err_t rgb_led_stop_effect(rgb_led_handle_t handle);

/**
 * @brief 删除RGB LED实例
 * @param handle RGB LED句柄
 * @return ESP_OK成功，其他失败
 */
esp_err_t rgb_led_delete(rgb_led_handle_t handle);

/**
 * @brief 获取RGB LED数量
 * @return 已创建的RGB LED数量
 */
int rgb_led_get_count(void);

/**
 * @brief 根据名称查找RGB LED句柄
 * @param name LED名称
 * @return RGB LED句柄，失败返回NULL
 */
rgb_led_handle_t rgb_led_find_by_name(const char *name);

/* ========== 黄昏光效果函数 ========== */

/**
 * @brief RGB LED黄昏效果（渐变到指定颜色）
 * @param handle RGB LED句柄
 * @param target_color 目标颜色（黄昏色）
 * @param duration_ms 渐变持续时间(ms)
 * @return ESP_OK成功，其他失败
 */
esp_err_t rgb_led_dusk(rgb_led_handle_t handle, rgb_color_t target_color, uint32_t duration_ms);

/**
 * @brief RGB LED黄昏光模拟效果（多阶段渐变）
 * @param handle RGB LED句柄
 * @param duration_ms 总持续时间(ms)
 * @note 实现从黄昏橙红 -> 暗红紫 -> 夜晚深紫的渐变
 * @return ESP_OK成功，其他失败
 */
esp_err_t rgb_led_dusk_simulation(rgb_led_handle_t handle, uint32_t duration_ms);

/**
 * @brief 设置黄昏光效果
 * @param handle RGB LED句柄
 * @param brightness 亮度 0-255
 * @param dusk_level 黄昏程度 0-100
 *        0: 日落橙红
 *       50: 深橙红
 *      100: 夜晚深紫
 * @return ESP_OK成功
 */
esp_err_t rgb_led_set_dusk_light(rgb_led_handle_t handle, uint8_t brightness, uint8_t dusk_level);

/**
 * @brief 获取黄昏光的预设颜色
 * @param level 预设等级 0-3
 *        0: RGB_DUSK_START
 *        1: RGB_DUSK_MIDDLE
 *        2: RGB_DUSK_END
 *        3: RGB_NIGHT
 * @return 颜色值
 */
rgb_color_t rgb_led_get_dusk_preset(uint8_t level);

#endif /* __RGB_LED_H */
