/**
 * @file bsp_gpio_irq.hpp
 * @brief GPIO 边沿中断分发（EXTI）——引脚与回调的注册表
 *
 * 用途：MSPM0 的 GPIOA / GPIOB 共享同一个中断向量（GROUP1），
 *       真正的中断入口由宿主实现一次：
 *
 * @code
 *   extern "C" void GROUP1_IRQHandler(void)
 *   {
 *     BspGpioIrq::dispatch(GPIOA);
 *     BspGpioIrq::dispatch(GPIOB);
 *   }
 * @endcode
 *
 * 引脚的电平极性 / 上下拉 / 是否使能中断由 SysConfig 生成，
 * 本类只负责"哪个引脚 -> 哪个回调"的分发，不碰 IOMUX。
 *
 * @note 回调在中断上下文执行：只能做计数、xQueueSendFromISR 这类短操作，
 *       不能 vTaskDelay，也不能阻塞。
 */

#ifndef __M0BSP_BSP_GPIO_IRQ_HPP__
#define __M0BSP_BSP_GPIO_IRQ_HPP__

#include <stdint.h>

#include "ti_msp_dl_config.h" // IWYU pragma: keep

class BspGpioIrq
{
public:
  /** @brief 边沿回调（中断上下文） */
  using Callback = void (*)(void *ctx);

  /** @brief 注册项 */
  struct Entry
  {
    GPIO_Regs *port; ///< GPIOA / GPIOB
    uint32_t   pin;  ///< DL_GPIO_PIN_x
    Callback   cb;   ///< 回调
    void      *ctx;  ///< 回调上下文
  };

  /**
   * @brief 注册一个引脚的边沿回调
   * @return true=注册成功，false=表满或参数无效
   */
  static bool attach(const Entry &e);

  /** @brief 注销某个引脚（没有注册项时无副作用） */
  static void detach(GPIO_Regs *port, uint32_t pin);

  /**
   * @brief 分发某个端口上已挂起的中断（在 GROUP1_IRQHandler 里调用）
   * @param port GPIOA 或 GPIOB
   */
  static void dispatch(GPIO_Regs *port);

private:
  static constexpr uint8_t MAX_ENTRIES = 12;
  static Entry             _entries[MAX_ENTRIES];
  static uint8_t           _count;
};

#endif /* __M0BSP_BSP_GPIO_IRQ_HPP__ */
