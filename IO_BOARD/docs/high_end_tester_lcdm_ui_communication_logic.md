# 高档测试机 LCDM 通讯与动作逻辑

本文件定义 MCU 如何把以下两份规格落成显示、触摸和动作：

- `high_end_tester_lcdm_ui_elements.md`：界面元素和状态内容。
- `high_end_tester_lcdm_ui_coordinates.md`：绘图坐标和触摸命中区。

原则：LCDM 只是显示和触摸终端；业务动作由 MCU 状态机决定。不能把旧 HMI 控件作为功能入口。

## LCDM 串口基础

| 项目 | 值 |
|---|---|
| MCU TX | `PA9 / USART1_TX` |
| MCU RX | `PA10 / USART1_RX` |
| 默认波特率 | `230400` |
| 数据格式 | `8N1` |
| 命令结束符 | `0xFF 0xFF 0xFF` |
| 显示方式 | MCU 发送 TJC 绘图指令 |
| 触摸方式 | LCDM 回传触摸包，MCU 解析成 K1-K4 |

## 上电初始化顺序

| 顺序 | MCU 操作 | 目的 |
|---:|---|---|
| 1 | 初始化 USART1：`230400 8N1` | 建立 LCDM 通讯通道 |
| 2 | 发送 `bkcmd=0` | 关闭普通命令返回，减少干扰包 |
| 3 | 发送 `dim=100` | 背光全亮 |
| 4 | 发送 `sendxy=1` | 开启坐标触摸回包 |
| 5 | 绘制 `screen_bg` | 先覆盖全屏，避免露出旧内容 |
| 6 | 按坐标表绘制所有元素 | 显示高档测试机主界面 |
| 7 | 再发送 `sendxy=1` | 确认绘制后仍开启坐标触摸 |

上电过程不应反复切换页面、不应依赖未知控件名、不应发送会露出旧控件的 `vis/ref/page` 组合。

## 显示指令

| 操作 | TJC 指令格式 | 使用位置 |
|---|---|---|
| 清屏 | `cls color` | 必要时全屏恢复 |
| 填充矩形 | `fill x,y,w,h,color` | 背景、区域、按钮 |
| 绘制文字 | `xstr x,y,w,h,font,color,bg,align,1,1,"text"` | 标题、状态、按钮文字 |
| 开坐标回包 | `sendxy=1` | 初始化和全屏重绘后 |
| 背光 | `dim=100` | 初始化 |
| 关闭返回码 | `bkcmd=0` | 初始化 |

每条指令必须追加结束字节：

```text
FF FF FF
```

## 全屏绘制顺序

| 顺序 | 元素 ID | 指令类型 |
|---:|---|---|
| 1 | `screen_bg` | `fill 0,0,480,272,WHITE` 或 `cls WHITE` |
| 2 | `title_bar` | `fill` |
| 3 | `title_text` | `xstr` |
| 4 | `role_text` | `xstr` |
| 5 | `status_panel` | `fill` |
| 6 | `state_text` | `xstr` |
| 7 | `main_text` | `xstr` |
| 8 | `key_k1` | `fill` + 两条 `xstr` |
| 9 | `key_k2` | `fill` + 两条 `xstr` |
| 10 | `key_k3` | `fill` + 两条 `xstr` |
| 11 | `key_k4` | `fill` + 两条 `xstr` |
| 12 | `ledm_panel` / `ledm_text` | `fill` + `xstr` |
| 13 | `detail_panel` / `detail_text` | `fill` + `xstr` |
| 14 | `sub_panel` / `sub_text` | `fill` + `xstr` |
| 15 | - | `sendxy=1` |

## 局部刷新规则

| 变化 | 刷新对象 | 备注 |
|---|---|---|
| 状态变化 | `state_text`、`main_text`、`sub_text` | 按状态表更新 |
| LEDM 内容变化 | `ledm_text` | 先清 `ledm_panel` 再写文字 |
| 点位/结果变化 | `detail_text` | 先清 `detail_panel` 再写文字 |
| 按键按下 | 对应 `key_kx` | 背景改 `BLUE` |
| 按键松开 | 对应 `key_kx` | 背景改 `DARK_GRAY` |
| 屏幕露出旧控件 | 全屏绘制 | 属于错误恢复，不改变业务状态 |

## 触摸回包

| 回包类型 | 格式 | MCU 处理优先级 |
|---|---|---:|
| 坐标触摸 | `0x67 xH xL yH yL event FF FF FF` | 1 |
| 组件触摸 | `0x65 page component event FF FF FF` | 2，仅兼容调试 |
| ASCII 命令 | `K1 FF FF FF`、`key=K1 FF FF FF` 等 | 3，仅兼容调试 |

正式高档测试机界面以坐标触摸为主。组件 ID 和 ASCII 只用于过渡验证，不能成为唯一功能路径。

## 坐标触摸解析

| 顺序 | 处理 |
|---:|---|
| 1 | 收到 `0x67` 包 |
| 2 | 解析 `raw_x = xH << 8 | xL` |
| 3 | 解析 `raw_y = yH << 8 | yL` |
| 4 | 保存原始坐标到调试变量 |
| 5 | 按坐标表先尝试 `(raw_x, raw_y)` |
| 6 | 如果不能命中，再按坐标方向兼容表尝试 `(raw_y, raw_x)` |
| 7 | 命中 K1-K4 后更新按键状态并执行业务动作 |
| 8 | 未命中时只显示调试信息，不执行业务动作，不触发旧控件 |

K1-K4 命中表固定如下：

| 按键 | X 范围 | Y 范围 | 固件内部键值 |
|---|---|---|---|
| K1 | `0..119` | `112..169` | `FIRST_GEN_KEY_SET` |
| K2 | `120..239` | `112..169` | `FIRST_GEN_KEY_CLEAR` |
| K3 | `240..359` | `112..169` | `FIRST_GEN_KEY_PLUS` |
| K4 | `360..479` | `112..169` | `FIRST_GEN_KEY_MINUS` |

## 触摸事件语义

| `event` | 含义 | MCU 处理 |
|---:|---|---|
| `1` | 按下 | 设置当前按键，刷新按下显示，执行短按入口或开始长按计时 |
| `0` | 松开 | 清除当前按键，刷新松开显示，结束长按计时 |
| 其它 | 未定义 | 记录调试信息，不执行业务动作 |

K1 长按学习必须依赖“按下保持时间”，不能把单次坐标包直接当成长按。

## 组件触摸兼容

组件触摸只用于临时屏端按钮调试。正式实现必须可以在没有这些组件的空白绘图页上工作。

| Component ID | 映射按键 |
|---:|---|
| `1` 或 `11` | K1 |
| `2` 或 `12` | K2 |
| `3` 或 `13` | K3 |
| `4` 或 `14` | K4 |

其它 Component ID 必须忽略，只显示调试信息。

## ASCII 触摸兼容

| ASCII | 映射按键 |
|---|---|
| `K1` / `key=K1` / `K1_DOWN` / `key=K1_DOWN` / `SELF` | K1 |
| `K2` / `key=K2` / `K2_DOWN` / `key=K2_DOWN` / `AUTO` | K2 |
| `K3` / `key=K3` / `K3_DOWN` / `key=K3_DOWN` / `RESET` | K3 |
| `K4` / `key=K4` / `K4_DOWN` / `key=K4_DOWN` / `OK` / `SAVE` | K4 |

## K1-K4 动作表

| 按键 | 触发条件 | 动作 | 显示结果 |
|---|---|---|---|
| K1 | 短按 | 进入自检 | `SELF / SELF TEST / Axxbxx` |
| K1 | 按住约 3 秒 | 进入学习 | `LEARN / LEARN MODE / LEArn` |
| K2 | 短按 | 进入自动测试 | `AUTO / AUTO TEST / Axxbxx` |
| K3 | 短按 | 复位到待机 | `READY / AUTO TEST / AUTO` |
| K4 | 学习状态短按 | 确认/保存学习资料 | `PASS` 或保存提示 |
| K4 | 非学习状态短按 | 确认当前结果；调试阶段可切 PASS | `PASS` |

## 固件数据流

```text
LCDM touch packet
  -> USART1 RX
  -> lcdm_tjc_poll_event()
  -> decode 0x67 / 0x65 / ASCII
  -> map to K1-K4 or NONE
  -> update high-end tester state
  -> redraw changed LCDM elements
```

## 原控件屏蔽规则

| 问题 | 处理规则 |
|---|---|
| 点触后露出旧控件 | 固件立即全屏重绘；屏端基础页必须改为空白或隐藏旧控件 |
| 旧控件有自己的弹出动作 | 不允许作为正式界面存在；正式页只能保留空白绘图背景或透明无动作触摸层 |
| MCU 收到未知组件 ID | 不执行任何业务动作 |
| MCU 收到非按键区坐标 | 不执行任何业务动作 |
| 需要调试坐标 | 只写 `touch_debug_text`，不能切页或显示旧控件 |

## 三文件关系

| 文件 | 负责内容 | 固件对应模块 |
|---|---|---|
| `high_end_tester_lcdm_ui_elements.md` | 元素、文字、状态内容 | UI 状态和绘图内容 |
| `high_end_tester_lcdm_ui_coordinates.md` | 坐标、触摸命中区、颜色 | 绘图函数和触摸映射函数 |
| `high_end_tester_lcdm_ui_communication_logic.md` | 串口、回包解析、动作表 | LCDM 驱动和按键状态机 |
