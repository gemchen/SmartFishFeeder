/**
 * @file tcp_server.h
 * @brief TCP服务器头文件
 * 
 * 提供TCP服务器功能，用于接收控制信号(0-9)
 */

#ifndef TCP_SERVER_H
#define TCP_SERVER_H

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 控制信号回调函数类型
 * @param command 接收到的命令字符 ('0'-'9')
 * @param client_fd 客户端socket描述符
 */
typedef void (*command_callback_t)(char command, int client_fd);

/**
 * @brief 初始化TCP服务器
 * @param port 服务器端口号
 * @return ESP_OK 成功，其他值失败
 */
esp_err_t tcp_server_init(uint16_t port);

/**
 * @brief 启动TCP服务器监听
 * @return ESP_OK 成功
 */
esp_err_t tcp_server_start(void);

/**
 * @brief 停止TCP服务器
 */
void tcp_server_stop(void);

/**
 * @brief 注册控制命令回调
 * @param callback 回调函数
 */
void tcp_server_register_command_callback(command_callback_t callback);

/**
 * @brief 获取服务器运行状态
 * @return true 服务器运行中，false 已停止
 */
bool tcp_server_is_running(void);

/**
 * @brief 获取服务器端口号
 * @return 端口号
 */
uint16_t tcp_server_get_port(void);

/**
 * @brief 发送响应到客户端
 * @param client_fd 客户端socket描述符
 * @param response 响应字符串
 * @return 发送的字节数，负值表示错误
 */
int tcp_server_send_response(int client_fd, const char* response);

#ifdef __cplusplus
}
#endif

#endif // TCP_SERVER_H
