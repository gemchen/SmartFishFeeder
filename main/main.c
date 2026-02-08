/**
 * @file main.c
 * @brief SG90舵机控制示例程序
 * 
 * 使用ESP32 MCPWM外设控制Tower Pro SG90舵机
 * 实现0° -> 90° -> 180° -> 90° -> 0°的循环转动
 * 支持WiFi连接和网络TCP控制信号(0-9)
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "sg90_servo.h"
#include "wifi_config.h"
#include "tcp_server.h"

static const char *TAG = "MAIN";

// 舵机配置
#define SERVO_SIGNAL_PIN    GPIO_NUM_17
#define TCP_SERVER_PORT     8080

// 全局舵机配置指针
static sg90_config_t *g_servo_config = NULL;

// 舵机角度映射表 (命令0-9对应角度)
static const uint8_t command_angle_map[10] = {
    0,    // 命令'0' -> 0°
    18,   // 命令'1' -> 18°
    36,   // 命令'2' -> 36°
    54,   // 命令'3' -> 54°
    72,   // 命令'4' -> 72°
    90,   // 命令'5' -> 90°
    108,  // 命令'6' -> 108°
    126,  // 命令'7' -> 126°
    144,  // 命令'8' -> 144°
    180   // 命令'9' -> 180°
};

/**
 * @brief 命令处理回调函数
 */
static void command_handler(char command, int client_fd)
{
    if (command < '0' || command > '9') {
        return;
    }
    
    uint8_t angle = command_angle_map[command - '0'];
    
    ESP_LOGI(TAG, "收到命令: %c -> 角度: %d°", command, angle);
    
    // 控制舵机转动
    if (g_servo_config != NULL) {
        // 设置目标角度，1秒后自动复位到0°
        sg90_set_angle_with_reset(g_servo_config, angle, 1000);
        ESP_LOGI(TAG, "舵机已转动到 %d°，1秒后将自动复位", angle);
        
        // 发送响应
        char response[64];
        snprintf(response, sizeof(response), "OK: Command %c -> Angle %d° (auto reset in 1s)\n", command, angle);
        tcp_server_send_response(client_fd, response);
    } else {
        char response[64];
        snprintf(response, sizeof(response), "ERROR: Servo not initialized\n");
        tcp_server_send_response(client_fd, response);
    }
}

/**
 * @brief 舵机控制任务
 */
void servo_control_task(void *pvParameters)
{
    ESP_LOGI(TAG, "舵机控制任务启动");
    
    // 声明舵机配置变量
    sg90_config_t servo_config = {
        .signal_pin = SERVO_SIGNAL_PIN,
        .timer = NULL,
        .oper = NULL,
        .comparator = NULL,
        .generator = NULL,
        .min_pulse_width_us = 500.0f,   // 0.5ms for 0°
        .max_pulse_width_us = 2500.0f,  // 2.5ms for 180°
    };
    
    g_servo_config = &servo_config;
    
    // 初始化舵机
    ESP_ERROR_CHECK(sg90_init(&servo_config));
    
    // 延时等待舵机稳定
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    ESP_LOGI(TAG, "舵机初始化完成，信号引脚: GPIO%d", SERVO_SIGNAL_PIN);
    ESP_LOGI(TAG, "开始等待WiFi连接...");
    
    // 等待WiFi连接
    esp_err_t ret = wifi_wait_connected(30000);  // 等待30秒
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "WiFi连接超时，继续运行独立模式");
    }
    
    // 初始化TCP服务器
    ESP_LOGI(TAG, "初始化TCP服务器，端口: %d", TCP_SERVER_PORT);
    ESP_ERROR_CHECK(tcp_server_init(TCP_SERVER_PORT));
    
    // 注册命令回调
    tcp_server_register_command_callback(command_handler);
    
    // 启动TCP服务器
    ESP_ERROR_CHECK(tcp_server_start());
    
    const char* ip_addr = wifi_get_ip_address();
    ESP_LOGI(TAG, "=================================================");
    ESP_LOGI(TAG, "SmartFishFeeder 服务已就绪");
    ESP_LOGI(TAG, "WiFi状态: %s", wifi_is_connected() ? "已连接" : "未连接");
    if (wifi_is_connected()) {
        ESP_LOGI(TAG, "IP地址: %s", ip_addr);
    }
    ESP_LOGI(TAG, "TCP服务器端口: %d", TCP_SERVER_PORT);
    ESP_LOGI(TAG, "=================================================");
    ESP_LOGI(TAG, "发送命令 '0'-'9' 控制舵机角度 (0°-180°)");
    
    // 延时等待TCP服务器启动
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // 默认转动到0°
    sg90_set_angle(&servo_config, 0);
    ESP_LOGI(TAG, "舵机初始角度: 0°");
    
    // 主循环 - 舵机控制
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));  // 降低频率，减少刷屏
    }
}

/**
 * @brief WiFi事件回调
 */
static void wifi_event_handler(void* handler_arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (strcmp(event_base, WIFI_EVENT) == 0) {
        switch (event_id) {
            case WIFI_EVENT_STA_CONNECTED:
                ESP_LOGI(TAG, "WiFi已连接到热点");
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                ESP_LOGW(TAG, "WiFi已断开连接");
                break;
        }
    } else if (strcmp(event_base, IP_EVENT) == 0) {
        switch (event_id) {
            case IP_EVENT_STA_GOT_IP:
                ESP_LOGI(TAG, "获取到IP地址，可以访问TCP服务器了");
                break;
        }
    }
}

/**
 * @brief 程序入口
 */
void app_main(void)
{
    ESP_LOGI(TAG, "=================================================");
    ESP_LOGI(TAG, "SmartFishFeeder - ESP32 智能喂鱼器");
    ESP_LOGI(TAG, "硬件: ESP32 DevKit V1 + Tower Pro SG90");
    ESP_LOGI(TAG, "功能: WiFi连接 + TCP网络控制");
    ESP_LOGI(TAG, "=================================================");
    
    // 注册WiFi事件回调
    wifi_register_event_callback(wifi_event_handler, NULL);
    
    // 初始化WiFi (从编译宏或环境变量读取配置)
#ifdef WIFI_SSID
    ESP_LOGI(TAG, "正在连接WiFi: %s", WIFI_SSID);
#else
    ESP_LOGI(TAG, "正在连接WiFi（未配置编译宏）");
#endif
    esp_err_t ret = wifi_init_sta(NULL, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi初始化失败: %s", esp_err_to_name(ret));
    }
    
    // 创建舵机控制任务
    xTaskCreate(
        servo_control_task,     // 任务函数
        "servo_control",       // 任务名称
        8192,                   // 堆栈大小 (增大以支持网络功能)
        NULL,                   // 参数
        5,                      // 优先级
        NULL                    // 任务句柄
    );
}
