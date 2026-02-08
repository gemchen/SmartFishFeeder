/**
 * @file sg90_servo.h
 * @brief SG90 舵机驱动头文件
 * 
 * 使用ESP32 MCPWM外设控制Tower Pro SG90舵机
 * 使用新的mcpwm_prelude.h API (ESP-IDF v5+)
 */

#ifndef SG90_SERVO_H
#define SG90_SERVO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"
#include "driver/gpio.h"
#include "driver/mcpwm_prelude.h"
#include "soc/mcpwm_periph.h"

typedef struct {
    gpio_num_t signal_pin;          /**< 信号引脚 */
    mcpwm_timer_handle_t timer;     /**< 定时器句柄 */
    mcpwm_oper_handle_t oper;       /**< 操作符句柄 */
    mcpwm_cmpr_handle_t comparator; /**< 比较器句柄 */
    mcpwm_gen_handle_t generator;   /**< 生成器句柄 */
    float min_pulse_width_us;       /**< 最小脉冲宽度（微秒），默认0.5ms */
    float max_pulse_width_us;       /**< 最大脉冲宽度（微秒），默认2.5ms */
} sg90_config_t;

/**
 * @brief 默认SG90配置
 */
#define SG90_DEFAULT_CONFIG(pin, mcpwm_unit, mcpwm_timer) \
    {                                                     \
        .signal_pin = pin,                                \
        .unit = mcpwm_unit,                               \
        .timer = mcpwm_timer,                             \
        .min_pulse_width_us = 500.0f,                     \
        .max_pulse_width_us = 2500.0f,                    \
    }

/**
 * @brief 初始化SG90舵机
 * 
 * @param config 舵机配置
 * @return esp_err_t 初始化结果
 */
esp_err_t sg90_init(const sg90_config_t *config);
esp_err_t sg90_set_angle(const sg90_config_t *config, float angle);
esp_err_t sg90_set_angle_with_reset(const sg90_config_t *config, float angle, uint32_t reset_delay_ms);
esp_err_t sg90_deinit(sg90_config_t *config);

#ifdef __cplusplus
}
#endif

#endif // SG90_SERVO_H
