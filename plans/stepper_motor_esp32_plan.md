# ESP32 驱动 28BYJ48 步进电机 - 实施计划

## 概述
使用 ESP32 DevKit V1 和 ESP-IDF 框架驱动 28BYJ48 步进电机，实现正转、反转、停止和速度可调功能。

## 硬件配置

### ESP32 DevKit V1 引脚分配（30针版本）
| 功能       | GPIO | 备注         |
|-----------|------|-------------|
| Motor IN1 | 16   | 蓝色线       |
| Motor IN2 | 17   | 粉色线       |
| Motor IN3 | 18   | 黄色线       |
| Motor IN4 | 19   | 橙色线       |
| GND       | GND  | 共地         |

### 28BYJ48 电机参数
- 电压：5V
- 相数：4相
- 步距角：5.625°/64
- 减速比：1:64
- 接线：5线4相

## 软件设计

### 头文件 stepper_motor.h
```c
#ifndef STEPPER_MOTOR_H
#define STEPPER_MOTOR_H

#include "esp_err.h"
#include "driver/gpio.h"

// 电机配置结构体
typedef struct {
    gpio_num_t in1_pin;
    gpio_num_t in2_pin;
    gpio_num_t in3_pin;
    gpio_num_t in4_pin;
    uint32_t step_delay_us;  // 步进延时（微秒）
} stepper_config_t;

// 电机方向
typedef enum {
    STEPPER_CW  = 0,   // 顺时针
    STEPPER_CCW = 1    // 逆时针
} stepper_direction_t;

// 电机状态
typedef enum {
    STEPPER_IDLE = 0,
    STEPPER_RUNNING,
    STEPPER_STOPPED
} stepper_state_t;

// 函数声明
esp_err_t stepper_init(const stepper_config_t *config);
esp_err_t stepper_set_speed(uint32_t rpm);
esp_err_t stepper_rotate(stepper_direction_t direction, uint32_t steps);
esp_err_t stepper_stop(void);
stepper_state_t stepper_get_state(void);
void stepper_deinit(void);

#endif // STEPPER_MOTOR_H
```

### 驱动文件 stepper_motor.c
- 实现 GPIO 初始化
- 实现 4相8拍控制算法
- 实现速度调节功能
- 实现正反转控制

### 主程序 main.c
- 初始化步进电机
- 创建电机控制任务
- 响应用户命令（通过串口或按键）

## 控制算法

### 28BYJ48 8拍励磁顺序
```
1: IN1=1, IN2=0, IN3=0, IN4=0
2: IN1=1, IN2=1, IN3=0, IN4=0
3: IN1=0, IN2=1, IN3=0, IN4=0
4: IN1=0, IN2=1, IN3=1, IN4=0
5: IN1=0, IN2=0, IN3=1, IN4=0
6: IN1=0, IN2=0, IN3=1, IN4=1
7: IN1=0, IN2=0, IN3=0, IN4=1
8: IN1=1, IN2=0, IN3=0, IN4=1
```

### 速度计算
- 基础延时 = 60,000,000 / (RPM × 步数/转 × 减速比)
- 28BYJ48 实际每转步数 = 64 × 64 = 4096 步

## 使用示例
```c
// 配置电机
stepper_config_t motor_config = {
    .in1_pin = GPIO_NUM_16,
    .in2_pin = GPIO_NUM_17,
    .in3_pin = GPIO_NUM_18,
    .in4_pin = GPIO_NUM_19,
    .step_delay_us = 1000,
};

// 初始化
stepper_init(&motor_config);

// 设置转速为15 RPM
stepper_set_speed(15);

// 正转100步
stepper_rotate(STEPPER_CW, 100);

// 停止
stepper_stop();
```

## 下一步
1. 确认硬件接线方案
2. 创建 ESP-IDF 项目结构
3. 编写驱动程序代码
4. 测试验证功能
