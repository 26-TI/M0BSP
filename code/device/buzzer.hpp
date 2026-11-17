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
 *   buzzer.on();   // 2700 Hz 方波（50%）
 *   buzzer.off();  // 静默：引脚恒高（100%）
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
    BspPwm  *pwm;                    ///< 已绑定的 PWM 对象
    uint8_t  ch;                     ///< 该 PWM 对象里用于蜂鸣器的通道号
    uint32_t period;                 ///< PWM 周期（= 定时器时钟 / 目标音调频率）
    uint32_t clk_freq = 80000000U;   ///< 定时器输入时钟 (Hz)，用于 set_frequency()
  };

  /**
   * @brief 绑定 PWM 通道
   * @param cfg 配置
   * @return true=成功，false=参数无效（pwm 为空 / period 为 0）
   */
  bool init(const Config &cfg);

  /** @brief 发声（占空比 50%） */
  void on();

  /**
   * @brief 停止发声（占空比 100% → 引脚恒高）
   *
   * @note 静默电平选择高电平的理由：
   *       - 无源蜂鸣器需要交流才响，直流（高或低）都不发声
   *       - 拉高便于用示波器/万用表确认引脚状态
   *       若你的驱动电路是“低电平导通”，发射时同样还是 50% 方波，不受影响。
   */
  void off();

  /**
   * @brief 改变音调频率
   * @param hz 目标频率 (Hz)，0 表示静音
   * @note 正在发声时会立即以新频率继续响
   */
  void set_frequency(uint32_t hz);

  /** @brief 当前是否在发声 */
  bool is_on() const { return _on; }

private:
  BspPwm  *_pwm      = nullptr;
  uint8_t  _ch       = 0;
  uint32_t _period   = 0;
  uint32_t _clk_freq = 80000000U;
  bool     _on       = false;
};

#endif /* __M0BSP_DEVICE_BUZZER_HPP__ */
