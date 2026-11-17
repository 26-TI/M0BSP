#!/usr/bin/env python3
"""
跨平台 SysConfig 启动器 —— 打开 TI SysConfig 界面并在关闭后重新生成代码。

本脚本随 M0BSP 库一起分发，可被任意宿主工程复用；它会自动找到宿主工程根
（含 CMakeLists.txt 且 SysConfig/ 下有 *.syscfg 的那个目录）。

用法（在宿主工程根目录下执行）：
    python3 Middlewares/M0BSP/tools/sysconfig_ui.py            # 开界面，关闭后自动 CLI 生成
    python3 Middlewares/M0BSP/tools/sysconfig_ui.py --gen      # 只跑 CLI 生成（不开界面）
    python3 Middlewares/M0BSP/tools/sysconfig_ui.py --dry-run  # 只打印将要执行的命令

工程根定位顺序：
    --project-root <dir>  →  环境变量 M0BSP_HOST_ROOT  →  当前目录向上查找  →  脚本所在目录向上查找

工具定位顺序（三级回退）：
    --sysconfig <dir>  →  环境变量 TI_SYSCONFIG_DIR  →  平台默认
        Linux  : ~/ti/sysconfig_1.27.0
        Windows: D:\\CCS\\sysconfig
    --sdk <dir>        →  环境变量 MSPM0_SDK_ROOT   →  平台默认
        Linux  : ~/ti/mspm0_sdk_2_10_00_04
        Windows: C:/ti/mspm0_sdk_2_10_00_04
"""

from __future__ import annotations

import argparse
import os
import subprocess
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent

LINUX_DEFAULTS = {
    "sysconfig": "~/ti/sysconfig_1.27.0",
    "sdk": "~/ti/mspm0_sdk_2_10_00_04",
}
WINDOWS_DEFAULTS = {
    "sysconfig": r"D:\CCS\sysconfig",
    "sdk": r"C:\ti\mspm0_sdk_2_10_00_04",
}


def looks_like_host(d: Path) -> bool:
    return (d / "CMakeLists.txt").is_file() and (d / "SysConfig").is_dir()


def find_project_root(explicit: str | None) -> Path:
    if explicit:
        return Path(explicit).expanduser().resolve()

    env = os.environ.get("M0BSP_HOST_ROOT")
    if env:
        return Path(env).expanduser().resolve()

    for start in (Path.cwd(), SCRIPT_DIR):
        for cand in (start, *start.parents):
            if looks_like_host(cand):
                return cand

    sys.exit(
        "❌ 找不到宿主工程根（需要同时存在 CMakeLists.txt 与 SysConfig/）\n"
        "   请在宿主工程根目录下执行，或用 --project-root / M0BSP_HOST_ROOT 指定。"
    )


def default_for(kind: str) -> Path:
    table = WINDOWS_DEFAULTS if os.name == "nt" else LINUX_DEFAULTS
    return Path(table[kind]).expanduser()


def pick(arg_value: str | None, env_name: str, kind: str) -> Path:
    if arg_value:
        return Path(arg_value).expanduser()
    env = os.environ.get(env_name)
    if env:
        return Path(env).expanduser()
    return default_for(kind)


def first_existing(candidates: list[Path]) -> Path | None:
    for c in candidates:
        if c.exists():
            return c
    return None


def find_syscfg(syscfg_dir: Path) -> Path:
    files = sorted(syscfg_dir.glob("*.syscfg"))
    if not files:
        sys.exit(f"❌ {syscfg_dir} 下没有 .syscfg 文件")
    if len(files) > 1:
        print(f"[提示] 找到多个 .syscfg，使用第一个：{files[0].name}")
    return files[0]


def build_common_args(product: Path, output: Path, syscfg: Path) -> list[str]:
    return ["--product", str(product), "--output", str(output), str(syscfg)]


def run(cmd: list[str], dry_run: bool) -> int:
    print("$ " + " ".join(str(c) for c in cmd))
    if dry_run:
        return 0
    return subprocess.call([str(c) for c in cmd])


def gui_command(ti_sysconfig: Path) -> list[str] | None:
    if os.name == "nt":
        exe = first_existing([ti_sysconfig / "sysconfig_gui.bat", ti_sysconfig / "sysconfig_gui.exe"])
    else:
        exe = first_existing([ti_sysconfig / "sysconfig_gui.sh", ti_sysconfig / "sysconfig_gui"])
    if exe is None:
        return None
    if exe.suffix == ".sh":
        return ["bash", str(exe)]
    return [str(exe)]


def cli_command(ti_sysconfig: Path) -> list[str] | None:
    if os.name == "nt":
        node = first_existing([ti_sysconfig / "nodejs" / "node.exe"])
        cli = first_existing([ti_sysconfig / "dist" / "cli.js"])
        if node is None or cli is None:
            bat = first_existing([ti_sysconfig / "sysconfig_cli.bat"])
            return [str(bat)] if bat else None
        return [str(node), str(cli)]
    sh = first_existing([ti_sysconfig / "sysconfig_cli.sh"])
    if sh is None:
        return None
    return ["bash", str(sh)]


def main() -> int:
    ap = argparse.ArgumentParser(description="打开 TI SysConfig 界面并生成代码")
    ap.add_argument("--project-root", help="宿主工程根目录")
    ap.add_argument("--sysconfig", help="SysConfig 安装目录")
    ap.add_argument("--sdk", help="MSPM0 SDK 根目录")
    ap.add_argument("--gen", action="store_true", help="只跑 CLI 生成，不开界面")
    ap.add_argument("--dry-run", action="store_true", help="只打印命令")
    args = ap.parse_args()

    project_root = find_project_root(args.project_root)
    syscfg_dir = project_root / "SysConfig"
    ti_sysconfig = pick(args.sysconfig, "TI_SYSCONFIG_DIR", "sysconfig")
    sdk = pick(args.sdk, "MSPM0_SDK_ROOT", "sdk")
    product = sdk / ".metadata" / "product.json"

    if not product.is_file():
        sys.exit(
            f"❌ 找不到 product.json：{product}\n"
            "   请用 --sdk <SDK 根目录> 或环境变量 MSPM0_SDK_ROOT 指定 MSPM0 SDK。"
        )

    syscfg = find_syscfg(syscfg_dir)
    common = build_common_args(product, syscfg_dir, syscfg)

    print(f"工程根：{project_root}")

    if not args.gen:
        gui = gui_command(ti_sysconfig)
        if gui is None:
            sys.exit(
                f"❌ 在 {ti_sysconfig} 找不到 SysConfig 界面程序\n"
                "   Linux  : sysconfig_gui.sh\n"
                "   Windows: sysconfig_gui.bat\n"
                "   请用 --sysconfig <SysConfig 安装目录> 或环境变量 TI_SYSCONFIG_DIR 指定。"
            )
        rc = run(gui + common, args.dry_run)
        if rc != 0:
            print(f"[警告] 界面返回码 {rc}（关闭窗口通常返回 0，若刚保存过可忽略）")
        print("[1/2] 界面已关闭，重新生成代码…")
    else:
        print("[1/2] 跳过界面（--gen）")

    cli = cli_command(ti_sysconfig)
    if cli is None:
        sys.exit(f"❌ 在 {ti_sysconfig} 找不到 SysConfig CLI（sysconfig_cli.sh / dist/cli.js）")

    rc = run(cli + common, args.dry_run)
    print("[2/2] 生成完成" if rc == 0 else f"[2/2] 生成失败，返回码 {rc}")
    return rc


if __name__ == "__main__":
    sys.exit(main())
