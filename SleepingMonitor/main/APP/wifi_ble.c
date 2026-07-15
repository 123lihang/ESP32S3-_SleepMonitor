/**
 ****************************************************************************************************
 * @file        wifi_ble.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2023-08-26
 * @brief       网络连接配置（支持蓝牙配网）
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

#include "wifi_ble.h"
#include "wifi_provisioning/manager.h"
#include "wifi_provisioning/scheme_ble.h"

/* 事件标志 */
static EventGroupHandle_t   wifi_event;
#define WIFI_CONNECTED_BIT  BIT0
#define WIFI_FAIL_BIT       BIT1

network_connet_info network_connet;
static const char *TAG = "wifi_ble";
char lcd_buff[100] = {0};

/* 配网相关配置 */
#define PROV_TRANSPORT_BLE      "ble"

/* 安全配置 - Security 1 (如需Security 2可修改) */
#define PROV_SECURITY_VERSION   1   /* 1: Security 1, 2: Security 2 */
#define PROV_POP                "abcd1234"  /* 连接密码 */

#if PROV_SECURITY_VERSION == 2
/* Security 2 配置 (开发模式) */
#define EXAMPLE_PROV_SEC2_USERNAME          "wifiprov"
#define EXAMPLE_PROV_SEC2_PWD               "abcd1234"

/* 预生成的salt和verifier (对应用户名: wifiprov, 密码: abcd1234) */
static const char sec2_salt[] = {
    0x03, 0x6e, 0xe0, 0xc7, 0xbc, 0xb9, 0xed, 0xa8, 0x4c, 0x9e, 0xac, 0x97, 0xd9, 0x3d, 0xec, 0xf4
};

static const char sec2_verifier[] = {
    0x7c, 0x7c, 0x85, 0x47, 0x65, 0x08, 0x94, 0x6d, 0xd6, 0x36, 0xaf, 0x37, 0xd7, 0xe8, 0x91, 0x43,
    0x78, 0xcf, 0xfd, 0x61, 0x6c, 0x59, 0xd2, 0xf8, 0x39, 0x08, 0x12, 0x72, 0x38, 0xde, 0x9e, 0x24,
    0xa4, 0x70, 0x26, 0x1c, 0xdf, 0xa9, 0x03, 0xc2, 0xb2, 0x70, 0xe7, 0xb1, 0x32, 0x24, 0xda, 0x11,
    0x1d, 0x97, 0x18, 0xdc, 0x60, 0x72, 0x08, 0xcc, 0x9a, 0xc9, 0x0c, 0x48, 0x27, 0xe2, 0xae, 0x89,
    0xaa, 0x16, 0x25, 0xb8, 0x04, 0xd2, 0x1a, 0x9b, 0x3a, 0x8f, 0x37, 0xf6, 0xe4, 0x3a, 0x71, 0x2e,
    0xe1, 0x27, 0x86, 0x6e, 0xad, 0xce, 0x28, 0xff, 0x54, 0x46, 0x60, 0x1f, 0xb9, 0x96, 0x87, 0xdc,
    0x57, 0x40, 0xa7, 0xd4, 0x6c, 0xc9, 0x77, 0x54, 0xdc, 0x16, 0x82, 0xf0, 0xed, 0x35, 0x6a, 0xc4,
    0x70, 0xad, 0x3d, 0x90, 0xb5, 0x81, 0x94, 0x70, 0xd7, 0xbc, 0x65, 0xb2, 0xd5, 0x18, 0xe0, 0x2e,
    0xc3, 0xa5, 0xf9, 0x68, 0xdd, 0x64, 0x7b, 0xb8, 0xb7, 0x3c, 0x9c, 0xfc, 0x00, 0xd8, 0x71, 0x7e,
    0xb7, 0x9a, 0x7c, 0xb1, 0xb7, 0xc2, 0xc3, 0x18, 0x34, 0x29, 0x32, 0x43, 0x3e, 0x00, 0x99, 0xe9,
    0x82, 0x94, 0xe3, 0xd8, 0x2a, 0xb0, 0x96, 0x29, 0xb7, 0xdf, 0x0e, 0x5f, 0x08, 0x33, 0x40, 0x76,
    0x52, 0x91, 0x32, 0x00, 0x9f, 0x97, 0x2c, 0x89, 0x6c, 0x39, 0x1e, 0xc8, 0x28, 0x05, 0x44, 0x17,
    0x3f, 0x68, 0x02, 0x8a, 0x9f, 0x44, 0x61, 0xd1, 0xf5, 0xa1, 0x7e, 0x5a, 0x70, 0xd2, 0xc7, 0x23,
    0x81, 0xcb, 0x38, 0x68, 0xe4, 0x2c, 0x20, 0xbc, 0x40, 0x57, 0x76, 0x17, 0xbd, 0x08, 0xb8, 0x96,
    0xbc, 0x26, 0xeb, 0x32, 0x46, 0x69, 0x35, 0x05, 0x8c, 0x15, 0x70, 0xd9, 0x1b, 0xe9, 0xbe, 0xcc,
    0xa9, 0x38, 0xa6, 0x67, 0xf0, 0xad, 0x50, 0x13, 0x19, 0x72, 0x64, 0xbf, 0x52, 0xc2, 0x34, 0xe2,
    0x1b, 0x11, 0x79, 0x74, 0x72, 0xbd, 0x34, 0x5b, 0xb1, 0xe2, 0xfd, 0x66, 0x73, 0xfe, 0x71, 0x64,
    0x74, 0xd0, 0x4e, 0xbc, 0x51, 0x24, 0x19, 0x40, 0x87, 0x0e, 0x92, 0x40, 0xe6, 0x21, 0xe7, 0x2d,
    0x4e, 0x37, 0x76, 0x2f, 0x2e, 0xe2, 0x68, 0xc7, 0x89, 0xe8, 0x32, 0x13, 0x42, 0x06, 0x84, 0x84,
    0x53, 0x4a, 0xb3, 0x0c, 0x1b, 0x4c, 0x8d, 0x1c, 0x51, 0x97, 0x19, 0xab, 0xae, 0x77, 0xff, 0xdb,
    0xec, 0xf0, 0x10, 0x95, 0x34, 0x33, 0x6b, 0xcb, 0x3e, 0x84, 0x0f, 0xb9, 0xd8, 0x5f, 0xb8, 0xa0,
    0xb8, 0x55, 0x53, 0x3e, 0x70, 0xf7, 0x18, 0xf5, 0xce, 0x7b, 0x4e, 0xbf, 0x27, 0xce, 0xce, 0xa8,
    0xb3, 0xbe, 0x40, 0xc5, 0xc5, 0x32, 0x29, 0x3e, 0x71, 0x64, 0x9e, 0xde, 0x8c, 0xf6, 0x75, 0xa1,
    0xe6, 0xf6, 0x53, 0xc8, 0x31, 0xa8, 0x78, 0xde, 0x50, 0x40, 0xf7, 0x62, 0xde, 0x36, 0xb2, 0xba
};

static esp_err_t get_sec2_salt(const char **salt, uint16_t *salt_len)
{
    *salt = sec2_salt;
    *salt_len = sizeof(sec2_salt);
    return ESP_OK;
}

static esp_err_t get_sec2_verifier(const char **verifier, uint16_t *verifier_len)
{
    *verifier = sec2_verifier;
    *verifier_len = sizeof(sec2_verifier);
    return ESP_OK;
}
#endif

/**
 * @brief       连接显示
 * @param       flag:0x80->显示SSID信息;0x04->连接中;0x02->连接失败;0x01->连接成功显示IP
 * @retval      无
 */
void connet_display(uint8_t flag)
{
    if((flag & 0x80) == 0x80)
    {
        lcd_fill(30, 40, 240+30, 130+16, WHITE);
        lcd_show_string(5, 40, 240, 16, 16, "BLE Provisioning", BLUE);
        lcd_show_string(5, 60, 240, 16, 16, "Use ESP BLE", BLUE);
        lcd_show_string(5, 80, 240, 16, 16, "Provisioning APP", BLUE);
        lcd_show_string(5, 100, 240, 16, 16, "POP:abcd1234", MAGENTA);
    }
    else if ((flag & 0x04) == 0x04)
    {
        lcd_show_string(5, 40, 240, 16, 16, "wifi connecting......", BLUE);
    }
    else if ((flag & 0x02) == 0x02)
    {
        lcd_show_string(5, 40, 240, 16, 16, "wifi connecting fail", BLUE);
    }
    else if ((flag & 0x01) == 0x01)
    {
        printf("%s\r\n", network_connet.ip_buf);
        lcd_show_string(5, 100, 240, 16, 16, (char*)network_connet.ip_buf, MAGENTA);
    }

    network_connet.connet_state &= 0x00;
}

/**
 * @brief       获取设备服务名称
 * @param       service_name: 输出缓冲区
 * @param       max: 缓冲区大小
 * @retval      无
 */
static void get_device_service_name(char *service_name, size_t max)
{
    uint8_t eth_mac[6];
    const char *ssid_prefix = "PROV_";
    esp_wifi_get_mac(WIFI_IF_STA, eth_mac);
    snprintf(service_name, max, "%s%02X%02X%02X",
             ssid_prefix, eth_mac[3], eth_mac[4], eth_mac[5]);
}

/**
 * @brief       配网事件处理函数
 * @param       arg: 参数
 * @param       event_base: 事件基类
 * @param       event_id: 事件ID
 * @param       event_data: 事件数据
 * @retval      无
 */
static void prov_event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data)
{
    if (event_base == WIFI_PROV_EVENT) {
        switch (event_id) {
            case WIFI_PROV_START:
                ESP_LOGI(TAG, "Provisioning started");
                network_connet.connet_state |= 0x80;
                network_connet.fun(network_connet.connet_state);
                break;
            case WIFI_PROV_CRED_RECV: {
                wifi_sta_config_t *wifi_sta_cfg = (wifi_sta_config_t *)event_data;
                ESP_LOGI(TAG, "Received Wi-Fi credentials"
                         "\n\tSSID     : %s\n\tPassword : %s",
                         (const char *) wifi_sta_cfg->ssid,
                         (const char *) wifi_sta_cfg->password);
                break;
            }
            case WIFI_PROV_CRED_FAIL: {
                wifi_prov_sta_fail_reason_t *reason = (wifi_prov_sta_fail_reason_t *)event_data;
                ESP_LOGE(TAG, "Provisioning failed!\n\tReason : %s",
                         (*reason == WIFI_PROV_STA_AUTH_ERROR) ?
                         "Wi-Fi station authentication failed" : "Wi-Fi access-point not found");
                network_connet.connet_state |= 0x02;
                network_connet.fun(network_connet.connet_state);
                break;
            }
            case WIFI_PROV_CRED_SUCCESS:
                ESP_LOGI(TAG, "Provisioning successful");
                network_connet.connet_state |= 0x04;
                network_connet.fun(network_connet.connet_state);
                break;
            case WIFI_PROV_END:
                ESP_LOGI(TAG, "Provisioning finished");
                break;
            default:
                break;
        }
    } else if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                esp_wifi_connect();
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                ESP_LOGI(TAG, "Disconnected. Connecting to the AP again...");
                esp_wifi_connect();
                break;
            default:
                break;
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Connected with IP Address:" IPSTR, IP2STR(&event->ip_info.ip));
        sprintf(network_connet.ip_buf, "IP:" IPSTR, IP2STR(&event->ip_info.ip));
        network_connet.connet_state |= 0x01;
        network_connet.fun(network_connet.connet_state);
        xEventGroupSetBits(wifi_event, WIFI_CONNECTED_BIT);
    } else if (event_base == PROTOCOMM_TRANSPORT_BLE_EVENT) {
        switch (event_id) {
            case PROTOCOMM_TRANSPORT_BLE_CONNECTED:
                ESP_LOGI(TAG, "BLE transport: Connected!");
                break;
            case PROTOCOMM_TRANSPORT_BLE_DISCONNECTED:
                ESP_LOGI(TAG, "BLE transport: Disconnected!");
                break;
            default:
                break;
        }
    }
}

/**
 * @brief       自定义数据端点处理函数
 * @param       session_id: 会话ID
 * @param       inbuf: 输入数据
 * @param       inlen: 输入数据长度
 * @param       outbuf: 输出数据
 * @param       outlen: 输出数据长度
 * @param       priv_data: 私有数据
 * @retval      ESP_OK
 */
esp_err_t custom_prov_data_handler(uint32_t session_id, const uint8_t *inbuf, ssize_t inlen,
                                          uint8_t **outbuf, ssize_t *outlen, void *priv_data)
{
    if (inbuf) {
        ESP_LOGI(TAG, "Received custom data: %.*s", inlen, (char *)inbuf);
    }
    char response[] = "SUCCESS";
    *outbuf = (uint8_t *)strdup(response);
    if (*outbuf == NULL) {
        ESP_LOGE(TAG, "System out of memory");
        return ESP_ERR_NO_MEM;
    }
    *outlen = strlen(response) + 1;

    return ESP_OK;
}

/**
 * @brief       初始化Wi-Fi STA模式
 * @param       无
 * @retval      无
 */
static void wifi_init_sta(void)
{
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
}

/**
 * @brief       蓝牙配网初始化（主函数）
 * @param       无
 * @retval      无
 */
void wifi_ble_prov_init(void)
{
    ESP_LOGI(TAG, "=== wifi_ble_prov_init START ===");
    
    /* ========== 第1步：初始化网络和事件循环 ========== */
    ESP_ERROR_CHECK(esp_netif_init());           // 初始化 TCP/IP 栈
    ESP_ERROR_CHECK(esp_event_loop_create_default()); // 创建默认事件循环
    
    /* ========== 第2步：创建事件组 ========== */
    network_connet.connet_state = 0x00;
    network_connet.fun = connet_display;
    wifi_event = xEventGroupCreate();

    /* ========== 第3步：注册事件处理函数 ========== */
    ESP_LOGI(TAG, "Registering event handlers...");
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, &prov_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(PROTOCOMM_TRANSPORT_BLE_EVENT, ESP_EVENT_ANY_ID, &prov_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &prov_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &prov_event_handler, NULL));

    /* ========== 第4步：创建Wi-Fi接口 ========== */
    esp_netif_create_default_wifi_sta();

    /* ========== 第5步：初始化Wi-Fi ========== */
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    /* ========== 第6步：初始化配网管理器 ========== */
    ESP_LOGI(TAG, "Initializing provisioning manager...");
    wifi_prov_mgr_config_t config = {
        .scheme = wifi_prov_scheme_ble,
        .scheme_event_handler = WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM
    };
    ESP_ERROR_CHECK(wifi_prov_mgr_init(config));

    /* ========== 第7步：检查是否已配网 ========== */
    bool provisioned = false;
    ESP_ERROR_CHECK(wifi_prov_mgr_is_provisioned(&provisioned));

    if (!provisioned) {
        ESP_LOGI(TAG, "Starting BLE provisioning");

        char service_name[12];
        get_device_service_name(service_name, sizeof(service_name));

        uint8_t custom_service_uuid[] = {
            0xb4, 0xdf, 0x5a, 0x1c, 0x3f, 0x6b, 0xf4, 0xbf,
            0xea, 0x4a, 0x82, 0x03, 0x04, 0x90, 0x1a, 0x02,
        };
        wifi_prov_scheme_ble_set_service_uuid(custom_service_uuid);

#if PROV_SECURITY_VERSION == 1
        wifi_prov_security_t security = WIFI_PROV_SECURITY_1;
        const char *pop = PROV_POP;
        wifi_prov_security1_params_t *sec_params = (void *)pop;
#else
        wifi_prov_security_t security = WIFI_PROV_SECURITY_2;
        const char *username = EXAMPLE_PROV_SEC2_USERNAME;
        const char *pop = EXAMPLE_PROV_SEC2_PWD;
        wifi_prov_security2_params_t sec2_params = {};
        get_sec2_salt(&sec2_params.salt, &sec2_params.salt_len);
        get_sec2_verifier(&sec2_params.verifier, &sec2_params.verifier_len);
        wifi_prov_security2_params_t *sec_params = &sec2_params;
#endif

        wifi_prov_mgr_endpoint_create("custom-data");

        ESP_LOGI(TAG, "Starting provisioning with service name: %s", service_name);
        ESP_LOGI(TAG, "BLE device name: %s, POP: %s", service_name, pop);
        
        ESP_ERROR_CHECK(wifi_prov_mgr_start_provisioning(security, (const void *)sec_params, 
                                                          service_name, NULL));

        wifi_prov_mgr_endpoint_register("custom-data", custom_prov_data_handler, NULL);

        /* 等待配网完成 */
        EventBits_t bits = xEventGroupWaitBits(wifi_event, 
                                                WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                                pdFALSE,
                                                pdFALSE,
                                                portMAX_DELAY);

        if (bits & WIFI_CONNECTED_BIT) {
            ESP_LOGI(TAG, "Wi-Fi connected successfully!");
        } else {
            ESP_LOGE(TAG, "Wi-Fi connection failed!");
        }

        wifi_prov_mgr_deinit();
        esp_event_handler_unregister(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, prov_event_handler);
        esp_event_handler_unregister(PROTOCOMM_TRANSPORT_BLE_EVENT, ESP_EVENT_ANY_ID, prov_event_handler);
        
    } else {
        ESP_LOGI(TAG, "Already provisioned, starting Wi-Fi STA");
        
        wifi_prov_mgr_deinit();
        
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_start());
        
        xEventGroupWaitBits(wifi_event, WIFI_CONNECTED_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
    }

    vEventGroupDelete(wifi_event);
}

/**
 * @brief       重置并重新启动配网（修复编译错误版本）
 * @param       无
 * @retval      无
 */
void wifi_ble_reset_and_reprovision(void)
{
    ESP_LOGW(TAG, "=== Resetting device for re-provisioning ===");
    
    /* 显示重置信息 */
    lcd_show_string(5, 40, 240, 16, 16, "Clearing WiFi data...", RED);
    lcd_show_string(5, 60, 240, 16, 16, "Device will restart", RED);
    lcd_show_string(5, 80, 240, 16, 16, "in 2 seconds", RED);
    
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    /* ========== 直接擦除NVS并重启，不清理其他组件 ========== */
    nvs_flash_deinit();
    nvs_flash_erase();
    nvs_flash_init();
    
    /* 重启设备 */
    esp_restart();
}