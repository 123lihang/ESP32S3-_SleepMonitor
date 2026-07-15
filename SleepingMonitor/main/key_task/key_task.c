/**
 * @file        key_task.c
 * @brief       独立的按键任务，用于检测长按重新配网
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "key.h"
#include "wifi_ble.h"
#include "lcd.h"
#include "lwip_demo.h"
#include "xl9555.h"

static const char *TAG = "KEY_TASK";
static TaskHandle_t key_task_handle = NULL;

/**
 * @brief       发送全部模拟数据（表格中的所有19种数据类型）
 */
void send_all_mock_data(void)
{
    if (client == NULL || g_publish_flag != 1) {
        ESP_LOGI("MOCK", "MQTT not connected, cannot send!");
        return;
    }
    
    ESP_LOGI("MOCK", "Sending all mock radar data...");
    
    // 1. 有人/无人状态
    esp_mqtt_client_publish(client, DEVICE_PUBLISH, "{\"type\":\"presence\",\"val\":1}", 0, 0, 0);
    vTaskDelay(pdMS_TO_TICKS(50));
    
    // 2. 运动状态
    esp_mqtt_client_publish(client, DEVICE_PUBLISH, "{\"type\":\"motion\",\"val\":2}", 0, 0, 0);
    vTaskDelay(pdMS_TO_TICKS(50));
    
    // 3. 呼吸频率
    esp_mqtt_client_publish(client, DEVICE_PUBLISH, "{\"type\":\"breath\",\"val\":20}", 0, 0, 0);
    vTaskDelay(pdMS_TO_TICKS(50));
    
    // 4. 心率
    esp_mqtt_client_publish(client, DEVICE_PUBLISH, "{\"type\":\"heart\",\"val\":75}", 0, 0, 0);
    vTaskDelay(pdMS_TO_TICKS(50));
    
    // 5. 睡眠阶段状态
    esp_mqtt_client_publish(client, DEVICE_PUBLISH, "{\"type\":\"sleep\",\"val\":1}", 0, 0, 0);
    vTaskDelay(pdMS_TO_TICKS(50));
    
    // 6. 睡眠质量评分
    esp_mqtt_client_publish(client, DEVICE_PUBLISH, "{\"type\":\"sleep_score\",\"val\":85}", 0, 0, 0);
    vTaskDelay(pdMS_TO_TICKS(50));
    
    // 7. 人体方位
    esp_mqtt_client_publish(client, DEVICE_PUBLISH, "{\"type\":\"pos\",\"x\":20,\"y\":5,\"z\":0}", 0, 0, 0);
    vTaskDelay(pdMS_TO_TICKS(50));
    
    // 8. 人体与雷达距离
    esp_mqtt_client_publish(client, DEVICE_PUBLISH, "{\"type\":\"distance\",\"val\":150}", 0, 0, 0);
    vTaskDelay(pdMS_TO_TICKS(50));
    
    // 9. 体动幅度
    esp_mqtt_client_publish(client, DEVICE_PUBLISH, "{\"type\":\"body_move\",\"val\":35}", 0, 0, 0);
    vTaskDelay(pdMS_TO_TICKS(50));
    
    // 10. 在床/离床状态
    esp_mqtt_client_publish(client, DEVICE_PUBLISH, "{\"type\":\"bed\",\"val\":1}", 0, 0, 0);
    vTaskDelay(pdMS_TO_TICKS(50));
    
    // 11. 睡眠异常标识
    esp_mqtt_client_publish(client, DEVICE_PUBLISH, "{\"type\":\"sleep_abnormal\",\"val\":3}", 0, 0, 0);
    vTaskDelay(pdMS_TO_TICKS(50));
    
    // 12. 无人检测超时
    esp_mqtt_client_publish(client, DEVICE_PUBLISH, "{\"type\":\"nopeople\",\"val\":0}", 0, 0, 0);
    vTaskDelay(pdMS_TO_TICKS(50));
    
    // 13. 清醒时长
    esp_mqtt_client_publish(client, DEVICE_PUBLISH, "{\"type\":\"awake_time\",\"val\":0}", 0, 0, 0);
    vTaskDelay(pdMS_TO_TICKS(50));
    
    // 14. 浅睡时长
    esp_mqtt_client_publish(client, DEVICE_PUBLISH, "{\"type\":\"light_sleep_time\",\"val\":0}", 0, 0, 0);
    vTaskDelay(pdMS_TO_TICKS(50));
    
    // 15. 深睡时长
    esp_mqtt_client_publish(client, DEVICE_PUBLISH, "{\"type\":\"deep_sleep_time\",\"val\":0}", 0, 0, 0);
    vTaskDelay(pdMS_TO_TICKS(50));
    
    // 16. 呼吸状态
    esp_mqtt_client_publish(client, DEVICE_PUBLISH, "{\"type\":\"last_breathe_state\",\"val\":1}", 0, 0, 0);
    vTaskDelay(pdMS_TO_TICKS(50));
    
    // 17. 异常挣扎状态
    esp_mqtt_client_publish(client, DEVICE_PUBLISH, "{\"type\":\"last_struggle\",\"val\":0}", 0, 0, 0);
    vTaskDelay(pdMS_TO_TICKS(50));
    
    // 18. 无人计时状态
    esp_mqtt_client_publish(client, DEVICE_PUBLISH, "{\"type\":\"last_no_people\",\"val\":0}", 0, 0, 0);
    vTaskDelay(pdMS_TO_TICKS(50));
    
    ESP_LOGI("MOCK", "All 19 mock data sent!");
}

/**
 * @brief       按键检测任务函数
 */
static void key_check_task(void *pvParameters)
{
    uint8_t key;
    
    while (1) {
        key = xl9555_key_scan(0);  /* 扫描按键 */

        if (key == KEY0_PRES) {
            wifi_ble_reset_and_reprovision();
        }
		else if (key == KEY1_PRES) {
            /* KEY1按键按下 - 发送雷达数据 */
            send_all_mock_data();
        }
        
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

/**
 * @brief       初始化按键任务
 */
void key_task_init(void)
{
    xTaskCreate(key_check_task, "key_check", 4096, NULL, 10, &key_task_handle);
    ESP_LOGI(TAG, "Key task created, long press 3s to reset provisioning, KEY1: send radar data");
}