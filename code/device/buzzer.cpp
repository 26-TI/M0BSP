/**
 * @file buzzer.cpp
 * @brief 设备层：蜂鸣器实现
 */

#include "device/buzzer.hpp"

bool DeviceBuzzer::init(const Config &cfg)
{
  if (cfg.pwm == nullptr || cfg.period == 0U)
  {
    return false;
  }

  _pwm      = cfg.pwm;
  _ch       = cfg.ch;
  _period   = cfg.period;
  _clk_freq = cfg.clk_freq;
  _on       = false;

  /* 静默时占空比 0 → 引脚恒低 */
  _pwm->set_duty(_ch, 0U);
  return true;
}

void DeviceBuzzer::on()
{
  if (_pwm == nullptr)
  {
    return;
  }

  _pwm->set_duty(_ch, _period / 2U); ///< 50% 占空比方波
  _on = true;
}

void DeviceBuzzer::off()
{
  if (_pwm == nullptr)
  {
    return;
  }

  /* 占空比 0 → 引脚恒低 */
  _pwm->set_duty(_ch, 0U);
  _on = false;
}

void DeviceBuzzer::set_frequency(uint32_t hz)
{
  if (_pwm == nullptr || _clk_freq == 0U)
  {
    return;
  }

  if (hz == 0U)
  {
    off();
    return;
  }

  uint32_t period = _clk_freq / hz;

  /* TIMG/TIMA 是 16 位定时器，装载值超过 65535 会截断产生错误音调 */
  if (period > 0xFFFFU)
  {
    period = 0xFFFFU;
  }

  if (period == 0U || !_pwm->set_period(period))
  {
    return;
  }

  _period = period;
  if (_on)
  {
    _pwm->set_duty(_ch, _period / 2U);
  }
  else
  {
    /* 静默中改频：把 CC 重新清 0，避免旧值残留 */
    _pwm->set_duty(_ch, 0U);
  }
}
