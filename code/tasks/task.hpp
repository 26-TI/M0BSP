/**
 * @file task.hpp
 * @brief M0BSP 系统任务统一入口声明
 *
 * 只放"系统级"任务（与具体车型 / 业务无关）。业务任务请写在宿主的 User/App/task/ 下。
 *
 * 使用方式：
 * @code
 *   #include "tasks/task.hpp"
 *   xTaskCreate(cpu_monitor_task, "cpu_mon", 0x100, NULL, 1, NULL);
 * @endcode
 *
 * 前置要求（宿主 FreeRTOSConfig.h）：
 *   configUSE_TICK_HOOK = 1
 *   configGENERATE_RUN_TIME_STATS = 1
 *   configUSE_TRACE_FACILITY = 1
 *   configUSE_STATS_FORMATTING_FUNCTIONS = 1
 * 并由宿主提供 CPU_RunTime 定义与 vApplicationTickHook（tick 计数）。
 */

#ifndef __M0BSP_TASK_HPP__
#define __M0BSP_TASK_HPP__

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief CPU 空闲率监控任务（周期 1 s，优先级建议 1）
   *
   * 结果写入全局 g_idle_pct，供调试器 / OLED 查看。
   *
   * @param arg FreeRTOS 任务参数（未使用）
   */
  void cpu_monitor_task(void *arg);

  /** @brief CPU 空闲率百分比（0~100），由 cpu_monitor_task 更新 */
  extern unsigned long g_idle_pct;

#ifdef __cplusplus
}
#endif

#endif /* __M0BSP_TASK_HPP__ */
