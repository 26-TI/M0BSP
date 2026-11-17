/**
 * @file bsp_encoder_exti.hpp
 * @brief 4 倍频正交编码器 —— 用 GPIO 外部中断实现（不占用 QEI 外设）
 *
 * 硬件要求（SysConfig）：
 *   - A/B 两相都配置成**输入**，并且**都开双边沿中断**（RISE_FALL）
 *   - A/B 必须在同一个 GPIO 端口（同一端口共用 GROUP1 中断，跨端口也能用，只是要多挂一次）
 *
 * 原理：A/B 任意一相跳变都进中断，读当前 (A,B) 状态，查表得到 ±1/0：
 *
 *   状态 = (A<<1)|B，正转序列 00 → 01 → 11 → 10 → 00，每步 +1，反转每步 -1。
 *   两个通道都双边沿 → 一个电气周期 4 个计数（4 倍频）。
 *
 * 用法：
 * @code
 *   BspEncoderExti enc;
 *   enc.init({KEY_PORT, KEY_PIN_ENCODER_SA_PIN, KEY_PIN_ENCODER_SB_PIN,
 *             false, false, false});
 *   // EXTI 回调（中断上下文）里：
 *   enc.on_edge();
 *   // 任务里：
 *   int32_t pos = enc.count();       // 累计位置（4 倍频）
 *   int32_t d   = enc.take_delta();  // 本次增量，取完清零
 * @endcode
 *
 * 极性 / 方向参数（你要“可选上沿还是下沿、高还是低”，就是这三个开关）：
 *   - invert_a / invert_b：该相有效电平取反（硬件反相、接线反了时用）
 *   - invert_dir        ：计数方向整体翻转（顺时针变逆时针）
 *
 * @note on_edge() 在中断上下文执行，只做"读电平 + 查表 + 累加"，不阻塞。
 */

#ifndef __M0BSP_BSP_ENCODER_EXTI_HPP__
#define __M0BSP_BSP_ENCODER_EXTI_HPP__

#include <stdint.h>

#include "ti_msp_dl_config.h" // IWYU pragma: keep

class BspEncoderExti
{
public:
  /** @brief 绑定配置 */
  struct Config
  {
    GPIO_Regs *port;      ///< A/B 所在端口（如 KEY_PORT / GPIOA）
    uint32_t   pin_a;     ///< A 相引脚（DL_GPIO_PIN_x）
    uint32_t   pin_b;     ///< B 相引脚（DL_GPIO_PIN_x）
    bool       invert_a;  ///< A 相电平取反
    bool       invert_b;  ///< B 相电平取反
    bool       invert_dir;///< 计数方向翻转
  };

  /**
   * @brief 绑定引脚并读入初始状态
   * @return true=成功，false=参数无效（端口/引脚为空）
   */
  bool init(const Config &cfg);

  /** @brief 处理一次边沿（在 EXTI 回调里调用） */
  void on_edge();

  /** @brief 累计计数（4 倍频，有符号） */
  int32_t count() const { return _count; }

  /** @brief 读取并清零增量（任务里用，临界区保护） */
  int32_t take_delta();

  /** @brief 清零累计值与增量 */
  void reset(int32_t value = 0);

  /** @brief 当前绑定的配置（调试用） */
  const Config &config() const { return _cfg; }

private:
  /** @brief 读 (A<<1)|B 两个相位位 */
  uint8_t read_state() const;

  Config           _cfg{};
  uint8_t          _state = 0U;     ///< 上一次的 (A,B)
  volatile int32_t _count = 0;      ///< 累计计数
  volatile int32_t _delta = 0;      ///< 待取增量
  bool             _ready = false;
};

#endif /* __M0BSP_BSP_ENCODER_EXTI_HPP__ */
