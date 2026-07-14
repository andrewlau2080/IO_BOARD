# 高档测试机 LCDM 当前确认版本记录

日期：2026-07-14

## 结论

当前固件版本已确认：

| 项目 | 状态 |
|---|---|
| LCDM 显示 | 正常 |
| LCDM 触摸 | 正常 |
| K1-K4 坐标映射 | 正常 |
| 按键动作链路 | 正常 |
| 原控件弹出问题 | 当前版本不再作为功能路径 |
| BEEP 判断 | 不作为触摸依据 |

## 固件规则

LCDM 按规格书通讯执行，不再使用 BEEP 声音作为输入判断。

| 项目 | 当前值 |
|---|---|
| 串口 | `USART1` |
| MCU TX | `PA9` |
| MCU RX | `PA10` |
| 波特率 | `230400` |
| 数据格式 | `8N1` |
| 命令结束符 | `FF FF FF` |
| 主要触摸回包 | `0x67 xH xL yH yL event FF FF FF` |

## 触摸坐标

当前正式按键区：

| 按键 | X 范围 | Y 范围 | 固件键值 |
|---|---|---|---|
| K1 | `0..119` | `112..169` | `FIRST_GEN_KEY_SET` |
| K2 | `120..239` | `112..169` | `FIRST_GEN_KEY_CLEAR` |
| K3 | `240..359` | `112..169` | `FIRST_GEN_KEY_PLUS` |
| K4 | `360..479` | `112..169` | `FIRST_GEN_KEY_MINUS` |

## 本次保留的实现重点

- LCDM 接收只按 `FF FF FF` 作为合法包结束。
- `lcdm_tjc.c` 使用 USART1 中断收包，并用 8 包 FIFO 防止连续触摸包被覆盖。
- 解析并记录 `0x67` 坐标触摸、`0x65` 组件触摸、`0x71` 数值返回。
- 正式 LCDM 后端启动时强制同步到 `230400`，再绘制界面并开启 `sendxy=1`。
- `first_gen_display.c` 负责把 LCDM 坐标包映射成 K1-K4，再交给 first-gen 测试状态机执行。
- 已加入 `g_first_gen_lcdm_*` 和 `g_lcdm_tjc_*` 调试变量，后续可直接读内存确认输入链路。

## 对应源码

| 功能 | 文件 |
|---|---|
| LCDM/TJC 串口与回包解析 | `IO_BOARD/src/lcdm_tjc.c`, `IO_BOARD/inc/lcdm_tjc.h` |
| 高档测试机 LCDM 显示和触摸映射 | `IO_BOARD/src/first_gen_display.c`, `IO_BOARD/inc/first_gen_display.h` |
| First-gen 测试业务状态机 | `IO_BOARD/src/first_gen_4051_scan.c` |
| 独立 LCDM 测试界面 | `IO_BOARD/src/lcdm_motor_ui.c` |

## 已验证构建

```sh
cd IO_BOARD
cmake --build build-lcdm-tester
```

已下载到板子并由现场确认：显示、触摸及对应动作均正常。
