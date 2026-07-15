/**
 ****************************************************************************************************
 * @file        usart.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V2.0
 * @date        2024-01-20
 * @brief       串口初始化代码 - 增强版雷达协议支持
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

#include "usart.h"

/* 全局变量：接收缓冲区 */
static uint8_t radar_rx_buffer[512];
static int radar_rx_index = 0;
static int radar_frame_started = 0;
static QueueHandle_t radar_data_queue = NULL;  /* LCD显示队列 */
static QueueHandle_t mqtt_data_queue = NULL;   /* MQTT发送队列 */

/**
 * @brief       获取雷达数据队列句柄
 * @param       无
 * @retval      队列句柄
 */
QueueHandle_t get_radar_data_queue(void)
{
    return radar_data_queue;
}

// 获取MQTT队列的函数
QueueHandle_t get_mqtt_data_queue(void)
{
    return mqtt_data_queue;
}

/**
 * @brief       初始化串口
 * @param       baudrate: 波特率, 根据自己需要设置波特率值
 * @note        注意: 必须设置正确的时钟源, 否则串口波特率就会设置异常.
 * @retval      无
 */
void usart_init(uint32_t baudrate)
{
    uart_config_t uart_config;                          /* 串口配置句柄 */

    uart_config.baud_rate = baudrate;                   /* 波特率 */
    uart_config.data_bits = UART_DATA_8_BITS;           /* 字长为8位数据格式 */
    uart_config.parity = UART_PARITY_DISABLE;           /* 无奇偶校验位 */
    uart_config.stop_bits = UART_STOP_BITS_1;           /* 一个停止位 */
    uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;   /* 无硬件控制流 */
    uart_config.source_clk = UART_SCLK_APB;             /* 配置时钟源 */
    uart_config.rx_flow_ctrl_thresh = 122;              /* 硬件控制流阈值 */
    uart_param_config(USART_UX, &uart_config);          /* 配置uart端口 */

    /* 配置uart引脚 */
    uart_set_pin(USART_UX, USART_TX_GPIO_PIN, USART_RX_GPIO_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    /* 安装串口驱动 */
    uart_driver_install(USART_UX, RX_BUF_SIZE * 2, RX_BUF_SIZE * 2, 20, NULL, 0);
    
    /* 创建雷达数据队列（给LCD显示用） */
    radar_data_queue = xQueueCreate(50, sizeof(radar_data_t));
    if (radar_data_queue == NULL) {
        printf("[RADAR] Failed to create LCD data queue!\r\n");
    }
    
    /* 创建MQTT数据队列（给MQTT发送用） */
    mqtt_data_queue = xQueueCreate(50, sizeof(radar_data_t));
    if (mqtt_data_queue == NULL) {
        printf("[RADAR] Failed to create MQTT data queue!\r\n");
    }
}

/**
 * @brief       发送雷达数据到队列
 * @param       data_type: 数据类型
 * @param       value_ptr: 数据指针
 * @param       value_len: 数据长度
 * @retval      无
 */
static void send_radar_data_to_queue(uint8_t data_type, void *value_ptr, uint8_t value_len)
{
    
    radar_data_t radar_data = {0};
    radar_data.data_type = data_type;
    radar_data.timestamp = xTaskGetTickCount();
    
    switch (data_type) {
        case 0x01:  /* 有人/无人 */
            if (value_len >= 1) radar_data.data.presence = *(uint8_t*)value_ptr;
            break;
        case 0x02:  /* 运动状态 */
            if (value_len >= 1) radar_data.data.motion_state = *(uint8_t*)value_ptr;
            break;
        case 0x03:  /* 呼吸率 */
            if (value_len >= 1) radar_data.data.breath_rate = *(uint8_t*)value_ptr;
            break;
        case 0x04:  /* 心率 */
            if (value_len >= 1) radar_data.data.heart_rate = *(uint8_t*)value_ptr;
            break;
        case 0x05:  /* 睡眠状态 */
            if (value_len >= 1) radar_data.data.sleep_state = *(uint8_t*)value_ptr;
            break;
        case 0x06:  /* 睡眠评分 */
            if (value_len >= 1) radar_data.data.sleep_score = *(uint8_t*)value_ptr;
            break;
        case 0x07:  /* 人体方位 */
            if (value_len >= 6) {
                uint8_t *pos = (uint8_t*)value_ptr;
                radar_data.data.position.x = (int16_t)((pos[0] << 8) | pos[1]);
                radar_data.data.position.y = (int16_t)((pos[2] << 8) | pos[3]);
                radar_data.data.position.z = (int16_t)((pos[4] << 8) | pos[5]);
            }
            break;
        case 0x08:  /* 人体距离 */
            if (value_len >= 2) radar_data.data.distance = *(uint16_t*)value_ptr;
            break;
        case 0x09:  /* 体动幅度 */
            if (value_len >= 1) radar_data.data.motion_amp = *(uint8_t*)value_ptr;
            break;
        case 0x0A:  /* 入床/离床 */
            if (value_len >= 1) radar_data.data.bed_state = *(uint8_t*)value_ptr;
            break;
        case 0x0B:  /* 异常状态 */
            if (value_len >= 1) radar_data.data.abnormal_type = *(uint8_t*)value_ptr;
            break;
        case 0x0E:  /* 清醒时长 */
            if (value_len >= 2) radar_data.data.awake_time = *(uint16_t*)value_ptr;
            break;
        case 0x0F:  /* 浅睡时长 */
            if (value_len >= 2) radar_data.data.light_sleep_time = *(uint16_t*)value_ptr;
            break;
        case 0x10:  /* 深睡时长 */
            if (value_len >= 2) radar_data.data.deep_sleep_time = *(uint16_t*)value_ptr;
            break;
        case 0x11:  /* 呼吸状态 */
            if (value_len >= 1) radar_data.data.breath_info = *(uint8_t*)value_ptr;
            break;
        case 0x12:  /* 异常挣扎状态 */
            if (value_len >= 1) radar_data.data.struggle_status = *(uint8_t*)value_ptr;
            break;
        case 0x13:  /* 无人计时状态 */
            if (value_len >= 1) radar_data.data.nopeople_status = *(uint8_t*)value_ptr;
            break;
        default:
            return;
    }
    
    // 同时发送到两个队列，队列满就丢弃旧数据
    if (radar_data_queue != NULL) {
        xQueueSend(radar_data_queue, &radar_data, 0);
    }
    if (mqtt_data_queue != NULL) {
        xQueueSend(mqtt_data_queue, &radar_data, 0);
    }
}

/**
 * @brief       解析人体方位数据
 * @param       data: 6字节方位数据 (x,y,z各2字节)
 * @note        首位为0表示正，首位为1表示负
 * @retval      无
 */
static void parse_position_data(uint8_t *data)
{
    int16_t x, y, z;
    
    /* 解析X坐标 */
    x = (int16_t)((data[0] << 8) | data[1]);
    /* 解析Y坐标 */
    y = (int16_t)((data[2] << 8) | data[3]);
    /* 解析Z坐标 */
    z = (int16_t)((data[4] << 8) | data[5]);
    
    printf("[RADAR] 人体方位 X: %d cm, Y: %d cm, Z: %d cm\r\n", x, y, z);
    send_radar_data_to_queue(0x07, data, 6);
}

/**
 * @brief       解析睡眠综合状态 (0x0C)
 * @param       data: 8字节数据
 * @retval      无
 */
static void parse_sleep_compre_state(uint8_t *data)
{
    sleep_compre_t compre;
    uint8_t presence = data[0];
    uint8_t sleep_state = data[1];
    uint8_t avg_breath = data[2];
    uint8_t avg_heart = data[3];
    uint8_t turn_over = data[4];
    uint8_t large_motion = data[5];
    uint8_t small_motion = data[6];
    uint8_t apnea = data[7];
    
    const char *state_str = "未知";
    switch (sleep_state) {
        case 0: state_str = "深睡"; break;
        case 1: state_str = "浅睡"; break;
        case 2: state_str = "清醒"; break;
        case 3: state_str = "离床"; break;
        default: break;
    }
    
    printf("[RADAR] 睡眠综合状态:\r\n");
    printf("  - 存在状态: %s\r\n", presence ? "有人" : "无人");
    printf("  - 睡眠状态: %s\r\n", state_str);
    printf("  - 平均呼吸: %d 次/分钟\r\n", avg_breath);
    printf("  - 平均心跳: %d 次/分钟\r\n", avg_heart);
    printf("  - 翻身次数: %d\r\n", turn_over);
    printf("  - 大幅度体动占比: %d%%\r\n", large_motion);
    printf("  - 小幅度体动占比: %d%%\r\n", small_motion);
    printf("  - 呼吸暂停次数: %d\r\n", apnea);
    
    /* 发送到队列 */
    send_radar_data_to_queue(0x01, &presence, 1);
    send_radar_data_to_queue(0x05, &sleep_state, 1);
    send_radar_data_to_queue(0x03, &avg_breath, 1);
    send_radar_data_to_queue(0x04, &avg_heart, 1);
}

/**
 * @brief       解析睡眠质量分析 (0x0D)
 * @param       data: 12字节数据
 * @retval      无
 */
static void parse_sleep_analysis(uint8_t *data)
{
    uint8_t score = data[0];
    uint16_t total_time = (data[1] << 8) | data[2];
    uint8_t awake_ratio = data[3];
    uint8_t light_ratio = data[4];
    uint8_t deep_ratio = data[5];
    uint8_t out_bed_time = data[6];
    uint8_t out_bed_count = data[7];
    uint8_t turn_over = data[8];
    uint8_t avg_breath = data[9];
    uint8_t avg_heart = data[10];
    uint8_t apnea = data[11];
    
    const char *rating_str = "未知";
    if (score > 0) {
        if (score <= 60) rating_str = "较差";
        else if (score <= 75) rating_str = "一般";
        else rating_str = "良好";
    }
    
    printf("[RADAR] 睡眠质量分析:\r\n");
    printf("  - 睡眠评分: %d (%s)\r\n", score, rating_str);
    printf("  - 睡眠总时长: %d 分钟 (%.1f 小时)\r\n", total_time, total_time / 60.0);
    printf("  - 清醒时长占比: %d%%\r\n", awake_ratio);
    printf("  - 浅睡时长占比: %d%%\r\n", light_ratio);
    printf("  - 深睡时长占比: %d%%\r\n", deep_ratio);
    printf("  - 离床时长: %d 分钟\r\n", out_bed_time);
    printf("  - 离床次数: %d\r\n", out_bed_count);
    printf("  - 翻身次数: %d\r\n", turn_over);
    printf("  - 平均呼吸: %d 次/分钟\r\n", avg_breath);
    printf("  - 平均心跳: %d 次/分钟\r\n", avg_heart);
    printf("  - 呼吸暂停次数: %d\r\n", apnea);
    
    /* 发送到队列 */
    send_radar_data_to_queue(0x06, &score, 1);
    send_radar_data_to_queue(0x0E, &total_time, 2);
    send_radar_data_to_queue(0x03, &avg_breath, 1);
    send_radar_data_to_queue(0x04, &avg_heart, 1);
}

/**
 * @brief       解析睡眠异常状态
 * @param       abnormal: 异常类型
 * @retval      无
 */
static void parse_sleep_abnormal(uint8_t abnormal)
{
    const char *abnormal_str = "未知";
    switch (abnormal) {
        case 0x00: abnormal_str = "睡眠时长不足4小时"; break;
        case 0x01: abnormal_str = "睡眠时长大于12小时"; break;
        case 0x02: abnormal_str = "长时间异常无人"; break;
        case 0x03: abnormal_str = "无异常"; break;
        default: break;
    }
    printf("[RADAR] 睡眠异常: %s\r\n", abnormal_str);
    send_radar_data_to_queue(0x0B, &abnormal, 1);
}

/**
 * @brief       解析睡眠质量评级
 * @param       rating: 评级 (0-无,1-良好,2-一般,3-较差)
 * @retval      无
 */
static void parse_sleep_rating(uint8_t rating)
{
    const char *rating_str = "未知";
    switch (rating) {
        case 0x00: rating_str = "无评级"; break;
        case 0x01: rating_str = "良好"; break;
        case 0x02: rating_str = "一般"; break;
        case 0x03: rating_str = "较差"; break;
        default: break;
    }
    printf("[RADAR] 睡眠质量评级: %s\r\n", rating_str);
    send_radar_data_to_queue(0x06, &rating, 1);
}

/**
 * @brief       解析异常挣扎状态
 * @param       status: 状态 (0-无,1-正常,2-异常)
 * @retval      无
 */
static void parse_struggle_status(uint8_t status)
{
    const char *status_str = "未知";
    switch (status) {
        case 0x00: status_str = "无"; break;
        case 0x01: status_str = "正常"; break;
        case 0x02: status_str = "异常挣扎"; break;
        default: break;
    }
    printf("[RADAR] 异常挣扎状态: %s\r\n", status_str);
    send_radar_data_to_queue(0x12, &status, 1);
}

/**
 * @brief       解析无人计时状态
 * @param       status: 状态 (0-无,1-正常,2-异常)
 * @retval      无
 */
static void parse_nopeople_timer(uint8_t status)
{
    const char *status_str = "未知";
    switch (status) {
        case 0x00: status_str = "无"; break;
        case 0x01: status_str = "正常"; break;
        case 0x02: status_str = "异常"; break;
        default: break;
    }
    printf("[RADAR] 无人计时状态: %s\r\n", status_str);
    send_radar_data_to_queue(0x13, &status, 1);
}

/**
 * @brief       解析入床/离床状态
 * @param       state: 0-离床, 1-入床
 * @retval      无
 */
static void parse_bed_state(uint8_t state)
{
    printf("[RADAR] 床状态: %s\r\n", state ? "入床" : "离床");
    send_radar_data_to_queue(0x0A, &state, 1);
}

/**
 * @brief       解析雷达数据帧
 * @param       frame: 完整帧数据
 * @param       len: 帧长度
 * @retval      无
 */
static void parse_radar_frame(uint8_t *frame, int len)
{
    /* 校验和计算（帧头+控制字+命令字+长度标识+数据 求和后取低八位） */
    uint8_t calc_checksum = 0;
    for (int i = 0; i < len - 3; i++) {
        calc_checksum += frame[i];
    }
    uint8_t recv_checksum = frame[len - 3];
    
    if (calc_checksum != recv_checksum) {
        printf("[RADAR] Checksum error! Calc: 0x%02X, Recv: 0x%02X\r\n", calc_checksum, recv_checksum);
        return;
    }
    
    /* 提取帧头、控制字、命令字和数据 */
    uint8_t ctrl_word = frame[2];
    uint8_t cmd_word = frame[3];
    uint16_t data_len = (frame[4] << 8) | frame[5];
    uint8_t *data_ptr = &frame[6];
    
    /* 根据控制字和命令字解析数据 */
    switch (ctrl_word) {
        case CTRL_WORD_PRESENCE:    /* 0x80 - 人体存在 */
            switch (cmd_word) {
                case CMD_PRESENCE_STATUS:   /* 0x01 - 存在状态 */
                    if (data_len >= 1) {
                        printf("[RADAR] 存在状态: %s\r\n", data_ptr[0] ? "有人" : "无人");
                        send_radar_data_to_queue(0x01, data_ptr, 1);
                    }
                    break;
                case CMD_MOTION_STATUS:     /* 0x02 - 运动状态 */
                    if (data_len >= 1) {
                        const char *motion_str = "未知";
                        switch (data_ptr[0]) {
                            case 0: motion_str = "无运动"; break;
                            case 1: motion_str = "静止"; break;
                            case 2: motion_str = "活跃"; break;
                            default: break;
                        }
                        printf("[RADAR] 运动状态: %s\r\n", motion_str);
                        send_radar_data_to_queue(0x02, data_ptr, 1);
                    }
                    break;
                case CMD_MOTION_PARAM:      /* 0x03 - 体动参数 */
                    if (data_len >= 1) {
                        printf("[RADAR] 体动幅度: %d\r\n", data_ptr[0]);
                        send_radar_data_to_queue(0x09, data_ptr, 1);
                    }
                    break;
                case CMD_DISTANCE:          /* 0x04 - 人体距离 */
                    if (data_len >= 2) {
                        uint16_t distance = (data_ptr[0] << 8) | data_ptr[1];
                        printf("[RADAR] 人体距离: %d cm\r\n", distance);
                        send_radar_data_to_queue(0x08, data_ptr, 2);
                    }
                    break;
                case CMD_POSITION:          /* 0x05 - 人体方位 */
                    if (data_len >= 6) {
                        parse_position_data(data_ptr);
                    }
                    break;
                default:
                    printf("[RADAR] 未知人体存在命令字: 0x%02X\r\n", cmd_word);
                    break;
            }
            break;
            
        case CTRL_WORD_BREATH:      /* 0x81 - 呼吸检测 */
            switch (cmd_word) {
                case CMD_BREATH_INFO:       /* 0x01 - 呼吸信息 */
                    if (data_len >= 1) {
                        const char *breath_str = "未知";
                        switch (data_ptr[0]) {
                            case 0: breath_str = "正常"; break;
                            case 1: breath_str = "正常"; break;
                            case 2: breath_str = "呼吸过高 (>25次/分钟)"; break;
                            case 3: breath_str = "呼吸过低 (<10次/分钟)"; break;
                            case 4: breath_str = "无人"; break;
                            default: break;
                        }
                        printf("[RADAR] 呼吸状态: %s\r\n", breath_str);
                        send_radar_data_to_queue(0x11, data_ptr, 1);
                    }
                    break;
                case CMD_BREATH_VALUE:      /* 0x02 - 呼吸数值 */
                    if (data_len >= 1) {
                        printf("[RADAR] 呼吸频率: %d 次/分钟\r\n", data_ptr[0]);
                        send_radar_data_to_queue(0x03, data_ptr, 1);
                    }
                    break;
                case CMD_BREATH_WAVE:       /* 0x05 - 呼吸波形 */
                    if (data_len >= 5) {
                        printf("[RADAR] 呼吸波形数据: %02X %02X %02X %02X %02X\r\n", 
                               data_ptr[0], data_ptr[1], data_ptr[2], data_ptr[3], data_ptr[4]);
                    }
                    break;
                default:
                    printf("[RADAR] 未知呼吸检测命令字: 0x%02X\r\n", cmd_word);
                    break;
            }
            break;
            
        case CTRL_WORD_HEART_RATE:  /* 0x85 - 心率监测 */
            switch (cmd_word) {
                case CMD_HEART_VALUE:       /* 0x02 - 心率数值 */
                    if (data_len >= 1) {
                        printf("[RADAR] 心跳频率: %d 次/分钟\r\n", data_ptr[0]);
                        send_radar_data_to_queue(0x04, data_ptr, 1);
                    }
                    break;
                case CMD_HEART_WAVE:        /* 0x05 - 心率波形 */
                    if (data_len >= 5) {
                        printf("[RADAR] 心率波形数据: %02X %02X %02X %02X %02X\r\n",
                               data_ptr[0], data_ptr[1], data_ptr[2], data_ptr[3], data_ptr[4]);
                    }
                    break;
                default:
                    printf("[RADAR] 未知心率监测命令字: 0x%02X\r\n", cmd_word);
                    break;
            }
            break;
            
        case CTRL_WORD_SLEEP:       /* 0x84 - 睡眠监测 */
            switch (cmd_word) {
                case CMD_SLEEP_STATUS:      /* 0x02 - 睡眠状态 */
                    if (data_len >= 1) {
                        const char *state_str = "未知";
                        switch (data_ptr[0]) {
                            case 0: state_str = "深睡"; break;
                            case 1: state_str = "浅睡"; break;
                            case 2: state_str = "清醒"; break;
                            case 3: state_str = "离床"; break;
                            default: break;
                        }
                        printf("[RADAR] 睡眠状态: %s\r\n", state_str);
                        send_radar_data_to_queue(0x05, data_ptr, 1);
                    }
                    break;
                case CMD_AWAKE_TIME:        /* 0x03 - 清醒时长 */
                    if (data_len >= 2) {
                        uint16_t time = (data_ptr[0] << 8) | data_ptr[1];
                        printf("[RADAR] 清醒时长: %d 分钟\r\n", time);
                        send_radar_data_to_queue(0x0E, data_ptr, 2);
                    }
                    break;
                case CMD_LIGHT_SLEEP_TIME:  /* 0x04 - 浅睡时长 */
                    if (data_len >= 2) {
                        uint16_t time = (data_ptr[0] << 8) | data_ptr[1];
                        printf("[RADAR] 浅睡时长: %d 分钟\r\n", time);
                        send_radar_data_to_queue(0x0F, data_ptr, 2);
                    }
                    break;
                case CMD_DEEP_SLEEP_TIME:   /* 0x05 - 深睡时长 */
                    if (data_len >= 2) {
                        uint16_t time = (data_ptr[0] << 8) | data_ptr[1];
                        printf("[RADAR] 深睡时长: %d 分钟\r\n", time);
                        send_radar_data_to_queue(0x10, data_ptr, 2);
                    }
                    break;
                case CMD_SLEEP_SCORE:       /* 0x06 - 睡眠评分 */
                    if (data_len >= 1) {
                        printf("[RADAR] 睡眠评分: %d 分\r\n", data_ptr[0]);
                        send_radar_data_to_queue(0x06, data_ptr, 1);
                    }
                    break;
                case CMD_SLEEP_COMPRE:      /* 0x0C - 睡眠综合状态 */
                    if (data_len >= 8) {
                        parse_sleep_compre_state(data_ptr);
                    }
                    break;
                case CMD_SLEEP_ANALYSIS:    /* 0x0D - 睡眠质量分析 */
                    if (data_len >= 12) {
                        parse_sleep_analysis(data_ptr);
                    }
                    break;
                case CMD_SLEEP_ABNORMAL:    /* 0x0E - 睡眠异常 */
                    if (data_len >= 1) {
                        parse_sleep_abnormal(data_ptr[0]);
                    }
                    break;
                case CMD_SLEEP_RATING:      /* 0x10 - 睡眠质量评级 */
                    if (data_len >= 1) {
                        parse_sleep_rating(data_ptr[0]);
                    }
                    break;
                case CMD_STRUGGLE_STATUS:   /* 0x11 - 异常挣扎状态 */
                    if (data_len >= 1) {
                        parse_struggle_status(data_ptr[0]);
                    }
                    break;
                case CMD_NOPEOPLE_TIMER:    /* 0x12 - 无人计时状态 */
                    if (data_len >= 1) {
                        parse_nopeople_timer(data_ptr[0]);
                    }
                    break;
                default:
                    printf("[RADAR] 未知睡眠监测命令字: 0x%02X\r\n", cmd_word);
                    break;
            }
            break;
            
        case CTRL_WORD_HEARTBEAT:   /* 0x01 - 心跳包 */
            printf("[RADAR] 心跳包收到\r\n");
            break;
            
        case CTRL_WORD_PRODUCT_INFO: /* 0x02 - 产品信息 */
            printf("[RADAR] 产品信息帧收到, 命令字: 0x%02X\r\n", cmd_word);
            break;
            
        case CTRL_WORD_WORK_STATUS: /* 0x05 - 工作状态 */
            if (cmd_word == 0x01 && data_len >= 1) {
                printf("[RADAR] 初始化状态: %s\r\n", data_ptr[0] ? "已完成" : "未完成");
            }
            break;
            
        default:
            printf("[RADAR] 未知数据帧, 控制字: 0x%02X, 命令字: 0x%02X\r\n", ctrl_word, cmd_word);
            /* 打印原始数据用于调试 */
            printf("[RADAR] Raw data: ");
            for (int i = 0; i < len; i++) {
                printf("%02X ", frame[i]);
            }
            printf("\r\n");
            break;
    }
}


/**
 * @brief       雷达数据解析任务
 * @param       pvParameters: 传入参数
 * @retval      无
 */
void radar_data_parse_task(void *pvParameters)
{
    uint8_t *data = (uint8_t *)malloc(RX_BUF_SIZE);
    if (!data) {
        printf("[RADAR] Failed to allocate memory!\r\n");
        vTaskDelete(NULL);
        return;
    }
    
    printf("[RADAR] Task started, waiting for radar data...\r\n");
    
    while (1) {
        /* 从串口读取数据 */
        int len = uart_read_bytes(USART_UX, data, RX_BUF_SIZE, pdMS_TO_TICKS(100));
        
        if (len > 0) {
            /* 逐字节解析，寻找帧头帧尾 */
            for (int i = 0; i < len; i++) {
                uint8_t byte = data[i];
                
                if (!radar_frame_started) {
                    /* 寻找帧头 0x53 0x59 */
                    if (byte == FRAME_HEADER1) {
                        radar_rx_index = 0;
                        radar_rx_buffer[radar_rx_index++] = byte;
                        radar_frame_started = 1;
                    }
                } else {
                    radar_rx_buffer[radar_rx_index++] = byte;
                    
                    /* 防止缓冲区溢出 */
                    if (radar_rx_index >= sizeof(radar_rx_buffer)) {
                        radar_frame_started = 0;
                        printf("[RADAR] Buffer overflow, resetting\r\n");
                        continue;
                    }
                    
                    /* 检查帧尾 0x54 0x43 */
                    if (radar_rx_index >= 2 && 
                        radar_rx_buffer[radar_rx_index-2] == FRAME_TAIL1 && 
                        radar_rx_buffer[radar_rx_index-1] == FRAME_TAIL2) {
                        /* 收到完整帧，进行解析 */
                        parse_radar_frame(radar_rx_buffer, radar_rx_index);
                        /* 重置，准备接收下一帧 */
                        radar_frame_started = 0;
                    }
                }
            }
        }
    }
    
    free(data);
}

/**
 * @brief       雷达数据显示任务（在LCD上显示关键信息）
 * @param       pvParameters: 传入参数
 * @retval      无
 */
void radar_display_task(void *pvParameters)
{
    radar_data_t radar_data = {0};
    QueueHandle_t queue = get_radar_data_queue();
    char display_buffer[64];
    uint8_t has_data = 0;  /* 标记是否收到过数据 */
    
    /* 先固定显示所有数据标题（两列布局） */
    /* 第一列 */
    lcd_show_string(5, 120, 240, 16, 16, "Presence:", BLUE);
    lcd_show_string(5, 140, 240, 16, 16, "Motion:", BLUE);
    lcd_show_string(5, 160, 240, 16, 16, "Breath:", BLUE);
    lcd_show_string(5, 180, 240, 16, 16, "Heart:", BLUE);
    lcd_show_string(5, 200, 240, 16, 16, "Sleep:", BLUE);
    /* 第二列 */
    lcd_show_string(140, 120, 240, 16, 16, "Sleep Score:", BLUE);
    lcd_show_string(140, 140, 240, 16, 16, "Bed:", BLUE);
    lcd_show_string(140, 160, 240, 16, 16, "Distance:", BLUE);
    lcd_show_string(140, 180, 240, 16, 16, "Position:", BLUE);
    
    while (1) {
        /* 从队列接收雷达数据，等待最大500ms */
        if (queue != NULL && xQueueReceive(queue, &radar_data, pdMS_TO_TICKS(500))) {
            has_data = 1;
            
            /* 根据数据类型更新对应的数值区域（数据跟在标题后面） */
            switch (radar_data.data_type) {
                case 0x01:  /* 有人/无人 */
                    sprintf(display_buffer, "%s", radar_data.data.presence ? "Person" : "Nobody");
                    lcd_show_string(80, 120, 240, 16, 16, display_buffer, BLUE);
                    break;
                    
                case 0x02:  /* 运动状态 */
                    {
                        const char *motion_str[] = {"None", "Static", "Active"};
                        uint8_t idx = radar_data.data.motion_state;
                        if (idx > 2) idx = 0;
                        sprintf(display_buffer, "%s", motion_str[idx]);
                        lcd_show_string(80, 140, 240, 16, 16, display_buffer, BLUE);
                    }
                    break;
                    
                case 0x03:  /* 呼吸率 */
                    sprintf(display_buffer, "%d bpm", radar_data.data.breath_rate);
                    lcd_show_string(80, 160, 240, 16, 16, display_buffer, BLUE);
                    break;
                    
                case 0x04:  /* 心率 */
                    sprintf(display_buffer, "%d bpm", radar_data.data.heart_rate);
                    lcd_show_string(80, 180, 240, 16, 16, display_buffer, BLUE);
                    break;
                    
                case 0x05:  /* 睡眠状态 */
                    {
                        const char *sleep_str[] = {"Deep Sleep", "Light Sleep", "Awake", "Out Bed"};
                        uint8_t idx = radar_data.data.sleep_state;
                        if (idx > 3) idx = 0;
                        sprintf(display_buffer, "%s", sleep_str[idx]);
                        lcd_show_string(80, 200, 240, 16, 16, display_buffer, BLUE);
                    }
                    break;
                    
                case 0x06:  /* 睡眠评分 */
                    sprintf(display_buffer, "%d", radar_data.data.sleep_score);
                    lcd_show_string(240, 120, 80, 16, 16, display_buffer, BLUE);
                    break;
                    
                case 0x0A:  /* 入床/离床 */
                    sprintf(display_buffer, "%s", radar_data.data.bed_state ? "In Bed" : "Out Bed");
                    lcd_show_string(185, 140, 120, 16, 16, display_buffer, BLUE);
                    break;
                    
                case 0x08:  /* 人体距离 */
                    sprintf(display_buffer, "%d cm", radar_data.data.distance);
                    lcd_show_string(215, 160, 80, 16, 16, display_buffer, BLUE);
                    break;
                    
				case 0x07:  /* 人体方位 */
					sprintf(display_buffer, "X=%d cm", radar_data.data.position.x);
					lcd_show_string(215, 180, 150, 16, 16, display_buffer, BLUE);
					sprintf(display_buffer, "Y=%d cm", radar_data.data.position.y);
					lcd_show_string(215, 196, 150, 16, 16, display_buffer, BLUE);
					sprintf(display_buffer, "Z=%d cm", radar_data.data.position.z);
					lcd_show_string(215, 212, 150, 16, 16, display_buffer, BLUE);
					break;
                    
                default:
                    break;
            }
        } else {
            /* 超时显示等待信息 */
            if (!has_data) {
                lcd_show_string(260, 120, 80, 16, 16, "Wait", BLUE);
            }
        }
    }
}