# 高档测试机 LCDM 显示元素表

本文件定义高档测试机 LCDM 页面上“有什么”。坐标和触摸范围只放在
`high_end_tester_lcdm_ui_coordinates.md`，串口通讯和动作只放在
`high_end_tester_lcdm_ui_communication_logic.md`。

目标：高档测试机界面由 MCU 通过 LCDM/TJC 绘图指令生成，不依赖屏内旧项目控件，不露出原控件。

## 页面

| 页面 ID | 页面名称 | 用途 | 规则 |
|---:|---|---|---|
| 0 | `tester_main` | 高档测试机主界面 | 第一版只使用这一页；上电后 MCU 绘制全屏界面 |

## 固定显示元素

| 元素 ID | 类型 | 固定内容 | 用途 | 刷新方式 |
|---|---|---|---|---|
| `screen_bg` | 色块 | 全屏背景 | 覆盖屏幕原内容，避免露出旧控件 | 全屏绘制 |
| `title_bar` | 色块 | 顶部深蓝栏 | 标题区背景 | 全屏绘制 |
| `title_text` | 文本 | `WIRE TESTER LCDM` | 设备/界面名称 | 全屏绘制 |
| `role_text` | 文本 | `TESTER` | 当前角色 | 全屏绘制 |
| `status_panel` | 色块 | 浅色状态区 | 承载状态和主提示 | 全屏绘制 |
| `key_band` | 色块 | K1-K4 按键带 | 承载四个触摸按键 | 全屏绘制 |
| `ledm_panel` | 色块 | LEDM 显示区 | 显示 6 字符兼容内容 | 全屏绘制 |
| `detail_panel` | 色块 | 详情区 | 显示点位、数量、故障原因 | 全屏绘制 |
| `sub_panel` | 色块 | 辅助提示区 | 显示操作提示或触摸调试信息 | 全屏绘制 |

## 动态显示元素

| 元素 ID | 类型 | 示例内容 | 数据来源 | 刷新方式 |
|---|---|---|---|---|
| `state_text` | 文本 | `READY` / `SELF` / `AUTO` / `LEARN` / `PASS` / `NG` | 固件状态机 | 状态变化刷新 |
| `main_text` | 文本 | `AUTO TEST` / `SELF TEST` / `PASS` / `NG` | 固件状态机 | 状态变化刷新 |
| `ledm_text` | 文本 | `AUTO` / `A01b01` / `PASS` / `001002` | LEDM 兼容显示模型 | 内容变化刷新 |
| `detail_text` | 文本 | `PROFILE DB50` / `OUT001 IN002` / `OK 092/092` | 测试流程 | 内容变化刷新 |
| `sub_text` | 文本 | `K1 SELF/LEARN  K2 AUTO  K3 RESET  K4 OK` | 当前状态提示 | 内容变化刷新 |
| `touch_debug_text` | 文本 | `T 060,140 -> K1 DOWN` | LCDM 触摸解析 | 调试阶段覆盖 `sub_text` |

## 触摸按键元素

| 元素 ID | 按键 | 上排显示 | 下排显示 | 高档测试机动作 | 固件内部键值 |
|---|---|---|---|---|---|
| `key_k1` | K1 | `K1` | `SELF/LEARN` | 短按自检；长按约 3 秒学习 | `FIRST_GEN_KEY_SET` |
| `key_k2` | K2 | `K2` | `AUTO` | 自动测试 | `FIRST_GEN_KEY_CLEAR` |
| `key_k3` | K3 | `K3` | `RESET` | 复位到待机 | `FIRST_GEN_KEY_PLUS` |
| `key_k4` | K4 | `K4` | `OK/SAVE` | 确认/保存；调试阶段可切 PASS | `FIRST_GEN_KEY_MINUS` |

## 状态显示内容

| 状态 ID | 场景 | `state_text` | `main_text` | `ledm_text` | `detail_text` | `sub_text` |
|---|---|---|---|---|---|---|
| `READY` | 待机 | `READY` | `AUTO TEST` | `AUTO` | `PROFILE DB50` | `K1 SELF/LEARN  K2 AUTO  K3 RESET  K4 OK` |
| `SELF` | 自检 | `SELF` | `SELF TEST` | `Axxbxx` | `OUTxxx INxxx` | `CHECKING...  K3 RESET` |
| `AUTO` | 自动测试 | `AUTO` | `AUTO TEST` | `Axxbxx` | `OK xxx/092` | `RUNNING  K3 RESET` |
| `LEARN` | 学习 | `LEARN` | `LEARN MODE` | `LEArn` | `OUT048 IN048` | `HOLD K1 3S  K4 SAVE` |
| `PASS` | 通过 | `PASS` | `PASS` | `PASS` | `TOTAL 092 OK` | `PRINT READY  REMOVE HARNESS` |
| `NG` | 不良 | `NG` | `NG` | `001002` | `OUT001 IN002` | `SHORT CIRCUIT  K3 RESET` |

## 实现边界

| 项目 | 规则 |
|---|---|
| 页面来源 | 不把 MOTOR 或其它旧 HMI 当作目标页面；只参考其已验证的串口和触摸通道 |
| 显示来源 | 所有可见内容都由本文件的元素表产生 |
| 触摸来源 | 触摸只产生 K1-K4 或无动作，不把事件交给旧页面控件 |
| 非命中区域 | 点到 K1-K4 以外区域只记录调试信息，不执行业务动作 |
| 原控件露出 | 属于错误状态；固件必须全屏重绘，屏端基础页必须为空白或无可见旧控件 |
