/**
 * @file bsp_encoder_exti.cpp
 * @brief 4 倍频正交解码（查表法）
 */

#include "bsp/bsp_encoder_exti.hpp"

namespace
{
  /**
   * @brief 状态转移表：索引 = (prev << 2) | cur，值 = ±1 / 0
   *
   * 状态 = (A<<1)|B；正转 00 → 01 → 11 → 10 → 00（每次 +1）
   * 斜向跳变（如同一状态、或两相同时变化）返回 0，自动忽略毛刺。
   */
  constexpr int8_t kTransition[16] = {
      /* prev=00 */ 0, +1, -1, 0,
      /* prev=01 */ -1, 0, 0, +1,
      /* prev=10 */ +1, 0, 0, -1,
      /* prev=11 */ 0, -1, +1, 0,
  };
} // namespace

uint8_t BspEncoderExti::read_state() const
{
  uint32_t a = (DL_GPIO_readPins(_cfg.port, _cfg.pin_a) != 0U) ? 1U : 0U;
  uint32_t b = (DL_GPIO_readPins(_cfg.port, _cfg.pin_b) != 0U) ? 1U : 0U;

  if (_cfg.invert_a)
  {
    a ^= 1U;
  }
  if (_cfg.invert_b)
  {
    b ^= 1U;
  }

  return (uint8_t)((a << 1) | b);
}

bool BspEncoderExti::init(const Config &cfg)
{
  if (cfg.port == nullptr || cfg.pin_a == 0U || cfg.pin_b == 0U)
  {
    return false;
  }

  _cfg   = cfg;
  _count = 0;
  _delta = 0;
  _state = read_state();
  _ready = true;
  return true;
}

void BspEncoderExti::on_edge()
{
  if (!_ready)
  {
    return;
  }

  const uint8_t s = read_state();
  if (s == _state)
  {
    /* 同一状态重复进入（毛刺 / 另一相还没变） */
    return;
  }

  int8_t step = kTransition[(_state << 2) | s];
  _state      = s;

  if (_cfg.invert_dir)
  {
    step = (int8_t)(-step);
  }

  if (step != 0)
  {
    _count = _count + step;
    _delta = _delta + step;
  }
}

int32_t BspEncoderExti::take_delta()
{
  const uint32_t primask = __get_PRIMASK();
  __disable_irq();
  const int32_t d = _delta;
  _delta          = 0;
  __set_PRIMASK(primask);
  return d;
}

void BspEncoderExti::reset(int32_t value)
{
  const uint32_t primask = __get_PRIMASK();
  __disable_irq();
  _count = value;
  _delta = 0;
  _state = read_state();
  __set_PRIMASK(primask);
}
