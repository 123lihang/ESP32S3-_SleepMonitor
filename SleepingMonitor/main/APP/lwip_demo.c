/**
 ****************************************************************************************************
 * @file        lwip_demo.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2023-08-26
 * @brief       MQTT客户端 - 使用公共 EMQX Broker
 ****************************************************************************************************
 */

#include "lwip_demo.h"

int g_publish_flag = 0;
static const char *TAG = "MQTT_EXAMPLE";
char g_lcd_buff[100] = {0};

esp_mqtt_client_handle_t client = NULL;

/**
 * @brief       错误日记
 */
static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0)
    {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

/**
 * @brief       MQTT事件处理程序
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;

    switch ((esp_mqtt_event_id_t)event_id)
    {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            g_publish_flag = 1;
            
            /* 订阅命令主题 */
            msg_id = esp_mqtt_client_subscribe(client, DEVICE_SUBSCRIBE, 0);
            ESP_LOGI(TAG, "subscribed to %s, msg_id=%d", DEVICE_SUBSCRIBE, msg_id);
            
            lcd_show_string(5, 80, 200, 16, 16, "MQTT Connected!", GREEN);
            break;
            
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            g_publish_flag = 0;
            lcd_show_string(5, 80, 200, 16, 16, "MQTT Disconnected!", RED);
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_DATA:
            ESP_LOGI("MQTT", "Received data on topic %.*s", event->topic_len, event->topic);
            
            if (strncmp(event->topic, DEVICE_SUBSCRIBE, event->topic_len) == 0) {
				char *data = malloc(event->data_len + 1);
				if (data) {
					memcpy(data, event->data, event->data_len);
					data[event->data_len] = '\0';
					ESP_LOGI(TAG, "msg: %s", data);
					if (strstr(data, "1") != NULL) {
						gpio_set_level(LED_GPIO_PIN, 0);  // LED亮
						ESP_LOGI(TAG, "LED已开启");
					} else if (strstr(data, "0") != NULL) {
						gpio_set_level(LED_GPIO_PIN, 1);  // LED灭
						ESP_LOGI(TAG, "LED已关闭");
					}
					free(data);
				}
			}
            break;

        case MQTT_EVENT_ERROR:
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT)
            {
                log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
                log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
                log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
                ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
            }
            break;

        default:
            ESP_LOGI(TAG, "Other event id:%d", event->event_id);
            break;
    }
}

/**
 * @brief       MQTT初始化
 */
void lwip_demo(void)
{
    /* MQTT配置 - 使用TCP非SSL连接 */
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.hostname = HOST_NAME,
        .broker.address.port = HOST_PORT,
        .broker.address.transport = MQTT_TRANSPORT_OVER_TCP,
        .credentials.client_id = CLIENT_ID,
        .credentials.username = USER_NAME,
        .credentials.authentication.password = PASSWORD,
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);

    vTaskDelay(pdMS_TO_TICKS(1000));
}

/* ==================== 雷达数据MQTT发送 ==================== */

static int16_t last_breath = -1;
static int16_t last_heart = -1;
static int16_t last_presence = -1;
static int16_t last_sleep_score = -1;
static int16_t last_distance = -1;
static int16_t last_motion = -1;
static int16_t last_bed = -1;
static int16_t last_sleep_state = -1;
static int16_t last_motion_amp = -1;
static int16_t last_abnormal = -1;
static int16_t last_x = -1, last_y = -1, last_z = -1;

/**
 * @brief       处理并发送雷达数据到MQTT
 */
void punish_radar_data(void)
{
    radar_data_t radar_data = {0};
    QueueHandle_t queue = get_mqtt_data_queue();
    
    if (queue == NULL || client == NULL) {
        return;
    }

    if (g_publish_flag != 1) {
        return;
    }

    radar_data_t latest_data = {0};
    bool has_data = false;

    /* 非阻塞读取队列，只处理最新数据 */
    while (xQueueReceive(queue, &radar_data, 0) == pdTRUE) {
        has_data = true;
        latest_data = radar_data;
    }
    
    if (!has_data) {
        return;
    }

    char json_buffer[128] = {0};
    bool should_send = false;
    
    switch (latest_data.data_type) {
        case 0x01:  /* 有人/无人状态 */
            if (latest_data.data.presence != last_presence) {
                snprintf(json_buffer, sizeof(json_buffer), "{\"type\":\"presence\",\"val\":%d}", 
                         latest_data.data.presence);
                last_presence = latest_data.data.presence;
                should_send = true;
            }
            break;
            
        case 0x02:  /* 运动状态 */
            if (latest_data.data.motion_state != last_motion) {
                snprintf(json_buffer, sizeof(json_buffer), "{\"type\":\"motion\",\"val\":%d}", 
                         latest_data.data.motion_state);
                last_motion = latest_data.data.motion_state;
                should_send = true;
            }
            break;
            
        case 0x03:  /* 呼吸频率 */
            if (abs(latest_data.data.breath_rate - last_breath) >= 2) {
                snprintf(json_buffer, sizeof(json_buffer), "{\"type\":\"breath\",\"val\":%d}", 
                         latest_data.data.breath_rate);
                last_breath = latest_data.data.breath_rate;
                should_send = true;
            }
            break;
            
        case 0x04:  /* 心率 */
            if (abs(latest_data.data.heart_rate - last_heart) >= 3) {
                snprintf(json_buffer, sizeof(json_buffer), "{\"type\":\"heart\",\"val\":%d}", 
                         latest_data.data.heart_rate);
                last_heart = latest_data.data.heart_rate;
                should_send = true;
            }
            break;
            
        case 0x05:  /* 睡眠状态 */
            if (latest_data.data.sleep_state != last_sleep_state) {
                snprintf(json_buffer, sizeof(json_buffer), "{\"type\":\"sleep\",\"val\":%d}", 
                         latest_data.data.sleep_state);
                last_sleep_state = latest_data.data.sleep_state;
                should_send = true;
            }
            break;
            
        case 0x06:  /* 睡眠评分 */
            if (latest_data.data.sleep_score != last_sleep_score && latest_data.data.sleep_score > 0) {
                snprintf(json_buffer, sizeof(json_buffer), "{\"type\":\"sleep_score\",\"val\":%d}", 
                         latest_data.data.sleep_score);
                last_sleep_score = latest_data.data.sleep_score;
                should_send = true;
            }
            break;
            
        case 0x07:  /* 人体方位 */
            if (abs(latest_data.data.position.x - last_x) > 5 || 
                abs(latest_data.data.position.y - last_y) > 5 || 
                abs(latest_data.data.position.z - last_z) > 5) {
                snprintf(json_buffer, sizeof(json_buffer), "{\"type\":\"pos\",\"x\":%d,\"y\":%d,\"z\":%d}", 
                         latest_data.data.position.x,
                         latest_data.data.position.y,
                         latest_data.data.position.z);
                last_x = latest_data.data.position.x;
                last_y = latest_data.data.position.y;
                last_z = latest_data.data.position.z;
                should_send = true;
            }
            break;
            
        case 0x08:  /* 人体距离 */
            if (abs(latest_data.data.distance - last_distance) > 10 && latest_data.data.distance > 0) {
                snprintf(json_buffer, sizeof(json_buffer), "{\"type\":\"distance\",\"val\":%d}", 
                         latest_data.data.distance);
                last_distance = latest_data.data.distance;
                should_send = true;
            }
            break;
            
        case 0x09:  /* 体动幅度 */
            if (abs(latest_data.data.motion_amp - last_motion_amp) > 5) {
                snprintf(json_buffer, sizeof(json_buffer), "{\"type\":\"body_move\",\"val\":%d}", 
                         latest_data.data.motion_amp);
                last_motion_amp = latest_data.data.motion_amp;
                should_send = true;
            }
            break;
            
        case 0x0A:  /* 入床/离床状态 */
            if (latest_data.data.bed_state != last_bed) {
                snprintf(json_buffer, sizeof(json_buffer), "{\"type\":\"bed\",\"val\":%d}", 
                         latest_data.data.bed_state);
                last_bed = latest_data.data.bed_state;
                should_send = true;
            }
            break;
            
        case 0x0B:  /* 异常状态 */
            if (latest_data.data.abnormal_type != last_abnormal) {
                snprintf(json_buffer, sizeof(json_buffer), "{\"type\":\"sleep_abnormal\",\"val\":%d}", 
                         latest_data.data.abnormal_type);
                last_abnormal = latest_data.data.abnormal_type;
                should_send = true;
            }
            break;
            
        default:
            break;
    }
        
    /* 发送MQTT消息 */
    if (should_send && strlen(json_buffer) > 0 && client != NULL) {
        if (g_publish_flag == 1) {
            int msg_id = esp_mqtt_client_publish(client, DEVICE_PUBLISH, json_buffer, 0, 0, 0);
            if (msg_id >= 0) {
                ESP_LOGI("RADAR_MQTT", "Sent: %s", json_buffer);
            }
        }
    }
}

/**
 * @brief       MQTT发送任务
 */
void mqtt_send_task(void *pvParameters)
{
    while (1) {
        if (client != NULL && g_publish_flag == 1) {
            punish_radar_data();
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
