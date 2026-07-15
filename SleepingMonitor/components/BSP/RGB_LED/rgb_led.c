/**
 * @file        rgb_led.c
 * @brief       RGB LED驱动模块 - ESP-IDF 5.x 兼容版本
 * @note        GPIO: R=15, G=16, B=17
 */

#include "rgb_led.h"
#include "esp_log.h"
#include "driver/ledc.h"
#include "esp_idf_version.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"

static const char *TAG = "RGB_LED";

/* RGB灯实例最大数量 */
#define RGB_LED_MAX_COUNT    4

/* 内部RGB LED结构体 */
typedef struct {
    rgb_led_config_t config;       /* 配置 */
    ledc_channel_t channel_r;       /* R通道 */
    ledc_channel_t channel_g;       /* G通道 */
    ledc_channel_t channel_b;       /* B通道 */
    rgb_color_t current_color;      /* 当前颜色 */
    uint8_t brightness;             /* 亮度 0-255 */
    TimerHandle_t breath_timer;      /* 呼吸效果定时器 */
    TimerHandle_t blink_timer;      /* 闪烁效果定时器 */
    uint32_t blink_count;           /* 闪烁计数 */
    uint32_t blink_interval;        /* 闪烁间隔 */
    rgb_color_t blink_color;        /* 闪烁颜色 */
    bool blink_state;               /* 闪烁状态 */
    bool is_active;                 /* 是否已激活 */
} rgb_led_instance_t;

/* 渐变效果数据 */
typedef struct {
    rgb_led_instance_t *led;
    rgb_color_t start_color;
    rgb_color_t end_color;
    uint32_t total_steps;
    uint32_t current_step;
    uint32_t step_delay_ms;
} breath_data_t;

/* 多阶段黄昏数据 */
typedef struct {
    rgb_led_instance_t *led;
    uint8_t phase;                 /* 0-3 */
    uint32_t phase_duration_ms;
    uint32_t elapsed_in_phase;
    uint32_t total_phases;
} dusk_phase_data_t;

/* 闪烁回调数据结构 */
typedef struct {
    rgb_led_instance_t *led;
    uint32_t remaining_count;
} blink_data_t;

/* 全局变量 */
static rgb_led_instance_t g_rgb_leds[RGB_LED_MAX_COUNT];
static int g_rgb_led_count = 0;
static bool g_driver_inited = false;
static ledc_timer_t g_ledc_timer;
static ledc_mode_t g_ledc_speed_mode;

/* 预定义颜色 */
const rgb_color_t RGB_RED     = {255, 0, 0};
const rgb_color_t RGB_GREEN   = {0, 255, 0};
const rgb_color_t RGB_BLUE    = {0, 0, 255};
const rgb_color_t RGB_YELLOW  = {255, 255, 0};
const rgb_color_t RGB_CYAN    = {0, 255, 255};
const rgb_color_t RGB_MAGENTA = {255, 0, 255};
const rgb_color_t RGB_WHITE   = {255, 255, 255};
const rgb_color_t RGB_BLACK   = {0, 0, 0};
const rgb_color_t RGB_ORANGE  = {255, 165, 0};
const rgb_color_t RGB_PURPLE  = {138, 43, 226};

/* 黄昏光颜色定义 */
const rgb_color_t RGB_DUSK_START      = {255, 140, 50};   /* 黄昏开始 - 橙红色 */
const rgb_color_t RGB_DUSK_MIDDLE     = {255, 100, 70};    /* 黄昏中期 - 深橙色 */
const rgb_color_t RGB_DUSK_END        = {180, 60, 80};      /* 黄昏结束 - 暗红紫色 */
const rgb_color_t RGB_NIGHT           = {50, 20, 40};       /* 夜晚 - 深紫色 */
const rgb_color_t RGB_SUNRISE_START   = {255, 180, 100};    /* 黎明开始 - 暖黄色 */
const rgb_color_t RGB_SUNRISE_END     = {255, 200, 150};    /* 黎明结束 - 淡黄色 */

/* ==================== 内部函数声明 ==================== */
static void breath_timer_callback(TimerHandle_t xTimer);
static void dusk_phase_timer_callback(TimerHandle_t xTimer);
static void blink_timer_callback(TimerHandle_t xTimer);
static void update_duty(rgb_led_instance_t *led);

/* ==================== 定时器回调函数 (C风格) ==================== */

/**
 * @brief       渐变效果定时器回调
 */
static void breath_timer_callback(TimerHandle_t xTimer)
{
    breath_data_t *data = (breath_data_t*)pvTimerGetTimerID(xTimer);
    
    if (data == NULL || data->led == NULL) {
        return;
    }
    
    data->current_step++;
    
    float t = (float)data->current_step / data->total_steps;
    if (t > 1.0f) t = 1.0f;
    
    /* 线性插值 */
    rgb_color_t color;
    color.r = data->start_color.r + (data->end_color.r - data->start_color.r) * t;
    color.g = data->start_color.g + (data->end_color.g - data->start_color.g) * t;
    color.b = data->start_color.b + (data->end_color.b - data->start_color.b) * t;
    
    rgb_led_set_color((rgb_led_handle_t)data->led, color);
    
    if (data->current_step >= data->total_steps) {
        xTimerStop(xTimer, 0);
        xTimerDelete(xTimer, 0);
        data->led->breath_timer = NULL;
        vPortFree(data);
    }
}

/**
 * @brief       黄昏多阶段定时器回调
 */
static void dusk_phase_timer_callback(TimerHandle_t xTimer)
{
    dusk_phase_data_t *data = (dusk_phase_data_t*)pvTimerGetTimerID(xTimer);
    
    if (data == NULL || data->led == NULL) {
        return;
    }
    
    data->elapsed_in_phase += 50;  /* 50ms per tick */
    
    if (data->elapsed_in_phase >= data->phase_duration_ms) {
        data->elapsed_in_phase = 0;
        data->phase++;
        
        if (data->phase >= data->total_phases) {
            /* 效果完成 */
            rgb_led_set_color((rgb_led_handle_t)data->led, RGB_NIGHT);
            xTimerStop(xTimer, 0);
            xTimerDelete(xTimer, 0);
            data->led->breath_timer = NULL;
            vPortFree(data);
            ESP_LOGI(TAG, "Dusk simulation completed");
            return;
        }
    }
    
    /* 各阶段的起始和结束颜色 */
    rgb_color_t start, end;
    float t = (float)data->elapsed_in_phase / data->phase_duration_ms;
    
    switch (data->phase) {
        case 0:  /* 橙红 -> 深橙 */
            start = RGB_DUSK_START;
            end = RGB_DUSK_MIDDLE;
            break;
        case 1:  /* 深橙 -> 暗红紫 */
            start = RGB_DUSK_MIDDLE;
            end = RGB_DUSK_END;
            break;
        case 2:  /* 暗红紫 -> 夜晚 */
            start = RGB_DUSK_END;
            end = RGB_NIGHT;
            break;
        default:
            start = RGB_NIGHT;
            end = RGB_NIGHT;
            break;
    }
    
    /* 线性插值 */
    rgb_color_t color;
    color.r = start.r + (end.r - start.r) * t;
    color.g = start.g + (end.g - start.g) * t;
    color.b = start.b+ (end.b - start.b) * t;
    
    rgb_led_set_color((rgb_led_handle_t)data->led, color);
}

/**
 * @brief       闪烁效果定时器回调
 */
static void blink_timer_callback(TimerHandle_t xTimer)
{
    blink_data_t *data = (blink_data_t*)pvTimerGetTimerID(xTimer);
    
    if (data == NULL || data->led == NULL) {
        return;
    }
    
    data->led->blink_state = !data->led->blink_state;
    
    if (data->led->blink_state) {
        rgb_led_set_color((rgb_led_handle_t)data->led, data->led->blink_color);
    } else {
        rgb_led_set_color((rgb_led_handle_t)data->led, RGB_BLACK);
    }
    
    if (data->remaining_count > 0) {
        data->remaining_count--;
        if (data->remaining_count <= 0) {
            rgb_led_set_color((rgb_led_handle_t)data->led, RGB_BLACK);
            xTimerStop(xTimer, 0);
            xTimerDelete(xTimer, 0);
            data->led->blink_timer = NULL;
            vPortFree(data);
        }
    }
}

/* ==================== 核心函数实现 ==================== */

/**
 * @brief       初始化RGB LED驱动
 */
esp_err_t rgb_led_driver_init(ledc_timer_t ledc_timer_num, ledc_mode_t ledc_speed_mode)
{
    if (g_driver_inited) {
        ESP_LOGW(TAG, "Driver already initialized");
        return ESP_OK;
    }

    g_ledc_timer = ledc_timer_num;
    g_ledc_speed_mode = ledc_speed_mode;

    /* 配置LEDC定时器 */
    ledc_timer_config_t timer_config = {
        .speed_mode = ledc_speed_mode,
        .timer_num = ledc_timer_num,
        .duty_resolution = LEDC_TIMER_8_BIT,  /* 8位分辨率，255级亮度 */
        .freq_hz = 1000,                      /* 1kHz频率 */
        .clk_cfg = LEDC_AUTO_CLK,
    };

    ESP_ERROR_CHECK(ledc_timer_config(&timer_config));

    /* 初始化所有实例 */
    memset(g_rgb_leds, 0, sizeof(g_rgb_leds));
    g_rgb_led_count = 0;
    g_driver_inited = true;

    ESP_LOGI(TAG, "RGB LED driver initialized (Timer: %d, Mode: %d)", ledc_timer_num, ledc_speed_mode);
    return ESP_OK;
}

/**
 * @brief       更新PWM占空比
 */
static void update_duty(rgb_led_instance_t *led)
{
    uint8_t r = (uint16_t)led->current_color.r * led->brightness / 255;
    uint8_t g = (uint16_t)led->current_color.g * led->brightness / 255;
    uint8_t b = (uint16_t)led->current_color.b * led->brightness / 255;

    ledc_set_duty(g_ledc_speed_mode, led->channel_r, r);
    ledc_set_duty(g_ledc_speed_mode, led->channel_g, g);
    ledc_set_duty(g_ledc_speed_mode, led->channel_b, b);

    ledc_update_duty(g_ledc_speed_mode, led->channel_r);
    ledc_update_duty(g_ledc_speed_mode, led->channel_g);
    ledc_update_duty(g_ledc_speed_mode, led->channel_b);
}

/**
 * @brief       创建RGB LED实例
 */
rgb_led_handle_t rgb_led_create(const rgb_led_config_t *config)
{
    if (!g_driver_inited) {
        ESP_LOGE(TAG, "Driver not initialized");
        return NULL;
    }

    if (g_rgb_led_count >= RGB_LED_MAX_COUNT) {
        ESP_LOGE(TAG, "Maximum RGB LED count reached");
        return NULL;
    }

    int index = g_rgb_led_count++;
    rgb_led_instance_t *led = &g_rgb_leds[index];

    /* 复制配置 */
    led->config = *config;
    led->brightness = 200;                 /* 默认亮度200 */
    led->current_color = RGB_DUSK_START;   /* 默认黄昏开始色（橙红） */
    led->is_active = true;
    led->breath_timer = NULL;
    led->blink_timer = NULL;
    led->blink_count = 0;
    led->blink_state = true;

    /* 计算LEDC通道编号 */
    ledc_channel_t base_channel = (ledc_channel_t)(index * 3);
    
    /* 配置R通道 */
    ledc_channel_config_t ch_r = {
        .speed_mode = g_ledc_speed_mode,
        .channel = base_channel,
        .duty = 0,
        .gpio_num = config->gpio_r,
        .timer_sel = g_ledc_timer,
        .hpoint = 0,
    };

    /* 配置G通道 */
    ledc_channel_config_t ch_g = {
        .speed_mode = g_ledc_speed_mode,
        .channel = (ledc_channel_t)(base_channel + 1),
        .duty = 0,
        .gpio_num = config->gpio_g,
        .timer_sel = g_ledc_timer,
        .hpoint = 0,
    };

    /* 配置B通道 */
    ledc_channel_config_t ch_b = {
        .speed_mode = g_ledc_speed_mode,
        .channel = (ledc_channel_t)(base_channel + 2),
        .duty = 0,
        .gpio_num = config->gpio_b,
        .timer_sel = g_ledc_timer,
        .hpoint = 0,
    };

    ESP_ERROR_CHECK(ledc_channel_config(&ch_r));
    ESP_ERROR_CHECK(ledc_channel_config(&ch_g));
    ESP_ERROR_CHECK(ledc_channel_config(&ch_b));

    led->channel_r = ch_r.channel;
    led->channel_g = ch_g.channel;
    led->channel_b = ch_b.channel;

    /* 初始关闭 */
    rgb_led_set_color((rgb_led_handle_t)led, RGB_BLACK);

    ESP_LOGI(TAG, "RGB LED created: %s (R=%d, G=%d, B=%d)", 
             config->name, config->gpio_r, config->gpio_g, config->gpio_b);

    return (rgb_led_handle_t)led;
}

/**
 * @brief       设置RGB LED颜色
 */
esp_err_t rgb_led_set_color(rgb_led_handle_t handle, rgb_color_t color)
{
    if (!handle || !((rgb_led_instance_t*)handle)->is_active) {
        return ESP_ERR_INVALID_ARG;
    }

    rgb_led_instance_t *led = (rgb_led_instance_t*)handle;
    led->current_color = color;
    update_duty(led);

    return ESP_OK;
}

/**
 * @brief       设置RGB LED亮度
 */
esp_err_t rgb_led_set_brightness(rgb_led_handle_t handle, uint8_t brightness)
{
    if (!handle || !((rgb_led_instance_t*)handle)->is_active) {
        return ESP_ERR_INVALID_ARG;
    }

    rgb_led_instance_t *led = (rgb_led_instance_t*)handle;
    led->brightness = brightness;
    update_duty(led);

    return ESP_OK;
}

/**
 * @brief       获取RGB LED当前颜色
 */
esp_err_t rgb_led_get_color(rgb_led_handle_t handle, rgb_color_t *color)
{
    if (!handle || !color || !((rgb_led_instance_t*)handle)->is_active) {
        return ESP_ERR_INVALID_ARG;
    }

    rgb_led_instance_t *led = (rgb_led_instance_t*)handle;
    *color = led->current_color;

    return ESP_OK;
}

/**
 * @brief       RGB LED黄昏效果（渐变到指定颜色）
 */
esp_err_t rgb_led_dusk(rgb_led_handle_t handle, rgb_color_t target_color, uint32_t duration_ms)
{
    if (!handle || !((rgb_led_instance_t*)handle)->is_active) {
        return ESP_ERR_INVALID_ARG;
    }

    rgb_led_instance_t *led = (rgb_led_instance_t*)handle;
    
    /* 停止之前的特效 */
    rgb_led_stop_effect(handle);

    /* 计算渐变步数 */
    uint32_t step_delay = 50;  /* 50ms per step */
    uint32_t steps = duration_ms / step_delay;
    if (steps < 1) steps = 1;

    /* 分配渐变数据 */
    breath_data_t *data = (breath_data_t*)pvPortMalloc(sizeof(breath_data_t));
    if (data == NULL) {
        ESP_LOGE(TAG, "Failed to allocate breath data");
        return ESP_ERR_NO_MEM;
    }

    data->led = led;
    data->start_color = led->current_color;
    data->end_color = target_color;
    data->total_steps = steps;
    data->current_step = 0;
    data->step_delay_ms = step_delay;

    /* 创建定时器 */
    led->breath_timer = xTimerCreate(
        "dusk_timer",
        pdMS_TO_TICKS(step_delay),
        pdTRUE,  /* 周期定时 */
        data,
        breath_timer_callback
    );

    if (led->breath_timer) {
        xTimerStart(led->breath_timer, 0);
        ESP_LOGI(TAG, "Dusk effect started, duration: %ums", duration_ms);
    } else {
        vPortFree(data);
        return ESP_FAIL;
    }

    return ESP_OK;
}

/**
 * @brief       RGB LED黄昏光模拟效果（多阶段渐变）
 */
esp_err_t rgb_led_dusk_simulation(rgb_led_handle_t handle, uint32_t duration_ms)
{
    if (!handle || !((rgb_led_instance_t*)handle)->is_active) {
        return ESP_ERR_INVALID_ARG;
    }

    rgb_led_instance_t *led = (rgb_led_instance_t*)handle;
    
    /* 停止之前的特效 */
    rgb_led_stop_effect(handle);

    /* 计算各阶段时间 (3个渐变阶段 + 1个保持) */
    uint32_t phase_duration = duration_ms / 4;

    /* 分配数据 */
    dusk_phase_data_t *data = (dusk_phase_data_t*)pvPortMalloc(sizeof(dusk_phase_data_t));
    if (data == NULL) {
        ESP_LOGE(TAG, "Failed to allocate dusk data");
        return ESP_ERR_NO_MEM;
    }

    data->led = led;
    data->phase = 0;
    data->phase_duration_ms = phase_duration;
    data->elapsed_in_phase = 0;
    data->total_phases = 4;

    /* 创建定时器 */
    led->breath_timer = xTimerCreate(
        "dusk_simulation",
        pdMS_TO_TICKS(50),  /* 50ms per tick */
        pdTRUE,
        data,
        dusk_phase_timer_callback
    );

    if (led->breath_timer) {
        xTimerStart(led->breath_timer, 0);
        ESP_LOGI(TAG, "Dusk simulation started, total duration: %ums", duration_ms);
    } else {
        vPortFree(data);
        return ESP_FAIL;
    }

    return ESP_OK;
}

/**
 * @brief       RGB LED呼吸效果
 */
esp_err_t rgb_led_breath(rgb_led_handle_t handle, rgb_color_t target_color, uint32_t duration_ms)
{
    return rgb_led_dusk(handle, target_color, duration_ms);
}

/**
 * @brief       RGB LED闪烁效果
 */
esp_err_t rgb_led_blink(rgb_led_handle_t handle, rgb_color_t color, uint32_t interval_ms, uint32_t count)
{
    if (!handle || !((rgb_led_instance_t*)handle)->is_active) {
        return ESP_ERR_INVALID_ARG;
    }

    rgb_led_instance_t *led = (rgb_led_instance_t*)handle;

    /* 停止之前的特效 */
    rgb_led_stop_effect(handle);

    led->blink_color = color;
    led->blink_interval = interval_ms;
    led->blink_count = count;
    led->blink_state = true;

    /* 分配数据 */
    blink_data_t *data = (blink_data_t*)pvPortMalloc(sizeof(blink_data_t));
    if (data == NULL) {
        ESP_LOGE(TAG, "Failed to allocate blink data");
        return ESP_ERR_NO_MEM;
    }

    data->led = led;
    data->remaining_count = count * 2;  /* 每个闪烁周期需要2次切换 */

    led->blink_timer = xTimerCreate(
        "blink_timer",
        pdMS_TO_TICKS(interval_ms),
        pdTRUE,
        data,
        blink_timer_callback
    );

    if (led->blink_timer) {
        /* 立即开始闪烁 */
        rgb_led_set_color(handle, color);
        xTimerStart(led->blink_timer, 0);
    } else {
        vPortFree(data);
        return ESP_FAIL;
    }

    return ESP_OK;
}

/**
 * @brief       停止所有特效
 */
esp_err_t rgb_led_stop_effect(rgb_led_handle_t handle)
{
    if (!handle || !((rgb_led_instance_t*)handle)->is_active) {
        return ESP_ERR_INVALID_ARG;
    }

    rgb_led_instance_t *led = (rgb_led_instance_t*)handle;

    if (led->breath_timer) {
        xTimerStop(led->breath_timer, 0);
        xTimerDelete(led->breath_timer, 0);
        led->breath_timer = NULL;
    }

    if (led->blink_timer) {
        xTimerStop(led->blink_timer, 0);
        xTimerDelete(led->blink_timer, 0);
        led->blink_timer = NULL;
    }

    return ESP_OK;
}

/**
 * @brief       删除RGB LED实例
 */
esp_err_t rgb_led_delete(rgb_led_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }

    rgb_led_instance_t *led = (rgb_led_instance_t*)handle;
    
    rgb_led_stop_effect(handle);
    rgb_led_set_color(handle, RGB_BLACK);
    led->is_active = false;

    return ESP_OK;
}

/**
 * @brief       获取RGB LED数量
 */
int rgb_led_get_count(void)
{
    return g_rgb_led_count;
}

/**
 * @brief       根据名称查找RGB LED句柄
 */
rgb_led_handle_t rgb_led_find_by_name(const char *name)
{
    for (int i = 0; i < g_rgb_led_count; i++) {
        if (g_rgb_leds[i].is_active && 
            strcmp(g_rgb_leds[i].config.name, name) == 0) {
            return (rgb_led_handle_t)&g_rgb_leds[i];
        }
    }
    return NULL;
}

/* ==================== 黄昏光便捷函数 ==================== */

/**
 * @brief       设置黄昏光效果
 */
esp_err_t rgb_led_set_dusk_light(rgb_led_handle_t handle, uint8_t brightness, uint8_t dusk_level)
{
    rgb_color_t dusk_color;
    
    /* 根据黄昏程度计算颜色 */
    if (dusk_level <= 33) {
        /* 早期黄昏 - 暖橙色 */
        float t = dusk_level / 33.0f;
        dusk_color.r = RGB_DUSK_START.r + (RGB_DUSK_MIDDLE.r - RGB_DUSK_START.r) * t;
        dusk_color.g = RGB_DUSK_START.g + (RGB_DUSK_MIDDLE.g - RGB_DUSK_START.g) * t;
        dusk_color.b = RGB_DUSK_START.b + (RGB_DUSK_MIDDLE.b - RGB_DUSK_START.b) * t;
    } else if (dusk_level <= 66) {
        /* 中期黄昏 - 深橙红 */
        float t = (dusk_level - 33) / 33.0f;
        dusk_color.r = RGB_DUSK_MIDDLE.r + (RGB_DUSK_END.r - RGB_DUSK_MIDDLE.r) * t;
        dusk_color.g = RGB_DUSK_MIDDLE.g + (RGB_DUSK_END.g - RGB_DUSK_MIDDLE.g) * t;
        dusk_color.b = RGB_DUSK_MIDDLE.b + (RGB_DUSK_END.b - RGB_DUSK_MIDDLE.b) * t;
    } else {
        /* 晚期黄昏 - 暗红紫到夜晚 */
        float t = (dusk_level - 66) / 34.0f;
        dusk_color.r = RGB_DUSK_END.r + (RGB_NIGHT.r - RGB_DUSK_END.r) * t;
        dusk_color.g = RGB_DUSK_END.g + (RGB_NIGHT.g - RGB_DUSK_END.g) * t;
        dusk_color.b = RGB_DUSK_END.b + (RGB_NIGHT.b - RGB_DUSK_END.b) * t;
    }
    
    /* 设置亮度 */
    rgb_led_set_brightness(handle, brightness);
    
    /* 设置颜色 */
    return rgb_led_set_color(handle, dusk_color);
}

/**
 * @brief       获取黄昏光的预设颜色
 */
rgb_color_t rgb_led_get_dusk_preset(uint8_t level)
{
    switch (level) {
        case 0: return RGB_DUSK_START;
        case 1: return RGB_DUSK_MIDDLE;
        case 2: return RGB_DUSK_END;
        case 3: return RGB_NIGHT;
        default: return RGB_DUSK_MIDDLE;
    }
}
