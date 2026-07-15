# ESP32-S3 毫米波雷达智能睡眠监测系统
实验室自研物联网设备，提供两套完全独立的硬件实现方案，分别验证不同显示外设方案，均实现雷达数据采集、FreeRTOS调度、LVGL界面、BLE配网、MQTT云端上报完整功能。

## 通用技术栈
ESP32-S3 | ESP-IDF | FreeRTOS | LVGL | MQTT | BLE配网 | UART雷达协议解析 | I2C/SPI/I80外设驱动

## 编译使用
进入任意一套工程目录执行：
idf.py build flash monitor