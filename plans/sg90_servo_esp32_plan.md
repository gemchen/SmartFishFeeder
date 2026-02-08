# ESP32 驱动 Tower Pro SG90 舵机 - 实施计划

## 概述
使用 ESP32 DevKit V1 和 ESP-IDF 框架控制 Tower Pro SG90 舵机，实现精确的角度控制。

## SG90 舵机参数
- 工作电压：4.8V - 6V
- 角度范围：0° - 180°
- 脉冲周期：20ms (50Hz)
- 脉冲宽度：0.5ms (0°) - 2.5ms (180°)

## 硬件配置

### ESP32 DevKit V1 引脚分配
| 功能       | GPIO  | 备注         |
|-----------|-------|-------------|
| 信号线     | 17    | 黄色/橙色线  |
| VCC        | 5V    | 红色线       |
| GND        | GND   | 棕色线       |

### 接线示意图
```
ESP32          SG90 舵机
-------       --------
GPIO 17  ----  信号线 (黄色/橙色)
5V        ----  VCC (红色)
GND       ----  GND (棕色)
```

### 电源注意事项
- **推荐**：使用外部5V电源给舵机供电，ESP32通过GND与舵机共地
- **注意**：多个舵机或大负载时必须使用外部电源
- 小型SG90短时间测试可直接用ESP32的5V引脚供电

## 软件设计

### 项目结构
```
sg90_servo_project/
├── main/
│   ├── main.c         # 应用入口
│   ├── sg90_servo.c   # 舵机驱动
│   ├── sg90_servo.h   # 舵机头文件
│   └── CMakeLists.txt
├── CMakeLists.txt
└── sdkconfig
```

### sg90_servo.h
```c
#ifndef SG90_SERVO_H
#define SG90_SERVO_H

#include "esp_err.h"
#include "driver/gpio.h"
#include "driver/mcpwm.h"

// 舵机配置结构体
typedef struct {
    gpio_num_t signal_pin;      // 信号引脚
    mcpwm_unit_t unit;          // MCPWM单元
    mcpwm_timer_t timer;        # 定时器
    float min_pulse_width_us;   # 最小脉冲宽度（微秒）
    float max_pulse_width_us;   # 最大脉冲宽度（微秒）
} sg90_config_t;

esp_err_t sg90_init(const sg90_config_t *config);
esp_err_t sg90_set_angle(mcpwm_unit_t unit, mcpwm_timer_t timer, float angle);
esp_err_t sg90_deinit(mcpwm_unit_t unit);

#endif // SG90_SERVO_H
```

### sg90_servo.c
- 实现 MCPWM 初始化
- 实现角度到脉冲宽度的转换
- 实现平滑转动控制（可选）

### 主程序功能
- 初始化舵机
- 实现角度循环测试（0° -> 90° -> 180° -> 90° -> 0°）
- 支持串口命令控制角度

## PWM 信号参数

### SG90 控制信号
- 频率：50Hz (周期20ms)
- 分辨率：10-bit (0-1023)
- 脉冲宽度范围：0.5ms - 2.5ms

### MCPWM 配置
```c
// 配置定时器
mcpwm_config_t pwm_config = {
    .frequency = 50,                    // 50Hz
    .counter_mode = MCPWM_UP_COUNTER,   // 向上计数
    .duty_mode = MCPWM_DUTY_MODE_0,     // 占空比模式0
};
```

### 角度与占空比计算
- 占空比 = (脉冲宽度 / 20ms) × 100%
- 例如：90° = 1.5ms → 1.5/20 × 100 = 7.5%

## 使用示例
```c
// 配置舵机
sg90_config_t servo = {
    .signal_pin = GPIO_NUM_17,
    .unit = MCPWM_UNIT_0,
    .timer = MCPWM_TIMER_0,
    .min_pulse_width_us = 500,   // 0.5ms
    .max_pulse_width_us = 2500,  // 2.5ms
};

// 初始化
sg90_init(&servo);

// 设置角度
sg90_set_angle(MCPWM_UNIT_0, MCPWM_TIMER_0, 90);  // 转到90度

// 延时
vTaskDelay(pdMS_TO_TICKS(1000));

// 转到0度
sg90_set_angle(MCPWM_UNIT_0, MCPWM_TIMER_0, 0);
```

## ESP-IDF 版本要求
- ESP-IDF v4.0 或更高版本
- 需要启用 MCPWM 外设驱动

## 下一步
1. 确认硬件接线方案
2. 创建 ESP-IDF 项目结构
3. 编写舵机驱动程序
4. 测试验证功能
5. 扩展更多控制功能
