/**
 * @file buzzer.hpp
 * @brief 设备层：蜂鸣器（基于 BspPwm 输出方波，占空比 50% 发声）
 *
 * 为什么在 device 层而不是 bsp 层：
 *   bsp 层只负责“把一个定时器通道变成 PWM 输出”，蜂鸣器是**器件语义**
 *   （开/关、周期性提示音），会随板上器件更换（无源蜂鸣器走 PWM、有源蜂鸣器走 GPIO）。
 *
 * 使用示例：
 * @code
 *   DeviceBuzzer buzzer;
 *   buzzer.init({&bsp_pwm_buzzer, 0, PWM_G12_BUZZER_INST_CLK_FREQ / 2700U});
 *   buzzer.on();   // 2700 Hz 发声
 *   buzzer.off();
 * @endcode
 */

#ifndef __M0BSP_DEVICE_BUZZER_HPP__
#define __M0BSP_DEVICE_BUZZER_HPP__

#include <stdint.h>

#include "bsp/bsp_pwm.hpp"

class DeviceBuzzer
{
public:
  /** @brief 初始化配置 */
  struct Config
  {
    BspPwm  *pwm;    ///< 已绑定的 PWM 对象
    uint8_t  ch;     ///< 该 PWM 对象里用于蜂鸣器的通道号
    uint32_t period; ///< PWM 周期（= 定时器时钟 / 目标音调频率）
  };

  /**
   * @brief 绑定 PWM 通道
   * @param cfg 配置
   * @return true=成功，false=参数无效（pwm 为空 / period 为 0）
   */
  bool init(const Config &cfg);

  /** @brief 发声（占空比 50%） */
  void on();

  /** @brief 停止发声（占空比 0） */
  void off();

  /** @brief 当前是否在发声 */
  bool is_on() const { return _on; }

private:
  BspPwm  *_pwm    = nullptr;
  uint8_t  _ch     = 0;
  uint32_t _period = 0;
  bool     _on     = false;
};

#endif /* __M0BSP_DEVICE_BUZZER_HPP__ */
