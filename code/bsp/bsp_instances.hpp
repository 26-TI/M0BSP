/**
 * @file bsp_instances.hpp
 * @brief 中断入口的 X-macro 展开（板级实例清单由宿主提供）
 *
 * 宿主在 User/Board/board_instances.hpp 里给出两份清单：
 *
 *   #define M0BSP_UART_INSTANCES(X)  X(UART_0, 64) X(UART_1, 64) ...
 *   #define M0BSP_CAN_INSTANCES(X)   X(MCAN0)
 *
 * 每一项的 pfx 是 SysConfig 的**实例前缀**（不带 _INST），例如 UART_3。
 * SysConfig 会为每个实例生成 `UART_3_INST_IRQHandler` / `UART_3_INST` 这类宏，
 * 本文件把它们拼成真正的 C 中断入口（UART3_IRQHandler）——库内因此不出现任何
 * 具体实例名，宿主改名只需改 board_instances.hpp。
 */

#ifndef __M0BSP_BSP_INSTANCES_HPP__
#define __M0BSP_BSP_INSTANCES_HPP__

#include "FreeRTOS.h" // IWYU pragma: keep
#include "task.h"

#include "ti_msp_dl_config.h" // IWYU pragma: keep

#include "board_instances.hpp" // 宿主提供：实例清单

#include "bsp/bsp_can.hpp"
#include "bsp/bsp_uart.hpp"

/** @brief 两级拼接（一级拼接的结果不会展开，必须两级） */
#define M0BSP_CAT_(a, b) a##b
#define M0BSP_CAT(a, b) M0BSP_CAT_(a, b)

/**
 * @brief 展开出一个 UART 中断入口
 * @param pfx     SysConfig 实例前缀（如 UART_3）
 * @param bufsize TX DMA 缓冲区大小，必须与宿主对象的模板实参一致
 */
#define M0BSP_UART_ISR(pfx, bufsize)                            \
  extern "C" void M0BSP_CAT(pfx, _INST_IRQHandler)(void)        \
  {                                                             \
    BaseType_t woken = pdFALSE;                                 \
    if (auto *inst = BspUart<bufsize>::find(M0BSP_CAT(pfx, _INST))) \
    {                                                           \
      inst->handle_isr(&woken);                                 \
    }                                                           \
    portYIELD_FROM_ISR(woken);                                  \
  }

/**
 * @brief 展开出一个 CAN 中断入口
 * @param pfx SysConfig 实例前缀（如 MCAN0）
 */
#define M0BSP_CAN_ISR(pfx)                                      \
  extern "C" void M0BSP_CAT(pfx, _INST_IRQHandler)(void)        \
  {                                                             \
    if (auto *inst = BspCan::instance())                        \
    {                                                           \
      inst->isr_handler();                                      \
    }                                                           \
  }

#endif /* __M0BSP_BSP_INSTANCES_HPP__ */
