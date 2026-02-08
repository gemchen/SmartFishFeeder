/**
 * @file wifi_config.h
 * @brief WiFi配置头文件
 * 
 * 提供WiFi连接功能，从环境变量读取SSID和密码
 */

#ifndef WIFI_CONFIG_H
#define WIFI_CONFIG_H

#include <stdbool.h>
#include "esp_err.h"
#include "esp_event.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief WiFi事件回调函数类型
 * @param event_base 事件基类
 * @param event_id 事件ID
 * @param event_data 事件数据
 */
typedef void (*wifi_event_callback_t)(void* event_handler_arg, 
                                       esp_event_base_t event_base,
                                       int32_t event_id,
                                       void* event_data);

/**
 * @brief 初始化WiFi并连接
 * @param ssid WiFi名称 (如果为NULL则从环境变量读取)
 * @param password WiFi密码 (如果为NULL则从环境变量读取)
 * @return ESP_OK 成功，其他值失败
 */
esp_err_t wifi_init_sta(const char* ssid, const char* password);

/**
 * @brief 等待WiFi连接成功
 * @param timeout_ms 超时时间(毫秒)
 * @return ESP_OK 成功，ESP_ERR_TIMEOUT 超时
 */
esp_err_t wifi_wait_connected(uint32_t timeout_ms);

/**
 * @brief 获取WiFi连接状态
 * @return true 已连接，false 未连接
 */
bool wifi_is_connected(void);

/**
 * @brief 获取本地IP地址
 * @return IP地址字符串
 */
const char* wifi_get_ip_address(void);

/**
 * @brief 断开WiFi连接
 */
void wifi_disconnect(void);

/**
 * @brief 注册WiFi事件回调
 * @param callback 回调函数
 * @param handler_arg 回调参数
 * @return ESP_OK 成功
 */
esp_err_t wifi_register_event_callback(wifi_event_callback_t callback, void* handler_arg);

#ifdef __cplusplus
}
#endif

#endif // WIFI_CONFIG_H
