/**
 ****************************************************************************************************
* @file        main.c
* @author      正点原子团队(ALIENTEK)
* @version     V1.0
* @date        2023-08-26
* @brief       Sleeping Monitor - 睡眠监测系统
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "led.h"
#include "lcd.h"
#include "wifi_ble.h"
#include "lwip_demo.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "usart.h"
#include "rgb_led.h"
#include "key_task.h"
#include "key.h"

static const char *TAG = "MAIN";

i2c_obj_t i2c0_master;
rgb_led_handle_t rgb_led_handle = NULL;  /* RGB LED句柄 */

char display_buffer[64];

/* 前向声明 */
void dusk_gradient_task(void *pvParameters);

/**
 * @brief       程序入口
 * @param       无
 * @retval      无
 */
void app_main(void)
{
    esp_err_t ret;

    ret = nvs_flash_init();             /* 初始化NVS */

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    led_init();                         /* 初始化LED */
    i2c0_master = iic_init(I2C_NUM_0);  /* 初始化IIC0 */
    spi2_init();                        /* 初始化SPI2 */
    xl9555_init(i2c0_master);           /* IO扩展芯片初始化 */
    lcd_init();                         /* 初始化LCD */

    lcd_show_string(5, 10, 240, 24, 24, "Sleeping Monitor", RED);

    /* ========== RGB LED黄昏光初始化 ========== */
    rgb_led_config_t rgb_config = {
        .gpio_r = 10,   /* R */
        .gpio_g = 9,   /* G */
        .gpio_b = 38,   /* B */
        .name = "dusk_led"
    };
    
    ret = rgb_led_driver_init(LEDC_TIMER_2, LEDC_LOW_SPEED_MODE);
    if (ret == ESP_OK) {
        rgb_led_handle = rgb_led_create(&rgb_config);
        if (rgb_led_handle) {
            // 直接设置纯黄色！
            rgb_led_set_color(rgb_led_handle, RGB_YELLOW);
            // 亮度拉满，更亮
            rgb_led_set_brightness(rgb_led_handle, 255);
            ESP_LOGI(TAG, "RGB LED 黄色已点亮");
        } else {
            ESP_LOGW(TAG, "RGB LED create failed");
        }
    } else {
        ESP_LOGW(TAG, "RGB LED driver init failed");
    }

	key_init();                       /* 初始化按键 */
	key_task_init(); 				/* 创建按键检测任务 */

    /* ========== 雷达初始化 ========== */
    usart_init(115200);                 /* 初始化雷达串口，波特率115200 */
	
	wifi_ble_prov_init();                    /* 蓝牙配网设置 */
    lwip_demo();                        /* MQTT初始化 */

	/* 创建雷达数据采集任务 */
    xTaskCreate(radar_data_parse_task, "radar_parse_task", 4096, NULL, 5, NULL);

	/* 创建雷达数据显示任务 */
    xTaskCreate(radar_display_task, "radar_display_task", 4096, NULL, 4, NULL);

	/* 创建雷达数据发送给mqtt任务 */
	xTaskCreate(mqtt_send_task, "mqtt_send_task", 4096, NULL, 3, NULL);

    /* 创建黄昏光渐变任务 */
    //xTaskCreate(dusk_gradient_task, "dusk_task", 4096, NULL, 2, NULL);


    ESP_LOGI(TAG, "System initialized successfully");
}    

/**
 * @brief       黄昏光渐变任务
 * @param       pvParameters 传入参数
 * @note        每小时执行一次黄昏到夜晚的渐变效果
 */
void dusk_gradient_task(void *pvParameters)
{
    while (1) {
        /* 等待一个小时 (可改为更短时间用于测试) */
        vTaskDelay(pdMS_TO_TICKS(60 * 60 * 1000));  /* 1小时 */
        
        if (rgb_led_handle) {
            /* 执行黄昏模拟效果 - 30秒内从橙红渐变到夜晚 */
            rgb_led_dusk_simulation(rgb_led_handle, 30000);
            ESP_LOGI(TAG, "Dusk simulation triggered");
        }
    }
}
