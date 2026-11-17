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

  _pwm    = cfg.pwm;
  _ch     = cfg.ch;
  _period = cfg.period;
  _on     = false;

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

  _pwm->set_duty(_ch, 0U);
  _on = false;
}
