# ==============================================================================
# M0BSP —— MSPM0G351x 板级支持库（CMake OBJECT 库）
#
# 宿主工程：
#   add_subdirectory(Middleware/M0BSP)
#   target_link_libraries(${CMAKE_PROJECT_NAME}.elf m0bsp)
#
# 严格依赖宿主提供（见 README）：
#   User/Board/hardware_config.h      —— 唯一配置源（功能开关 / 任务参数）
#   User/Board/board_instances.hpp    —— 实例清单（中断入口按它展开）
#   User/Board/board_cfg.{hpp,cpp}    —— 驱动对象与 SysConfig 实例的绑定
# ==============================================================================
cmake_minimum_required(VERSION 3.22)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    message(FATAL_ERROR
        "M0BSP 是库，不能单独配置。请在宿主工程里 add_subdirectory() 接入本库。")
endif()

enable_language(C CXX ASM)

# OBJECT 库而不是 STATIC：库内有强符号中断入口（UARTx_IRQHandler / CANFDx_IRQHandler），
# 静态库成员不会因为"要覆盖启动文件的弱符号"被链接器拉进来，会静默失效。
add_library(m0bsp OBJECT)

file(GLOB_RECURSE M0BSP_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/code/*.cpp
)
target_sources(m0bsp PRIVATE ${M0BSP_SOURCES})

# 包含路径只有 code/：库内一律写 "bsp/bsp_uart.hpp"、"device/motor.hpp"，
# 分层关系从 include 语句就能看出来（tools/check_layers.py 也按这个检查）。
target_include_directories(m0bsp PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/code
)

# 宿主配置头（board_instances.hpp / hardware_config.h）只给本库自己看
target_include_directories(m0bsp PRIVATE
    ${CMAKE_SOURCE_DIR}/User/Board
)

target_compile_features(m0bsp PUBLIC cxx_std_20)
