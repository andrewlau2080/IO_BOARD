# LCDM 打印终端触屏界面定义

本界面按之前打印内容 mockup 先固定一版，用于后续插上 LCDM 后直接触屏输入和调试。LCDM 采用与 MOTOR / Steering Engine 项目相同的 TJC 陶晶驰串口 HMI 智能屏方向，屏幕负责按钮、键盘、文本输入；MCU 负责保存字段、生成预览文本、生成 ZPL 并通过打印机通讯口发给斑马打印机。

## 页面

| page | 用途 |
|---:|---|
| 0 | 打印内容编辑、预览、打印 |
| 2 | 设备对码、工位绑定、换机 |

建议 800x480 或接近尺寸 LCDM。若屏幕尺寸不同，控件名称保持一致即可，位置可按屏幕调整。

## LCDM 硬件接口

| 项目 | 规格 |
|---|---|
| LCDM 品牌/协议 | TJC 陶晶驰串口 HMI，兼容 Nextion 风格 ASCII 指令 + `FF FF FF` 结束符 |
| LCDM 通讯 | GPIO 软件 UART：`PB3=MCU_TX` 接 LCDM RX，`PB5=MCU_RX` 接 LCDM TX，`PB4` 预留 LCDM RESET |
| LCDM 串口参数 | 默认 9600 8N1；实机稳定后可评估 38400 |
| 打印机通讯 | `USART1`，`PA9=TX`，`PA10=RX`，默认 9600 8N1 |
| 禁用脚 | `PA2` 已用于 `ADC2_IN2` 扫描输入，不能接 LCDM；`PH2` 不写入 LQFP100 最终规格 |
| 电平 | AT32 侧为 3.3V TTL；LCDM/打印机转换模块若为 5V TTL，必须加电平转换或确认 3.3V 兼容 |

## 控件命名

MCU 会写入以下控件，LCDM 工程中名称要一致：

| 控件名 | 类型 | 显示内容 |
|---|---|---|
| `nStation` | number | 测试机编号，1-10 |
| `tResult` | text | `PASS` 或 `NG` |
| `tTitle` | text/input | 标签抬头 |
| `tItem` | text/input | 款项 / 型号 |
| `tContent` | text/input | 内容 |
| `tCode` | text/input | 英文数字代码 / 条码内容 |
| `nQty` | number | 数量 |
| `nCopies` | number | 打印份数 |
| `tPreview` | text | MCU 返回的 ASCII 预览 |
| `tStatus` | text | `READY` / `PRINT OK` / `PRINT ERROR` |

建议按钮：

| 按钮 | component id | 动作 |
|---|---:|---|
| 预览 | 1 | 发送 `preview` |
| 打印 | 2 | 发送 `print` |
| 默认值/清除 | 3 | 发送默认触摸事件，MCU 恢复默认字段 |

## 设备对码页

`page 2` 用于测试机对码和换机。测试机端无 LCDM，现场人员在测试机上长按 `K1 自检 + K4 确认` 进入对码，打印主机 LCDM 显示正在申请对码的设备。

建议控件：

| 控件名 | 类型 | 显示内容 |
|---|---|---|
| `tPairList` | text/list | 正在申请对码的设备短码、原工位、在线状态 |
| `nPairStation` | number | 准备绑定或替换的工位号，1-10 |
| `tPairCode` | text | 当前选中设备短码 |
| `tPairStatus` | text | `WAIT` / `ASSIGN` / `OK` / `FAIL` |

建议按钮：

| 按钮 | 动作 |
|---|---|
| 刷新 | 发送 `pair.refresh` |
| 绑定 | 发送 `pair.bind=<station>,<code>` |
| 替换 | 发送 `pair.replace=<station>,<code>` |
| 取消 | 发送 `pair.cancel=<code>` |

详细对码流程见 `docs/station_pairing_plan.md`。

## LCDM 发给 MCU 的数据包

每个包以 `FF FF FF` 结束。

| 包内容 | 说明 |
|---|---|
| `station=1` | 测试机编号，1-10 |
| `result=PASS` | 测试结果 |
| `result=NG` | 测试结果 |
| `title=HARNESS TEST` | 抬头 |
| `item=MODEL-A` | 款项 / 型号 |
| `content=LINE 01 / SHIFT A / OPERATOR 001` | 内容 |
| `code=A1B1-000001` | 条码内容 |
| `qty=1` | 数量 |
| `copies=1` | 打印份数 |
| `preview` | 刷新预览 |
| `print` | 提交打印 |
| `refresh` | 要求 MCU 重发所有字段 |

也兼容 TJC 触摸事件 `65 page component event FF FF FF`。当前固件定义 component id：1=预览，2=打印，3=恢复默认。

## 默认内容

| 字段 | 默认值 |
|---|---|
| 测试机编号 | `1` |
| 测试结果 | `PASS` |
| 抬头 | `HARNESS TEST` |
| 款项 / 型号 | `MODEL` |
| 内容 | `CONTENT` |
| 代码 | `CODE000001` |
| 数量 | `1` |
| 份数 | `1` |

## 打印机参数页

建议增加 `page 1` 作为打印机参数页。参数页不一定一开始全部显示，但控件和协议先预留，后面调 ZT230 或其它 Zebra/ZPL 兼容机时不用改 MCU 程序。

MCU 会回写以下控件：

| 控件名 | 类型 | 内容 |
|---|---|---|
| `nBaud` | number | 打印 485 波特率 |
| `nDataBits` | number | 数据位，7/8/9 |
| `nStopBits` | number | 停止位，1/2 |
| `tParity` | text | `NONE` / `EVEN` / `ODD` |
| `nDirEn` | number | 是否启用 DE/RE 方向控制 |
| `nDirHi` | number | DE/RE 发送有效电平，1=高有效 |
| `nLblW` | number | 标签宽度 dot，对应 ZPL `^PW` |
| `nLblL` | number | 标签长度 dot，对应 ZPL `^LL` |
| `nOrgX` | number | 原点 X，对应 ZPL `^LHx,y` |
| `nOrgY` | number | 原点 Y，对应 ZPL `^LHx,y` |
| `nRot` | number | 旋转，0=N、1=R、2=I、3=B |
| `nSpeed` | number | 打印速度 ips，对应 ZPL `^PR` |
| `nDark` | number | 浓度，0-30，对应 ZPL `^MD` |
| `nTitleFont` | number | 标题字体 dot |
| `nBodyFont` | number | 正文字体 dot |
| `nFootFont` | number | 底部字体 dot |
| `nBarW` | number | 条码模块宽度，对应 ZPL `^BY` |
| `nBarRatio` | number | 条码宽窄比，2 或 3 |
| `nBarH` | number | 条码高度 dot |
| `nBarText` | number | 条码下方是否显示文字 |

LCDM 修改参数时发送：

| 包内容 | 说明 |
|---|---|
| `cfg.baud=9600` | 波特率，可用 1200/2400/4800/9600/19200/38400/57600/115200 |
| `cfg.databits=8` | 数据位 |
| `cfg.stop=1` | 停止位 |
| `cfg.parity=NONE` | 无校验 |
| `cfg.parity=EVEN` | 偶校验 |
| `cfg.parity=ODD` | 奇校验 |
| `cfg.dir_en=0` | 不启用方向脚 |
| `cfg.dir_hi=1` | 方向脚高有效 |
| `cfg.width=600` | 标签宽度 dot |
| `cfg.length=360` | 标签长度 dot |
| `cfg.orgx=0` | 标签原点 X |
| `cfg.orgy=0` | 标签原点 Y |
| `cfg.rot=0` | 旋转 |
| `cfg.speed=4` | 打印速度 |
| `cfg.dark=15` | 打印浓度 |
| `cfg.titlefont=34` | 标题字号 |
| `cfg.bodyfont=28` | 正文字号 |
| `cfg.footfont=26` | 底部字号 |
| `cfg.barw=2` | 条码宽度 |
| `cfg.barratio=2` | 条码宽窄比 |
| `cfg.barh=70` | 条码高度 |
| `cfg.bartext=1` | 显示条码文字 |
| `cfg.default=1` | 恢复打印参数默认值 |

## 输入限制

当前 MCU 只保存可打印 ASCII 字符，范围 `0x20` 到 `0x7E`。为避免破坏 TJC 文本指令和 ZPL 指令：

| 字符 | MCU 处理 |
|---|---|
| 非 ASCII 可打印字符 | 转空格 |
| `"` | 转 `'` |
| `\` | 转 `/` |
| `^` / `~` | ZPL 输出时转空格 |

## 调试建议

第一版 LCDM 工程先做一页即可：左侧字段输入，右侧 `tPreview` 预览，下方放 `预览`、`打印`、`默认值` 三个按钮。实机确认输入包和打印输出后，再考虑增加模板保存、历史记录、打印机状态读取。
