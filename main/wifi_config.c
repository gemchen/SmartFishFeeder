/**
 * @file wifi_config.c
 * @brief WiFi配置实现
 * 
 * 实现WiFi连接功能，支持从环境变量读取配置
 */

#include "wifi_config.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_mac.h"
#include <string.h>

static const char *TAG = "WIFI_CONFIG";

// WiFi配置
static wifi_config_t wifi_config = {
    .sta = {
        .threshold.authmode = WIFI_AUTH_WPA2_PSK,
    },
};

static esp_netif_t *sta_netif = NULL;
static bool wifi_connected = false;
static void (*user_callback)(void*, esp_event_base_t, int32_t, void*) = NULL;
static void* user_callback_arg = NULL;

/**
 * @brief WiFi事件处理
 */
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    switch (event_id) {
        case WIFI_EVENT_STA_START:
            ESP_LOGI(TAG, "WiFi station模式启动");
            break;
            
        case WIFI_EVENT_STA_CONNECTED:
            ESP_LOGI(TAG, "WiFi已连接到AP");
            break;
            
        case WIFI_EVENT_STA_DISCONNECTED:
            ESP_LOGW(TAG, "WiFi断开连接");
            wifi_connected = false;
            // 尝试重新连接
            esp_wifi_connect();
            break;
            
        default:
            break;
    }
    
    // 调用用户回调
    if (user_callback) {
        user_callback(user_callback_arg, event_base, event_id, event_data);
    }
}

/**
 * @brief IP事件处理
 */
static void ip_event_handler(void* arg, esp_event_base_t event_base,
                              int32_t event_id, void* event_data)
{
    switch (event_id) {
        case IP_EVENT_STA_GOT_IP:
            wifi_connected = true;
            esp_ip4_addr_t *ip = &((ip_event_got_ip_t*)event_data)->ip_info.ip;
            ESP_LOGI(TAG, "获取到IP地址: " IPSTR, IP2STR(ip));
            break;
            
        case IP_EVENT_STA_LOST_IP:
            ESP_LOGW(TAG, "IP地址丢失");
            wifi_connected = false;
            break;
            
        default:
            break;
    }
}

/**
 * @brief 从环境变量读取WiFi配置
 * 
 * 优先使用编译时宏定义，其次尝试运行时环境变量
 */
static void load_wifi_config_from_env(void)
{
    // 优先使用编译时宏定义
#ifdef WIFI_SSID
    strncpy((char*)wifi_config.sta.ssid, WIFI_SSID, sizeof(wifi_config.sta.ssid) - 1);
    wifi_config.sta.ssid[sizeof(wifi_config.sta.ssid) - 1] = '\0';
    ESP_LOGI(TAG, "从编译宏读取SSID: %s", WIFI_SSID);
#else
    // 尝试从运行时环境变量读取
    const char* env_ssid = getenv("WIFI_SSID");
    if (env_ssid) {
        strncpy((char*)wifi_config.sta.ssid, env_ssid, sizeof(wifi_config.sta.ssid) - 1);
        wifi_config.sta.ssid[sizeof(wifi_config.sta.ssid) - 1] = '\0';
        ESP_LOGI(TAG, "从环境变量读取SSID: %s", env_ssid);
    } else {
        ESP_LOGW(TAG, "未设置WIFI_SSID（编译宏或环境变量）");
    }
#endif
    
#ifdef WIFI_PASSWORD
    strncpy((char*)wifi_config.sta.password, WIFI_PASSWORD, sizeof(wifi_config.sta.password) - 1);
    wifi_config.sta.password[sizeof(wifi_config.sta.password) - 1] = '\0';
    ESP_LOGI(TAG, "从编译宏读取密码成功");
#else
    // 尝试从运行时环境变量读取
    const char* env_password = getenv("WIFI_PASSWORD");
    if (env_password) {
        strncpy((char*)wifi_config.sta.password, env_password, sizeof(wifi_config.sta.password) - 1);
        wifi_config.sta.password[sizeof(wifi_config.sta.password) - 1] = '\0';
        ESP_LOGI(TAG, "从环境变量读取密码成功");
    } else {
        ESP_LOGW(TAG, "未设置WIFI_PASSWORD（编译宏或环境变量）");
    }
#endif
}

esp_err_t wifi_init_sta(const char* ssid, const char* password)
{
    ESP_LOGI(TAG, "开始初始化WiFi station模式");
    
    // 检查WiFi是否已经初始化
    static bool wifi_initialized = false;
    if (wifi_initialized) {
        ESP_LOGW(TAG, "WiFi已经初始化过，跳过初始化");
        return ESP_OK;
    }
    
    // 初始化NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // 初始化TCP/IP栈
    ESP_LOGI(TAG, "初始化TCP/IP栈...");
    ret = esp_netif_init();
    ESP_LOGI(TAG, "esp_netif_init() 返回: %s", esp_err_to_name(ret));
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(ret);
    }
    
    // 创建默认事件循环（在ESP-IDF v5.x中需要）
    ESP_LOGI(TAG, "创建默认事件循环...");
    ret = esp_event_loop_create_default();
    ESP_LOGI(TAG, "esp_event_loop_create_default() 返回: %s", esp_err_to_name(ret));
    if (ret == ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "事件循环已存在");
    }
    
    // 检查WiFi驱动状态
    ESP_LOGI(TAG, "检查WiFi驱动状态...");
    wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&init_config);
    ESP_LOGI(TAG, "esp_wifi_init() 返回: %s", esp_err_to_name(ret));
    if (ret == ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "WiFi已初始化，跳过wifi_init");
    } else {
        ESP_ERROR_CHECK(ret);
    }
    
    // 检查是否已有netif存在
    ESP_LOGI(TAG, "检查现有netif实例...");
    esp_netif_t* existing_netif = esp_netif_get_default_netif();
    if (existing_netif) {
        ESP_LOGW(TAG, "警告: 已存在默认netif实例 %p", existing_netif);
        // 如果已存在netif，直接使用它
        sta_netif = existing_netif;
    } else {
        // 创建WiFi station网络接口
        ESP_LOGI(TAG, "创建WiFi station网络接口...");
        sta_netif = esp_netif_create_default_wifi_sta();
        if (sta_netif == NULL) {
            ESP_LOGE(TAG, "创建WiFi station接口失败");
            return ESP_FAIL;
        }
        ESP_LOGI(TAG, "WiFi station接口创建成功: %p", sta_netif);
    }
    
    // 注册事件处理
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, 
                                &wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, 
                                &ip_event_handler, NULL);
    
    wifi_initialized = true;
    
    // 设置WiFi配置
    if (ssid && password) {
        // 使用提供的SSID和密码
        strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
        wifi_config.sta.ssid[sizeof(wifi_config.sta.ssid) - 1] = '\0';
        strncpy((char*)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
        wifi_config.sta.password[sizeof(wifi_config.sta.password) - 1] = '\0';
        ESP_LOGI(TAG, "使用提供的SSID: %s", ssid);
    } else {
        // 从环境变量读取
        load_wifi_config_from_env();
    }
    
    // 检查SSID是否有效
    if (strlen((char*)wifi_config.sta.ssid) == 0) {
        ESP_LOGE(TAG, "WiFi SSID为空，请设置WIFI_SSID环境变量");
        return ESP_ERR_INVALID_ARG;
    }
    
    // 设置WiFi模式和配置
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    
    // 启动WiFi
    ESP_ERROR_CHECK(esp_wifi_start());
    
    // 开始连接
    ESP_ERROR_CHECK(esp_wifi_connect());
    
    ESP_LOGI(TAG, "WiFi station初始化完成，正在连接...");
    
    return ESP_OK;
}

esp_err_t wifi_wait_connected(uint32_t timeout_ms)
{
    uint32_t tick_start = xTaskGetTickCount();
    
    while (!wifi_connected) {
        vTaskDelay(pdMS_TO_TICKS(100));
        
        // 检查超时
        if ((xTaskGetTickCount() - tick_start) * portTICK_PERIOD_MS > timeout_ms) {
            ESP_LOGW(TAG, "WiFi连接超时");
            return ESP_ERR_TIMEOUT;
        }
    }
    
    return ESP_OK;
}

bool wifi_is_connected(void)
{
    return wifi_connected;
}

const char* wifi_get_ip_address(void)
{
    if (sta_netif && wifi_connected) {
        esp_netif_ip_info_t ip_info;
        if (esp_netif_get_ip_info(sta_netif, &ip_info) == ESP_OK) {
            static char ip_str[16];
            snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&ip_info.ip));
            return ip_str;
        }
    }
    return "0.0.0.0";
}

void wifi_disconnect(void)
{
    if (wifi_connected) {
        esp_wifi_disconnect();
        wifi_connected = false;
        ESP_LOGI(TAG, "WiFi已断开连接");
    }
}

esp_err_t wifi_register_event_callback(wifi_event_callback_t callback, void* handler_arg)
{
    user_callback = callback;
    user_callback_arg = handler_arg;
    return ESP_OK;
}
