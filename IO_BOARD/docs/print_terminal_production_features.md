# 打印控制端量产功能

本文件对应 `IO_APP_MODE=PRINT_TERMINAL`。它与
`FIRST_GEN_4051_LOCAL` 测试机程序分开构建；测试机扫描、学习、WiFi 客户端和
学习线束 Flash 区不由本模块修改。

## LCDM 操作

打印控制器复用现有 `260728song.tft` 的字体、颜色、标题栏和 K1--K4 区域，以直接
绘制方式工作，不依赖旧版 `tTitle`、`tStatus` 等不存在的 HMI 控件。

正常打印页：

| 按键 | 动作 |
|---|---|
| K1 | 预览当前模板 |
| K2 | 本机测试打印 |
| K3 | 进入 `LABEL SETUP` |
| K4 | 刷新状态 |

`LABEL SETUP` 页上半部可点按切换 `LABEL DATA` / `COMM/WIFI` 标签。每个项目采用
浅蓝项目格 + 白色蓝字内容格；点内容格会进入与测试机 WiFi 设置相同的 ASCII 键盘。
键盘仅刷新改动的字符后缀，SSID 中的 `_` 也会显式画出；密码按生产要求明文显示。

`LABEL DATA` 保存 8 个模板，每套包含：模板名、标题、项目、内容、代码/条码、数量、
份数、工位和 PASS/NG。K1/K2 切换 T1--T8，K4 以掉电安全双副本写入 Flash；以后可直接
切换并调用。

`COMM/WIFI` 可设置：控制器名、产线 ID、WiFi SSID/密码、TCP 监听端口、打印串口波特率
和 IR fallback 开关；IP、ESP-AT STA MAC 为只读显示。MAC 读到一次后独立保存，之后进入
设置页即可显示。K1 重新进行 WiFi 服务端连接，K2 发送当前模板的测试打印，K4 保存。修改
SSID、密码或监听端口后保存会自动重启 WiFi 服务端。

设置页首次进入时整页绘制一次；之后 WIFI 状态、IP、MAC 和在线颜色只刷新各自的值区，
稳定状态不重复发送 LCDM 绘图命令。触摸输入按“按下—释放”锁存，兼容 `sendxy=1` 连续
坐标回包，避免重复进入页面造成闪屏。

## 独立 Flash 区

| 地址 | 内容 |
|---|---|
| `0x0807D800` | 打印控制器配置副本 A |
| `0x0807E000` | 打印控制器配置副本 B |
| `0x0807E800` | 测试机设备/WiFi 配置副本 A（不动） |
| `0x0807F000` | 测试机设备/WiFi 配置副本 B（不动） |
| `0x0807F800` | 测试机学习线束 recipe（不动） |

每个打印控制器记录包含 MCU UID、序号、CRC 和提交标记；断电时旧副本仍可读取。打印
模板、WiFi 服务参数、打印串口参数与标签版式参数都在该独立区域，不与测试机资料混用。

## WiFi 打印通讯

控制器以 ESP-AT STA 模式加入已保存的 AP，并依次配置：

```text
AT
AT+CIPSERVER=0              # 清除 MCU 上次复位前仍在 ESP 内的旧监听
AT+CWMODE=1
AT+CWJAP="<SSID>","<password>"
AT+CIFSR
AT+CIPMUX=1
AT+CIPMODE=0
AT+CIPSERVER=1,<listen_port>
```

成功后状态为 `WIFI SERVER ONLINE`。它接收测试机的
`+IPD,<link>,<length>:<JSON>` 生产请求，按 `(device_uid,event_id)` 去重，先回：

```text
ACK,<event_id>,QUEUED\n
```

打印 ZPL 成功后回：

```text
DONE,<event_id>\n
```

失败则回 `ERROR,<event_id>\n`。丢失 ACK/DONE 后同一 `event_id` 重传只返回既有状态，
不会二次打印。服务器支持多个 ESP-AT link ID、发送队列、CIPSEND prompt、发送超时重试
和 WiFi 断线重连。

打印端与测试机共用同一套 PC3/PB9、115200-8N1 和 `AT+CWJAP` 加入逻辑，但打印端的
SSID/密码/监听端口保存在独立的打印控制器 Flash 区。加入阶段的 `WIFI DISCONNECT` 按
测试机规则视为过渡提示；`AT+CIFSR` 遇到 DHCP 尚未完成时会在限定时间内重试，不再
立即进入 `WIFI RETRY`。

测试机当前 JSON 请求已兼容上述 ACK/DONE/ERROR 格式。打印端亦接受维护用简写：

```text
PRINT,<event_id>,<station>,<quantity>,PASS
```

## 打印机参数

LCDM 外部 `cfg.*` 协议仍可设置标签尺寸、原点、旋转、速度、浓度、字体、条码和串口
参数。每个有效 `cfg.*` 修改现在会同步写入打印控制器双副本 Flash，而不再只保留在 RAM。
默认打印传输为 `PB5=TX`、`PB3=RX`、9600 8N1；实际打印机/RS485 转换器仍需按硬件要求
确认 DE/RE、线序和电平。
