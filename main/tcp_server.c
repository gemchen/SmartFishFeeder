/**
 * @file tcp_server.c
 * @brief TCP服务器实现
 * 
 * 实现TCP服务器功能，用于接收控制信号(0-9)
 */

#include "tcp_server.h"
#include "esp_log.h"
#include "esp_err.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

static const char *TAG = "TCP_SERVER";

#define TCP_SERVER_BACKLOG     5      // 连接队列长度
#define TCP_SERVER_BUFFER_SIZE 64     // 接收缓冲区大小

// TCP服务器状态
typedef struct {
    int server_fd;              // 服务器socket描述符
    uint16_t port;              // 服务器端口
    bool running;               // 运行状态
    command_callback_t callback; // 命令回调函数
} tcp_server_state_t;

static tcp_server_state_t server_state = {
    .server_fd = -1,
    .port = 0,
    .running = false,
    .callback = NULL,
};

/**
 * @brief 设置socket为非阻塞模式
 */
static int set_socket_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        return -1;
    }
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

/**
 * @brief TCP服务器任务
 * 处理客户端连接和命令接收
 */
static void tcp_server_task(void *pvParameters)
{
    ESP_LOGI(TAG, "TCP服务器任务启动");
    
    server_state.running = true;
    
    while (server_state.running) {
        // 接受客户端连接
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        int client_fd = accept(server_state.server_fd, 
                               (struct sockaddr*)&client_addr, 
                               &client_len);
        
        if (client_fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                vTaskDelay(pdMS_TO_TICKS(100));
                continue;
            }
            ESP_LOGE(TAG, "接受连接失败: %s", strerror(errno));
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        
        ESP_LOGI(TAG, "客户端连接成功: fd=%d", client_fd);
        
        // 获取客户端信息
        char client_ip[16];
        inet_ntoa_r(client_addr.sin_addr, client_ip, sizeof(client_ip));
        ESP_LOGI(TAG, "客户端IP: %s, 端口: %d", client_ip, ntohs(client_addr.sin_port));
        
        // 接收数据
        char buffer[TCP_SERVER_BUFFER_SIZE];
        ssize_t received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
        
        if (received > 0) {
            buffer[received] = '\0';
            ESP_LOGI(TAG, "收到数据: %s (长度: %zd)", buffer, received);
            
            // 处理每个字符命令 (0-9)
            for (int i = 0; i < received; i++) {
                char cmd = buffer[i];
                
                // 检查是否为有效命令 (0-9)
                if (cmd >= '0' && cmd <= '9') {
                    ESP_LOGI(TAG, "收到有效命令: %c", cmd);
                    
                    // 调用回调函数
                    if (server_state.callback) {
                        server_state.callback(cmd, client_fd);
                    }
                } else if (cmd == '\n' || cmd == '\r') {
                    // 忽略换行符
                } else {
                    ESP_LOGW(TAG, "收到无效命令: %c (0x%02x)", cmd, cmd);
                }
            }
        } else if (received == 0) {
            ESP_LOGI(TAG, "客户端关闭连接");
        } else {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                ESP_LOGE(TAG, "接收数据失败: %s", strerror(errno));
            }
        }
        
        // 关闭客户端连接
        close(client_fd);
        ESP_LOGI(TAG, "客户端连接已关闭: fd=%d", client_fd);
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    ESP_LOGI(TAG, "TCP服务器任务退出");
    vTaskDelete(NULL);
}

esp_err_t tcp_server_init(uint16_t port)
{
    ESP_LOGI(TAG, "初始化TCP服务器，端口: %d", port);
    
    // 检查端口范围
    if (port == 0) {
        port = 8080;  // 默认端口
    }
    
    server_state.port = port;
    server_state.server_fd = -1;
    
    // 创建服务器socket
    server_state.server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_state.server_fd < 0) {
        ESP_LOGE(TAG, "创建socket失败: %s", strerror(errno));
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Socket创建成功: fd=%d", server_state.server_fd);
    
    // 设置socket选项，允许地址复用
    int opt = 1;
    if (setsockopt(server_state.server_fd, SOL_SOCKET, SO_REUSEADDR, 
                   &opt, sizeof(opt)) < 0) {
        ESP_LOGE(TAG, "设置SO_REUSEADDR失败: %s", strerror(errno));
    }
    
    // 绑定地址
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    
    if (bind(server_state.server_fd, (struct sockaddr*)&server_addr, 
              sizeof(server_addr)) < 0) {
        ESP_LOGE(TAG, "绑定地址失败: %s", strerror(errno));
        close(server_state.server_fd);
        server_state.server_fd = -1;
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "地址绑定成功");
    
    // 设置监听
    if (listen(server_state.server_fd, TCP_SERVER_BACKLOG) < 0) {
        ESP_LOGE(TAG, "监听失败: %s", strerror(errno));
        close(server_state.server_fd);
        server_state.server_fd = -1;
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "TCP服务器初始化完成，监听端口 %d", port);
    
    return ESP_OK;
}

esp_err_t tcp_server_start(void)
{
    if (server_state.server_fd < 0) {
        ESP_LOGE(TAG, "服务器未初始化");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (server_state.running) {
        ESP_LOGW(TAG, "服务器已经在运行");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "启动TCP服务器任务");
    
    // 创建服务器任务
    BaseType_t ret = xTaskCreate(
        tcp_server_task,           // 任务函数
        "tcp_server",             // 任务名称
        4096,                      // 堆栈大小
        NULL,                      // 参数
        5,                         // 优先级
        NULL                       // 任务句柄
    );
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "创建任务失败");
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

void tcp_server_stop(void)
{
    ESP_LOGI(TAG, "停止TCP服务器");
    
    server_state.running = false;
    
    // 关闭服务器socket
    if (server_state.server_fd >= 0) {
        close(server_state.server_fd);
        server_state.server_fd = -1;
    }
    
    ESP_LOGI(TAG, "TCP服务器已停止");
}

void tcp_server_register_command_callback(command_callback_t callback)
{
    server_state.callback = callback;
    ESP_LOGI(TAG, "命令回调已注册");
}

bool tcp_server_is_running(void)
{
    return server_state.running && server_state.server_fd >= 0;
}

uint16_t tcp_server_get_port(void)
{
    return server_state.port;
}

int tcp_server_send_response(int client_fd, const char* response)
{
    if (client_fd < 0 || response == NULL) {
        return -1;
    }
    
    ssize_t sent = send(client_fd, response, strlen(response), 0);
    if (sent < 0) {
        ESP_LOGE(TAG, "发送响应失败: %s", strerror(errno));
        return -1;
    }
    
    ESP_LOGI(TAG, "发送响应成功: %s (长度: %zd)", response, sent);
    return sent;
}
