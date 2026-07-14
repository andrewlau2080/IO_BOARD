# 高档测试机 LCDM 显示程序规划

本规划用于 `TESTER_LCDM` 高档测试机。第一版 LCDM 工作模式尽量与现有 LEDM/TM1637 一致，不重新定义复杂 UI：原来 LEDM 显示什么状态，LCDM 就显示同等状态，只是用大字和辅助文字表达得更清楚。

## 程序结构

新增 `first_gen_display` 显示兼容层：

| 文件 | 作用 |
|---|---|
| `inc/first_gen_display.h` | 对一代/高档测试流程提供统一显示和按键接口 |
| `src/first_gen_display.c` | 后端选择：LEDM/TM1637 或 TJC LCDM |
| `src/first_gen_4051_scan.c` | 不再直接调用 TM1637；只调用 `first_gen_display_*()` |

默认仍然编译 LEDM 后端，不影响现有普通测试机。

```sh
cmake -S . -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake
cmake --build build
```

高档 LCDM 测试机固件：

```sh
cmake -S . -B build-lcdm-tester -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake \
  -DIO_APP_MODE=FIRST_GEN_4051_LOCAL \
  -DFIRST_GEN_DISPLAY_BACKEND=LCDM
cmake --build build-lcdm-tester
```

## LEDM 到 LCDM 的模式映射

| LEDM 原显示 | LCDM 第一版显示 |
|---|---|
| `WIRE TESTER` 滚动 | `tTitle=WIRE TESTER`，`tMain=WIRE TESTER` |
| `AUTO-T` | `tState=READY`，`tMain=AUTO TEST` |
| `LEArn` | `tState=LEARN`，`tMain=LEARN` |
| `A01b01`... | `tLedm=A01b01`，`tMain=A01b01` |
| `004004` | `tLedm=004004`，用于学习 OUT/IN 数 |
| `tot-08` | `tLedm=tot-08`，用于学习总点数 |
| `SAUEd` | `tState=SAVED`，`tMain=SAVED` |
| `PASS` | `tState=PASS`，`tMain=PASS` |
| `NG` | `tState=NG`，`tMain=NG` |
| `001002` | `tLedm=001002`，表示当前 OUT/IN 或故障点 |
| `Er0001` | `tState=ERROR`，`tMain=Er0001` |
| `Prnt` | `tState=PRINT`，`tMain=PRINT READY` |
| `Printd` | `tState=PRINT`，`tMain=PRINT DONE` |

说明：第一版 LCDM 先按 6 字符 LEDM 兼容显示，后续再把 `001002` 展开成 `OUT001 / IN002`、故障类型、详细点位表。

## TJC LCDM 页面 0 控件

高档测试机 LCDM 第一版只要求 `page 0`。

| 控件名 | 类型 | MCU 写入内容 |
|---|---|---|
| `tTitle` | text | 固定标题，例如 `WIRE TESTER` |
| `tState` | text | `BOOT/READY/TESTING/PASS/NG/LEARN/SAVED/ERROR/PRINT` |
| `tMain` | text | 大字主显示，例如 `PASS`、`NG`、`AUTO TEST` |
| `tLedm` | text | LEDM 兼容 6 字符显示，例如 `A01b01`、`LEArn`、`001002` |
| `tSub` | text | 辅助提示，例如 `K1 SELF/LEARN  K2 AUTO  K3 RESET  K4 OK` |

## LCDM 按键映射

LCDM 必须模拟 LEDM 的 4 个按键，不改变测试流程。

| LEDM 键 | LCDM 功能 | TJC component id | 固件内部键值 |
|---|---|---:|---|
| K1 | 短按自检；长按约 3 秒学习 | 11 | `FIRST_GEN_KEY_SET` |
| K2 | 自动测试 / 进入实时扫描 | 12 | `FIRST_GEN_KEY_CLEAR` |
| K3 | 复位 / 重来 | 13 | `FIRST_GEN_KEY_PLUS` |
| K4 | 学习确认保存 | 14 | `FIRST_GEN_KEY_MINUS` |

TJC 按钮建议打开按下和松开事件。固件收到按下事件时保持对应键值，收到松开事件时恢复无按键，因此 K1 长按学习可以沿用 LEDM 逻辑。

也可以用 ASCII 指令调试：

| LCDM 发给 MCU | 含义 |
|---|---|
| `K1` 或 `key=K1` | K1 一次短按 |
| `K2` 或 `key=K2` | K2 一次短按 |
| `K3` 或 `key=K3` | K3 一次短按 |
| `K4` 或 `key=K4` | K4 一次短按 |
| `key=K1_DOWN` | K1 按下，用于长按学习 |
| `key=K1_UP` | K1 松开 |

## 当前限制

- 第一版 LCDM 后端还只是 LEDM 兼容显示，不做二代机那种完整 128 点网格页。
- `TESTER_LCDM` 和 `PRINT_HOST` 都使用 `PA9/PA10` LCDM 显示接口，角色仍由 Flash 配置决定，不能靠 LCDM 是否存在判定。
- LCDM 页面内容后续可以扩展，但不要改变 K1-K4 的基础语义，否则普通 LEDM 和高档 LCDM 两套操作会分裂。
