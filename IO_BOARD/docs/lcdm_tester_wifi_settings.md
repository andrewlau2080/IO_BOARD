# LCDM 测试机 WiFi 与机身设置

本文件定义高档 LCDM 测试机的本机维护设置。目标是让每一台测试机可独立
设置机身编号、线体/工位和 WiFi，但不会让操作员改写测试机的硬件唯一身份。

## 身份和可编辑字段

| 字段 | LCDM 是否可改 | 来源/用途 |
|---|---:|---|
| `device_uid` | 否 | AT32F455 内部 UID 三个字；LCDM 只读显示，作为真正唯一身份 |
| `wifi_mac` | 否 | ESP-AT `AT+CIPSTAMAC?` 读取后自动保存；长按 K3 进入设置页时直接显示，重新测试模块后更新 |
| `machine_id` | 是 | 机身资产/标签编号，例如 `IOB-01` |
| `line_id` | 是 | 产线编号，例如 `L01` |
| `station_id` | 是 | 工位编号，例如 `ST03`；当前紧凑打印帧会转换为数值 `3` |
| `wifi_ssid` | 是 | 本机要连接的 AP 名称 |
| `wifi_password` | 是 | 本机 AP 密码；生产设置页和编辑键盘直接显示明文，便于现场核对 |
| `print_host` | 是 | 本线打印主机的 IPv4/DNS 地址，例如 `192.168.1.20` |
| `print_port` | 是 | 打印主机实现本机打印协议的 TCP 端口 |

替换整台测试机时，新的 AT32 有新的 `device_uid`；应由打印主机/MAS 将新 UID
绑定到原 `line_id + station_id`，不能通过 LCDM 伪造或覆盖 UID。
`machine_id` 的全厂唯一性不能由一台离线 MCU 自行证明，因此打印主机/MAS 在绑定或
上报时仍应拒绝重复资产编号；本机只负责格式校验和保存。

## Flash 隔离与回滚

内部 Flash 的末尾三个 2 KiB 扇区固定如下：

| 地址 | 内容 |
|---:|---|
| `0x0807E800` | 设备/WiFi 配置副本 A（`+0x100` 为独立 MAC 记录） |
| `0x0807F000` | 设备/WiFi 配置副本 B（`+0x100` 为独立 MAC 记录） |
| `0x0807F800` | 已学习线束矩阵（原有 recipe） |

配置记录带 magic、版本、递增序号、AT32 UID、CRC32 和最后写入的 commit 标记。
保存时先擦除/写入未使用副本并读回校验，旧副本始终保留；掉电或写入失败时启动
会回退到上一份有效配置。链接脚本已把这三个扇区从应用 Flash 排除，不能把 WiFi
配置混入学习资料扇区。

## 设置入口和退出

普通测试页面中，K1-K4 以外的触摸仍被固件忽略。

1. 测试机处于 `READY` 或 K3 已复位后的安全待机状态时，按住 **K3 约 3 秒**。
2. 达到约 3 秒即进入 `WIFI SETUP` 页面（无需等待松手）；此时才执行 `tsw 255,1`，解锁
   其余 LCDM 触控区域。
3. K3 短按为 `CANCEL/RESET`：取消未保存的输入、回到普通测试页，并按现有硬件
   条件重新尝试已保存的 AP。PCB 没有 AT32 -> ESP `EN` 线，因此 K3 是软件重试入口，
   不是 ESP 硬复位。
4. K4 为 `SAVE`；SSID、密码、打印主机或端口有改动时必须先完成网络测试才允许保存。
5. 离开设置页时固件会再次 `tsw 255,0` 并重绘普通 K1-K4 页面，其他区域重新屏蔽。

K1 的 3 秒长按仍只用于学习线束；K2 仍用于自动测试。

## LCDM 页面布局

当前固件以 480 x 272 动态绘制以下路由器式维护页。UID 和 MAC 为只读身份行，
其余资料按左右双列排列：

设置页沿用普通测试页的 32 px 顶栏和 214..271 px K1-K4 底栏；资料行采用 28 px
高度并在每行底部保留可见分隔线，避免字段文字与下横线相撞。

```text
TESTER SETTINGS / WIFI
UID            001122334455... | MAC          AA:BB:CC:DD:EE:FF
MACHINE NO     IOB-01          | PD LINE      L01
STATION        WT-01           | WIFI LINK    OK 192.168.1.42
WIFI SSID      FACTORY-IOBOARD | PRINT HOST   192.168.1.20
WIFI PWD       example-pass    | PRINT PORT   5100
status: SELECT FIELD, ENTER TEXT, TEST, THEN SAVE

 K1 SELF/LEARN | K2 WIFI TEST | K3 CANCEL/RESET | K4 OK/SAVE
```

点按字段会选择输入目标并高亮。固件会直接绘制英文/数字/常用符号软键盘，不依赖修改
现有 `.tft` 文件。编辑页支持
小写/大写、`ABC/123`、空格、退格、清空、取消和确认；密码始终显示明文。输入时只
刷新编辑栏的值区域，键盘、标题和未变化的字段不重绘。若 TJC
工程另有中文输入法或自定义键盘，也可在确认后将完整字段以 ASCII 命令发给 MCU。
密码虽然在生产设置页明文显示，但不会由普通运行状态或调试日志主动回显。

建议的组件 ID（按下事件）如下；也可完全使用坐标触摸，二者由固件同时兼容：

| Component ID | 动作 |
|---:|---|
| 21 | 选择 `MACHINE NO` |
| 22 | 选择 `PD LINE` |
| 23 | 选择 `STATION` |
| 24 | 选择 `WIFI SSID` |
| 25 | 选择 `WIFI PASSWORD` |
| 26 | 选择 `PRINT HOST` |
| 27 | 选择 `PRINT PORT` |
| 31 | 网络测试 |
| 32 | 保存 |
| 33 | 取消/复位 |

## LCDM 到 MCU 输入协议

每条文本命令以 TJC 的 `FF FF FF` 结束。固件内置软键盘只在用户点 `OK` 时一次性
更新 RAM 字段，随后仍须通过 K4/`cfg.save` 写 Flash；外部 TJC 键盘也应在用户点
“确认”时发送完整字段，而不是逐个字符写 Flash。

```text
cfg.machine=IOB-01
cfg.line=L01
cfg.station=ST03
cfg.ssid=FACTORY-IOBOARD
cfg.password=example-password
cfg.host=192.168.1.20
cfg.port=5100
cfg.field=ssid
cfg.value=FACTORY-IOBOARD
cfg.test
cfg.save
cfg.cancel
cfg.refresh
```

也兼容不带 `cfg.` 前缀的 `machine=`、`line=`、`station=`、`ssid=`、`password=`、
`host=`、`port=`。
字段字符串不得包含 CR/LF 或其它控制字符；SSID/密码长度受 ESP-AT 限制（SSID 最多
32 字节、WPA2 密码通常 8-63 字节）。非英文 SSID 可保存和透传，但 LCDM 与 ESP-AT
必须使用同一字符编码，量产前要以现场 AP 实测确认。

## WiFi 测试与保存逻辑

按 `K2 NETWORK TEST` 或发送 `cfg.test` 后，固件使用已确认的 `PC3/PB9` 115200 软件 UART
先清理旧 TCP 会话，再依次执行：

```text
AT+CIPCLOSE
AT
AT+CWMODE=1
AT+CWJAP="<SSID>","<password>"
AT+CIFSR
AT+CIPSTAMAC?
AT+CIPSTART="TCP","<PRINT HOST>",<PRINT PORT>   # 仅填写 host/port 时
```

`AT+CIFSR` 在 DHCP 尚未完成时可能先返回空地址或 `0.0.0.0`；固件会在有限等待窗口
内自动重复查询，不把这一次中间结果直接判为 `NO STA IP`。设置页打开后由前台测试独占
PC3/PB9，测试结束也不会立即启动后台重连，因此 K2 连续重试不会与生产 AT 命令串行冲突。

每次成功取得 `STA MAC` 后，固件立即把 MAC 写入 WiFi 配置区的独立记录，不要求按 K4；
因此下次长按 K3 进入页面时，即使尚未重新测试，也会先显示上次已读到的 MAC。若更换
ESP 模块，重新执行 K2 测试后该记录会更新。若未填写主机地址/端口，测试在取得 `STAIP` 后显示 WiFi 成功；若填写了两项，只有
`CIPSTART` 也成功才显示 `NETWORK PASS PRINT HOST OK - K4 SAVE`。主机和端口必须成对
填写；主机仅接受正常 IPv4/DNS 名，端口为 `1..65535`。密码中的 `"` 和 `\` 在发送
ESP-AT 前会转义。超时、错误密码、AP 不可达、打印主机不可达、ESP-AT 空 Flash 都显示
失败，旧 Flash 配置不改写；操作者可改字段后再次测试，或 K3 返回/重试。K4 可以保存
新的 SSID/密码，即使当前 AP 暂时不可达；保存后的配置会在退出设置页后由后台状态机继续
重连。

保存后的 AT32 配置才是本机唯一来源。启动时，LCDM 测试机使用保存的 SSID/密码和
打印主机地址/端口做后台 ESP-AT TCP 会话；连接进行时不会把打印 JSON 混入 AT 命令通道。
断线后以相同的保存配置重连。此方式不依赖未核实的 ESP `SYSSTORE` 持久化行为。

## 首次上机检查

1. 使用 LCDM 构建烧录后，在普通待机页确认点击其它区域没有动作。
2. 长按 K3 3 秒并松开，确认只在此时出现设置页。
3. 核对 UID 为只读，编辑机身号、线体号、工位号。
4. 输入 SSID/密码以及本线打印主机地址/端口，K2 测试，确认屏幕显示 `NETWORK PASS`。
5. K4 保存，K3 返回普通测试页；断电再上电，重新进入设置页核对字段仍在。
6. 让线束 PASS 后触发 Hall，确认主机收到 TCP `print_request` 并回 ACK/DONE；再确认
   LCDM `COMPLETE` 后必须拆线才允许下一件。
7. 输入错误密码或错误打印主机测试失败，断电再上电，确认上一份已保存资料仍在。
8. 确认学习线束后，分别复核学习资料和 WiFi 设置没有互相覆盖。
