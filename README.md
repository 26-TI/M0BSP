# M0BSP

MSPM0G351x（Cortex-M0+）板级支持库：**bsp → service → device → tasks** 四层 + `algorithm`，
以 CMake **OBJECT 库** `m0bsp` 的形式被宿主工程 `add_subdirectory()` 接入。

代码来自 `Rh-i/MSPM0_FreeRTOS_BDC_Core` 的 `User/{Bsp,Device,Service,Algorithm,App/task}`，
按层重新组织并去掉了对具体板级实例名的依赖。

## 目录

```
code/
├── bsp/          驱动层：uart / pwm / qei / can / i2c / gpio / key
│                 （只封装硬件，不含任何板上器件语义，也不出现 SysConfig 实例名）
├── service/      服务层：user_system（轻量格式化输出）
├── device/       设备层：iic_oled / iic_track / ins / ti_gyro / motor / buzzer + fort.h 字模
├── tasks/        系统任务：cpu_monitor_task（业务任务请写在宿主的 User/App/task/）
└── algorithm/    算法：pid
tools/check_layers.py   分层检查（CI / 本地都可以跑）
```

## 宿主必须提供（库只包含、不定义）

| 文件 | 作用 |
| --- | --- |
| `User/Board/hardware_config.h` | **唯一配置源**：功能开关、任务优先级/栈深、超时 |
| `User/Board/board_instances.hpp` | **实例清单**：`M0BSP_UART_INSTANCES(X)` / `M0BSP_CAN_INSTANCES(X)`，中断入口按它展开 |
| `User/Board/board_cfg.{hpp,cpp}` | 驱动对象定义 + `board_init()`：把对象绑到 SysConfig 实例 |
| `User/Board/FreeRTOSConfig.h` | RTOS 配置（tick 1 kHz、堆大小、钩子开关） |
| `User/Board/freertos_hooks.cpp` | `vApplicationTickHook` / 栈溢出 / 堆失败钩子 |

## 接入

```cmake
add_subdirectory(Middleware/M0BSP)
target_link_libraries(${CMAKE_PROJECT_NAME}.elf m0bsp)
```

## 设计约定

- **库内不出现任何 SysConfig 实例名**（`UART_3_INST` / `PWM_A0_INST` …）。
  在 SysConfig 里改名时，只需改宿主 `board_instances.hpp` 与 `board_cfg.cpp`。
- 中断入口由 `code/bsp/bsp_instances.hpp` 的 X-macro 按清单展开，
  例：`X(UART_3, 64)` → `void UART3_IRQHandler(void)`。
- 分层规则由 `tools/check_layers.py` 检查：`algorithm/bsp → service → device → tasks`。
- 库不初始化任何"板上不存在的器件"：没有对应 GPIO/外设就不实例化、不创建任务。

## 许可

代码主体来自作者自有工程（`Rh-i/MSPM0_FreeRTOS_BDC_Core`），本仓库暂未声明开源许可，
使用前请与作者确认。`code/device/fort.h` 为 8×16 ASCII 点阵字模数据，来源待确认。
FreeRTOS 本体不在本仓库内（宿主放在 `Middleware/FreeRTOS`），其许可见其 `LICENSE.md`。
