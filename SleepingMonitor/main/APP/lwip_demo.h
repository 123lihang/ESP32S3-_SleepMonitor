/**
 ****************************************************************************************************
 * @file        udp.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2023-08-26
 * @brief       LWIP实验
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

#ifndef __LWIP_DEMO_H
#define __LWIP_DEMO_H

#include <string.h>
#include <sys/socket.h>
#include "freertos/FreeRTOS.h"

#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
#include "mqtt_client.h"
#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "esp_netif.h"
#include "led.h"
#include "lcd.h"
#include "usart.h"
#include "wifi_ble.h"

extern esp_mqtt_client_handle_t client;
extern int g_publish_flag;
extern char g_lcd_buff[100];

/* EMQX Broker配置 - 使用公共 broker.emqx.io (非SSL) */
#define HOST_NAME           "broker.emqx.io"
#define HOST_PORT           1883
#define CLIENT_ID           "esp32_radar_client"
#define USER_NAME           ""
#define PASSWORD            ""

/* 发布与订阅主题 */
#define DEVICE_PUBLISH      "radar/data"
#define DEVICE_SUBSCRIBE    "radar/cmd"

void lwip_demo(void);
void punish_radar_data(void);
void mqtt_send_task(void *pvParameters);

#endif
