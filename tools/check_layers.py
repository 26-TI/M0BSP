#!/usr/bin/env python3
"""
分层检查：code/ 下每个文件只能包含本层或更低层的库头文件。

    python3 tools/check_layers.py        # 有违规时退出码 1

层次（低 → 高）：algorithm、bsp → service → device → tasks
例外：宿主头（hardware_config.h、board_instances.hpp）与 HAL / FreeRTOS / SysConfig 头不在检查范围内。
      `service/user_system.hpp` 是纯格式化工具（不依赖 RTOS / HAL），各层都可以包含。
"""

import pathlib
import re
import sys

CODE = pathlib.Path(__file__).resolve().parent.parent / "code"

# 每层允许包含的库目录
ALLOWED = {
    "algorithm": {"algorithm"},
    "bsp": {"bsp"},
    "service": {"service", "algorithm"},
    "device": {"device", "service", "bsp", "algorithm"},
    "tasks": {"tasks", "device", "service", "bsp", "algorithm"},
}
SHARED = {"service/user_system.hpp"}
INC = re.compile(r'^\s*#\s*include\s+"([^"]+)"', re.M)


def main() -> int:
    bad = []
    known = set(ALLOWED)
    for path in sorted(CODE.rglob("*")):
        if path.suffix not in {".hpp", ".cpp", ".h"}:
            continue
        layer = path.relative_to(CODE).parts[0]
        if layer not in ALLOWED:
            bad.append(f"{path.relative_to(CODE)}: 未知层目录 {layer}/")
            continue
        for target in INC.findall(path.read_text(encoding="utf-8")):
            top = target.split("/", 1)[0]
            if top not in known or target in SHARED:
                continue  # 宿主 / 第三方头文件（ti_msp_dl_config.h、FreeRTOS.h、fort.h …）或共享工具头
            if top not in ALLOWED[layer]:
                bad.append(f"{path.relative_to(CODE)}: {layer}/ 不能包含 {target}")
            elif not (CODE / target).is_file():
                bad.append(f"{path.relative_to(CODE)}: 找不到 {target}")
    for line in bad:
        print(line)
    print(f"check_layers: {'OK' if not bad else f'{len(bad)} 处违规'}")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
