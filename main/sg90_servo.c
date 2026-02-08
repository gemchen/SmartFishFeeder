/**
 * @file sg90_servo.c
 * @brief SG90 舵机驱动实现
 * 
 * 使用ESP32 MCPWM外设控制Tower Pro SG90舵机
 * 使用新的mcpwm_prelude.h API (ESP-IDF v5+)
 */

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "sg90_servo.h"
#include "esp_log.h"

static const char *TAG = "SG90_SERVO";

// SG90 脉冲参数
#define SG90_FREQUENCY_HZ      50      // 50Hz = 20ms周期
#define SG90_RESOLUTION_HZ     1000000 // 1MHz分辨率 = 1us
#define SG90_PERIOD_TICKS      20000   // 20000 ticks @ 1MHz = 20ms = 50Hz

esp_err_t sg90_init(const sg90_config_t *config)
{
    ESP_LOGI(TAG, "初始化SG90舵机");
    ESP_LOGI(TAG, "信号引脚: GPIO%d", config->signal_pin);
    ESP_LOGI(TAG, "脉冲宽度范围: %.1f-%.1fus", 
             config->min_pulse_width_us, 
             config->max_pulse_width_us);
    
    // 1. 配置GPIO
    gpio_config_t io_conf = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << config->signal_pin),
        .pull_down_en = 0,
        .pull_up_en = 0,
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    gpio_set_level(config->signal_pin, 0);
    
    // 2. 创建MCPWM定时器
    mcpwm_timer_config_t timer_config = {
        .group_id = 0,
        .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
        .resolution_hz = SG90_RESOLUTION_HZ,  // 1MHz分辨率 = 1us
        .count_mode = MCPWM_TIMER_COUNT_MODE_UP,
        .period_ticks = SG90_PERIOD_TICKS,      // 20ms = 50Hz
    };
    ESP_ERROR_CHECK(mcpwm_new_timer(&timer_config, &config->timer));
    
    // 3. 创建MCPWM操作符
    mcpwm_operator_config_t oper_config = {
        .group_id = 0,
    };
    ESP_ERROR_CHECK(mcpwm_new_operator(&oper_config, &config->oper));
    
    // 4. 连接定时器到操作符
    ESP_ERROR_CHECK(mcpwm_operator_connect_timer(config->oper, config->timer));
    
    // 5. 创建比较器
    mcpwm_comparator_config_t comp_config = {
        .flags.update_cmp_on_tez = true,  // 在定时器计数到0时更新
    };
    ESP_ERROR_CHECK(mcpwm_new_comparator(config->oper, &comp_config, &config->comparator));
    
    // 6. 创建PWM生成器
    mcpwm_generator_config_t gen_config = {
        .gen_gpio_num = config->signal_pin,
    };
    ESP_ERROR_CHECK(mcpwm_new_generator(config->oper, &gen_config, &config->generator));
    
    // 7. 配置生成器动作 - 在计数到0时输出高电平，在比较值时输出低电平
    ESP_ERROR_CHECK(mcpwm_generator_set_actions_on_timer_event(config->generator,
        MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, 
                                      MCPWM_TIMER_EVENT_EMPTY, 
                                      MCPWM_GEN_ACTION_HIGH),
        MCPWM_GEN_TIMER_EVENT_ACTION_END()));
    
    ESP_ERROR_CHECK(mcpwm_generator_set_actions_on_compare_event(config->generator,
        MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP,
                                        config->comparator,
                                        MCPWM_GEN_ACTION_LOW),
        MCPWM_GEN_COMPARE_EVENT_ACTION_END()));
    
    // 8. 启动定时器
    ESP_ERROR_CHECK(mcpwm_timer_enable(config->timer));
    ESP_ERROR_CHECK(mcpwm_timer_start_stop(config->timer, MCPWM_TIMER_START_NO_STOP));
    
    // 9. 设置初始角度为0度
    sg90_set_angle(config, 0.0f);
    
    ESP_LOGI(TAG, "SG90舵机初始化完成");
    return ESP_OK;
}

esp_err_t sg90_set_angle(const sg90_config_t *config, float angle)
{
    // 限制角度范围
    if (angle < 0.0f) {
        angle = 0.0f;
    } else if (angle > 180.0f) {
        angle = 180.0f;
    }
    
    // 根据公式计算脉冲宽度
    // 0° -> min_pulse, 180° -> max_pulse
    float pulse_width = config->min_pulse_width_us + 
                        (angle / 180.0f) * 
                        (config->max_pulse_width_us - config->min_pulse_width_us);
    
    ESP_LOGI(TAG, "设置角度: %.1f° (脉冲宽度: %.1fus)", angle, pulse_width);
    
    // 设置比较值（以微秒为单位）
    ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(config->comparator, 
                                                        (uint32_t)pulse_width));
    
    return ESP_OK;
}

esp_err_t sg90_set_angle_with_reset(const sg90_config_t *config, float angle, uint32_t reset_delay_ms)
{
    esp_err_t ret = ESP_OK;
    
    // 先设置目标角度
    ret = sg90_set_angle(config, angle);
    if (ret != ESP_OK) {
        return ret;
    }
    
    // 延时后自动复位到0°
    ESP_LOGI(TAG, "将在 %ums 后自动复位到 0°", reset_delay_ms);
    vTaskDelay(pdMS_TO_TICKS(reset_delay_ms));
    
    ret = sg90_set_angle(config, 0.0f);
    ESP_LOGI(TAG, "舵机已自动复位到 0°");
    
    return ret;
}

esp_err_t sg90_deinit(sg90_config_t *config)
{
    ESP_LOGI(TAG, "反初始化SG90舵机");
    
    // 停止并删除生成器
    if (config->generator) {
        mcpwm_generator_set_actions_on_timer_event(config->generator,
            MCPWM_GEN_TIMER_EVENT_ACTION_END());
        mcpwm_generator_set_actions_on_compare_event(config->generator,
            MCPWM_GEN_COMPARE_EVENT_ACTION_END());
        mcpwm_del_generator(config->generator);
        config->generator = NULL;
    }
    
    // 删除比较器
    if (config->comparator) {
        mcpwm_del_comparator(config->comparator);
        config->comparator = NULL;
    }
    
    // 删除操作符
    if (config->oper) {
        mcpwm_del_operator(config->oper);
        config->oper = NULL;
    }
    
    // 停止并删除定时器
    if (config->timer) {
        mcpwm_timer_start_stop(config->timer, MCPWM_TIMER_STOP_EMPTY);
        mcpwm_del_timer(config->timer);
        config->timer = NULL;
    }
    
    return ESP_OK;
}
