/**
 * @file bsp_gpio_irq.cpp
 * @brief GPIO 边沿中断分发实现
 */

#include "bsp/bsp_gpio_irq.hpp"

BspGpioIrq::Entry   BspGpioIrq::_entries[MAX_ENTRIES];
uint8_t             BspGpioIrq::_count = 0U;

bool BspGpioIrq::attach(const Entry &e)
{
  if (e.port == nullptr || e.pin == 0U || e.cb == nullptr)
  {
    return false;
  }

  /* 同一引脚重复注册：覆盖旧回调，避免表被占满 */
  for (uint8_t i = 0U; i < _count; ++i)
  {
    if (_entries[i].port == e.port && _entries[i].pin == e.pin)
    {
      _entries[i] = e;
      return true;
    }
  }

  if (_count >= MAX_ENTRIES)
  {
    return false;
  }

  _entries[_count++] = e;
  return true;
}

void BspGpioIrq::detach(GPIO_Regs *port, uint32_t pin)
{
  for (uint8_t i = 0U; i < _count; ++i)
  {
    if (_entries[i].port == port && _entries[i].pin == pin)
    {
      _entries[i] = _entries[_count - 1U];
      --_count;
      return;
    }
  }
}

void BspGpioIrq::dispatch(GPIO_Regs *port)
{
  if (port == nullptr)
  {
    return;
  }

  /* 只处理"已使能"的挂起位，读操作不清标志，必须显式 clear */
  const uint32_t status = DL_GPIO_getEnabledInterruptStatus(port, 0xFFFFFFFFU);
  if (status == 0U)
  {
    return;
  }

  for (uint8_t i = 0U; i < _count; ++i)
  {
    const Entry &e = _entries[i];
    if (e.port != port || (status & e.pin) == 0U)
    {
      continue;
    }

    DL_GPIO_clearInterruptStatus(port, e.pin);
    e.cb(e.ctx);
  }

  /* 没有注册回调的引脚也要清掉，否则中断会一直重入 */
  DL_GPIO_clearInterruptStatus(port, status);
}
