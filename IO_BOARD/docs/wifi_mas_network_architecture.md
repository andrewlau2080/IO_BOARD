# WiFi / MAS 组网与互联架构

本文先固定测试机、本线 AT32 打印主机、AP 网络、MAS 系统、手机/手持机/电脑之间的互联框架。WiFi 模块型号暂不决定；后续无论选 UART WiFi、SPI WiFi 或 ESP32 协处理器，本架构和上层业务协议保持不变。

配套文件：

- 演示 PPT：`docs/wifi_mas_network_architecture.pptx`
- WiFi 模块 PCB 连接规格：`docs/wifi_module_pcb_connection_plan.md`

## 目标规模

| 项目 | 规划 |
|---|---|
| 车间规模 | 一个车间十几条生产拉 |
| 单条生产拉 | 约 10 台测试机 + 1 台打印主机/打印机 |
| 在线对象 | 所有测试机、打印主机、打印机状态都可被 MAS 查看 |
| 客户端 | MAS 系统、电脑网页/客户端、手机 APP、手持机 APP |
| 数据用途 | 在线监控、打印队列、PASS/NG 统计、工位效率、故障点统计、追溯和后续数据分析 |

## 总体拓扑

建议采用“每条线一个微型/工业 AP + 一台本线 AT32 打印主机”的架构。测试机和打印主机都作为 WiFi Station 接入本线 AP；生产控制不依赖 MAS 在线。测试机把测试结果和打印请求发给本线 AT32 打印主机，打印主机本地排队、打印、缓存，再向 MAS 上报。AP 只提供网络连接，MAS 只负责接收、显示、统计、查询和可选资料管理，不参与本线实时生产控制。

```text
                 MAS / Dashboard / Statistics Server
                   |  MQTT Broker + HTTP API + Web UI
                   |
        Factory Ethernet / VLAN / Uplink
                   |
       +-----------+-------------+-------------+
       |                         |             |
  Line 01 Micro/Industrial AP   Line 02 AP   Line NN AP
       |                         |
       |                         |
  +----+-------------------+     +-------------------+
  | Line 01                |     | Line 02           |
  |                        |     |                   |
  | Tester ST01 ... ST10   |     | Tester ST01...    |
  | AT32 Print Host        |     | AT32 Print Host   |
  | Printer                |     | Printer           |
  +------------------------+     +-------------------+
```

推荐层级：

| 层级 | 设备/服务 | 职责 |
|---|---|---|
| 设备层 | 测试机 AT32 + WiFi 模块 | 本地扫描、学习、PASS/NG、故障点、触发打印、上传心跳和结果 |
| 线体层 | AT32 打印主机 + LCDM + 打印机 | 线体主机、工位绑定、标签模板、打印队列、打印状态、本线缓存、MAS 同步 |
| 车间层 | AP/交换机/VLAN | 提供稳定局域网，隔离办公网络和生产设备网络 |
| MAS 服务层 | MQTT Broker、HTTP API、数据库、Web UI | 接收设备上报、实时监控、历史记录、统计、权限、报表 |
| 客户端层 | 电脑、手机、手持机 | 查看状态、统计、报警、参数下发、工单/产品资料维护 |

## 关键原则

| 原则 | 说明 |
|---|---|
| 本地测试不依赖网络 | 测试机断网也必须能完成本地扫描、PASS/NG 显示和基本操作 |
| 打印可本线闭环 | 单条线的测试机和打印主机必须能在 MAS 断线时继续排队、打印和缓存 |
| AT32 完成生产控制 | 测试、对码、换机、绑定、打印、缓存、补传都由测试机和本线 AT32 打印主机完成 |
| 上层协议不绑定 WiFi 模块 | 固件业务层只调用 `net_service` / `line_comm_transport`，不直接依赖 ESP/某厂 AT 指令 |
| MAS 只做上级系统 | 第一版 MAS 服务部署在车间内网；后续可同步到云端或总厂系统，但不影响本线生产 |
| 所有记录带时间和序号 | 每条测试结果、打印任务、状态变化都带 `event_id`、时间戳、设备 ID，便于追溯 |

## MAS 与本线打印的关系

MAS 不是打印、换机、绑定、测试的前置条件。正式运行时按下面分工：

| 对象 | 在线时 | MAS 断线时 |
|---|---|---|
| 测试机 | 发心跳、测试结果、打印请求到本线打印主机；可同时由打印主机转发 MAS | 继续本地测试；测试结果和打印请求仍发给本线打印主机；短时发不出则本机缓存 |
| 本线 AT32 打印主机 | 本地接收 ST01-ST10 请求、排队打印、缓存、上报 MAS | 继续接收本线测试机请求、继续打印；测试/打印记录写入本地缓存 |
| MAS | 接收上报、监控、统计、报表、跨线查询、可选资料管理 | 不影响本线生产；恢复后接收打印主机补传 |
| 手机/电脑/手持机 | 从 MAS 查看全车间数据 | MAS 断线期间无法看到最新全车间数据；本线 LCDM/打印主机仍显示本线状态 |

关键结论：

```text
测试机 -> 本线 AT32 打印主机 -> 打印机
                         |
                         +-> MAS 上报、统计、APP/电脑监控
```

不要设计成：

```text
测试机 -> MAS -> 打印主机 -> 打印机
```

否则 MAS 或上级网络异常时会直接停产。

## 推荐网络模式

第一版推荐：

| 项目 | 建议 |
|---|---|
| WiFi 模式 | 每条拉配置 1 个微型/工业 AP；测试机和本线 AT32 打印主机作为 Station 接入 |
| SSID | 独立生产设备 SSID，例如 `FACTORY-IOBOARD` |
| VLAN | 单独生产设备 VLAN，限制访问范围；AP 不负责业务逻辑 |
| IP | DHCP 为主，服务器按 MAC/设备 ID 做保留地址；设备业务不要依赖固定 IP |
| 时间 | 可由 MAS/NTP 统一授时；离线时 AT32 使用本地递增事件号 |
| 通讯协议 | 本线内轻量消息协议负责测试机到 AT32 打印主机；打印主机再通过 WiFi 模块用 MQTT/HTTP 或简化协议向 MAS 上报 |
| 安全 | WPA2/WPA3；MQTT/HTTP 使用账号 token，后续支持 TLS |

为什么正常模式不建议每条线自组热点：

| 方案 | 问题 |
|---|---|
| 每条线打印主机开 AP | 车间十几条线会有十几个热点，漫游、信道、管理和手机访问都复杂 |
| 手机直接连设备热点 | 同时监控多条线困难，也绕过 MAS 数据库 |
| 设备互相直连 | 后续统计、权限、追溯和跨线监控会变复杂 |

但这里要区分两种断线：

| 断线类型 | 处理 |
|---|---|
| MAS/服务器/上级网络断线，车间 AP 仍工作 | 本线测试机仍通过车间 AP 访问本线打印主机，继续生产打印 |
| 车间 AP 或整条网络不可用 | 需要另设应急链路：打印主机临时应急 AP、RS485/有线备用、或保留现有 IR 触发作为最低限度打印触发 |

应急 AP 不作为正式组网方式，只作为 AP 故障时的降级方案，SSID 可隐藏并只允许本线 ST01-ST10 接入。是否启用应急 AP、还是改用 RS485/IR 备用，需要后续按现场布线和可靠性决定。

## 设备身份

每台设备必须有稳定唯一身份，不能只靠 IP。

| 字段 | 示例 | 说明 |
|---|---|---|
| `device_uid` | `AT32UID...` 或 WiFi MAC | 永久唯一 ID，出厂写入或读取 MCU UID/MAC |
| `device_type` | `tester` / `print_host` / `printer` | 设备类型 |
| `factory_id` | `F01` | 工厂 |
| `workshop_id` | `W01` | 车间 |
| `line_id` | `L01` | 生产拉 |
| `station_id` | `ST01` ... `ST10` | 工位号，仅测试机使用 |
| `short_code` | `A3F9` | 对码时 LEDM/LCDM 显示的短码 |
| `fw_version` | `1.0.0` | 固件版本 |
| `hw_version` | `IOB-2026-07` | 硬件版本 |

绑定关系以本线 AT32 打印主机本地保存为准，MAS 只保存同步后的全车间记录：

```text
device_uid -> factory/workshop/line/station/device_type
```

换机时不改工位，只把新 `device_uid` 绑定到原 `line_id + station_id`。

## MAS 服务组件

| 服务 | 第一版职责 |
|---|---|
| MQTT Broker | 全车间心跳、状态、测试结果、打印事件、命令下发 |
| API Server | 设备注册、绑定记录、资料上传、历史查询、统计查询 |
| Database | 设备表、绑定表、测试记录、打印记录、报警记录、产品资料 |
| Web Dashboard | 车间总览、线体总览、单机监控、统计报表 |
| Mobile/Handheld API | 手机/手持机登录、扫码/查询、报警确认、换机辅助 |
| Time Service | 给无 RTC 设备提供时间同步 |

每条线 AT32 打印主机还需要本地服务：

| 本地服务 | 职责 |
|---|---|
| AT32 Local Protocol | 接收本线 ST01-ST10 心跳、测试结果、打印请求、对码请求 |
| AT32 Print Queue | 打印队列、重试、打印状态 |
| AT32 Local Cache | 缓存测试结果、打印记录、对码记录、资料版本 |
| AT32 Sync State Machine | MAS 在线时上传缓存、下载资料；MAS 离线时继续重连 |
| Line UI / LCDM | 本线状态、对码、打印错误、本地缓存数量 |

部署建议：

| 阶段 | 部署 |
|---|---|
| 开发/试产 | 一台工控机、mini PC 或车间服务器运行 MQTT + API + DB |
| 量产 | 车间服务器或虚拟机，数据库定时备份 |
| 多车间 | 每车间本地服务 + 总厂服务器汇总 |

## MQTT Topic 规划

Topic 使用统一前缀，方便权限隔离和按线订阅。

```text
factory/{factory_id}/workshop/{workshop_id}/line/{line_id}/device/{device_uid}/telemetry
factory/{factory_id}/workshop/{workshop_id}/line/{line_id}/device/{device_uid}/event
factory/{factory_id}/workshop/{workshop_id}/line/{line_id}/device/{device_uid}/state
factory/{factory_id}/workshop/{workshop_id}/line/{line_id}/device/{device_uid}/cmd
factory/{factory_id}/workshop/{workshop_id}/line/{line_id}/device/{device_uid}/ack
factory/{factory_id}/workshop/{workshop_id}/line/{line_id}/print_host/{print_host_uid}/queue
factory/{factory_id}/workshop/{workshop_id}/line/{line_id}/summary
```

本线打印主机本地协议也可使用同样后缀；第一版不要求 AT32 打印主机运行完整 MQTT broker/API，只要求实现等价的本线消息收发和 ACK：

```text
line/{line_id}/device/{device_uid}/telemetry
line/{line_id}/device/{device_uid}/event
line/{line_id}/device/{device_uid}/cmd
line/{line_id}/print/request
line/{line_id}/print/status
```

建议消息方向：

| Topic 后缀 | 方向 | 用途 |
|---|---|---|
| `telemetry` | 设备 -> 打印主机 -> MAS | 周期心跳、RSSI、电压、计数器、当前模式 |
| `event` | 设备 -> 打印主机 -> MAS | 测试完成、NG、打印请求、报警、换机、对码 |
| `state` | 设备 -> 打印主机 -> MAS | 在线/离线、空闲、测试中、PASS、NG、打印中 |
| `cmd` | 打印主机 -> 设备；MAS 命令先到打印主机 | 参数下发、绑定、开始学习、清故障、重启 |
| `ack` | 设备 -> 打印主机 -> MAS | 命令确认、错误码、执行结果 |
| `summary` | 打印主机 -> MAS -> 客户端 | 线体汇总状态，供大屏/APP 快速订阅 |

## 第一版消息格式

第一版建议用 JSON，便于调试和手机/电脑系统接入。AT32 如果 RAM 紧张，WiFi 协处理器或打印主机可负责 JSON 封包；MCU 内部仍可用紧凑结构体。

### 心跳

```json
{
  "msg": "heartbeat",
  "ver": 1,
  "event_id": 12345,
  "ts": "2026-07-06T10:20:30+08:00",
  "device_uid": "AT32-0011223344556677",
  "device_type": "tester",
  "line_id": "L01",
  "station_id": "ST03",
  "state": "READY",
  "rssi": -58,
  "fw": "1.0.0",
  "err": 0
}
```

### 测试结果

```json
{
  "msg": "test_result",
  "ver": 1,
  "event_id": 12346,
  "device_uid": "AT32-0011223344556677",
  "line_id": "L01",
  "station_id": "ST03",
  "product_id": "MODEL-A",
  "serial": "A1B1-000001",
  "result": "PASS",
  "test_count": 238,
  "duration_ms": 1840,
  "connected_pairs": 96,
  "ng_out": 0,
  "ng_in": 0
}
```

NG 时必须带故障点：

```json
{
  "msg": "test_result",
  "result": "NG",
  "ng_type": "missing",
  "ng_out": 17,
  "ng_in": 17,
  "expected_in": 17,
  "actual_in": 0
}
```

### 打印请求

```json
{
  "msg": "print_request",
  "ver": 1,
  "event_id": 12347,
  "line_id": "L01",
  "station_id": "ST03",
  "device_uid": "AT32-0011223344556677",
  "product_id": "MODEL-A",
  "serial": "A1B1-000001",
  "result": "PASS",
  "test_count": 238
}
```

### 打印状态

```json
{
  "msg": "print_status",
  "ver": 1,
  "event_id": 90012,
  "line_id": "L01",
  "print_host_uid": "PRTHOST-L01",
  "station_id": "ST03",
  "queue_id": 456,
  "state": "DONE",
  "printer_state": "READY",
  "error": 0
}
```

### 对码申请

```json
{
  "msg": "pair_req",
  "ver": 1,
  "device_uid": "AT32-0011223344556677",
  "short_code": "6677",
  "device_type": "tester",
  "old_line_id": "L01",
  "old_station_id": "ST03",
  "fw": "1.0.0"
}
```

### 对码分配

```json
{
  "msg": "pair_assign",
  "ver": 1,
  "target_uid": "AT32-0011223344556677",
  "line_id": "L01",
  "station_id": "ST03",
  "product_id": "MODEL-A",
  "profile_rev": 12
}
```

## 测试规划 / CSV Profile 下发

测试规划文档可以先用 CSV 做第一版，方便电脑端编辑和现场调试。关键原则是：CSV/Profile 不直接推给每台测试机，而是先交给本线 AT32 打印主机。打印主机负责校验、保存当前版本，并把压缩后的 profile 下发给 ST01-ST10。

正式 CSV 制作说明见 `docs/csv_test_profile_and_result_format.md`。该文件统一定义测试规划下载、测试结果保存、断网缓存和后补上传的字段，不再使用旧的简化 CSV 表头。

推荐路径：

```text
第一版上位机:
PC 上位机 -> 本线 AP -> 本线 AT32 打印主机 -> ST01-ST10 测试机

正式 MAS:
PC/MAS -> 本线 AT32 打印主机 -> ST01-ST10 测试机
```

断网时的规则：

| 场景 | 处理 |
|---|---|
| MAS 在线 | PC/MAS 上传 CSV，打印主机校验后保存并分发；profile 元数据同步 MAS |
| MAS 离线但本线 AP 正常 | 使用打印主机已保存的当前 profile，继续生产打印；新结果和资料事件先缓存 |
| 需要现场临时更新 | 电脑连接本线生产网，把 CSV 上传到本线打印主机；打印主机本地生效，MAS 恢复后补传 |
| 测试机更换 | 打印主机按原 `STxx` 下发当前 `profile_id/profile_rev`，新测试机不需要重新编辑 CSV |

正式 CSV 第一行固定表头：

```csv
row_type,file_ver,line_id,station_id,device_uid,product_id,product_name,profile_id,profile_rev,profile_crc,work_order,batch_no,serial_no,test_id,event_id,point_no,out_point,in_point,expected_type,expected_group,required,open_limit,short_limit,result,ng_type,actual_out,actual_in,actual_value,print_required,print_state,operator_id,created_at,started_at,ended_at,remark
```

测试规划至少包含 `HEADER + PLAN` 行；测试结果至少包含 `HEADER + RESULT + SUMMARY` 行。规划点位统一使用 `OUT001`...`OUT128` 和 `IN001`...`IN128`，规则类型第一版支持 `CONNECT/OPEN/NC`。

打印主机 AT32 内部保存时不必长期保存完整 CSV 文本，建议保存成紧凑结构：

| 字段 | 说明 |
|---|---|
| `profile_id` | 产品/线序资料 ID |
| `profile_rev` | 版本号，递增 |
| `point_count` | 有效规则数量 |
| `crc32` | 下发和 ACK 校验用 |
| `rules[]` | OUT/IN 对应关系、开路/短路/GND/NC 规则、必测标志 |

下发流程：

```text
1. PC/MAS -> AT32 Print Host: profile_upload(csv/profile)
2. Print Host: 解析、检查 OUT/IN 范围、检查重复和冲突、生成 crc32
3. Print Host: 保存当前 profile，并记录 profile_event
4. Print Host -> Tester ST01-ST10: profile_push(profile_id, rev, crc32, rules)
5. Tester -> Print Host: profile_ack(rev, crc32, result)
6. Print Host -> MAS: profile_sync，MAS 在线立即同步，离线则后补
```

因此电脑端可以有三种实现方式：

| 方式 | 说明 | 建议 |
|---|---|---|
| 简单上位机 | 只负责编辑/导入 CSV、选择线体、上传到本线打印主机 | 第一版优先 |
| MAS 页面 | 在 MAS 内维护产品资料和版本，发布到指定线体打印主机 | 正式系统 |
| U 盘/SD 导入 | 通过打印主机本地接口导入 profile | 可作为网络异常时的维修方案 |

## 程序逻辑分层

测试机固件建议拆成以下逻辑层：

| 模块 | 职责 |
|---|---|
| `io_scan` | 4051 扫描、学习、PASS/NG 判断 |
| `first_gen_4051_scan` | 一代本机流程、LEDM 或 LCDM 显示、按键、打印触发 |
| `line_comm_transport` | 抽象 IR / WiFi / RS485 传输，不处理业务统计 |
| `net_service` | WiFi 连接、本线打印主机会话、MAS 同步会话、重连、时间同步 |
| `device_identity` | UID、短码、绑定信息、固件/硬件版本 |
| `event_queue` | 本机短缓存、事件编号、重发去重；长期缓存放打印主机 |
| `mas_protocol` | JSON/二进制消息封包、命令解析、ACK |

打印主机建议拆成：

| 模块 | 职责 |
|---|---|
| `print_terminal` | LCDM 界面、标签字段、打印队列 |
| `print_driver` | 串口/网口输出 ZPL、ESC/POS、TSPL 等打印语言，或通过 WiFi 协处理器转发 |
| `line_master` | 本线 ST01-ST10 绑定、对码、资料下发、本线 AT32 Local Protocol |
| `mas_client` | 上传本地缓存、打印状态和线体汇总，接收 PC/MAS 上传的资料 |
| `printer_monitor` | 打印机在线、缺纸、错误、队列状态 |

## 测试机状态机

```text
BOOT
  -> WIFI_CONNECTING
  -> REGISTERING
  -> SYNC_CONFIG
  -> READY
  -> LEARNING
  -> TESTING
  -> PASS_READY
  -> PRINT_REQUESTED
  -> PRINT_DONE
  -> READY

任意状态:
  -> OFFLINE_CACHE    网络断开但本地继续工作
  -> ERROR_LOCK       硬件/资料/扫描错误
  -> PAIRING          K1+K4 长按进入对码
```

状态说明：

| 状态 | 行为 |
|---|---|
| `BOOT` | 读取 UID、绑定表、上次配置、计数器 |
| `WIFI_CONNECTING` | 连接 AP；失败时进入本地模式并继续重试 |
| `REGISTERING` | 向本线打印主机注册；打印主机再同步 MAS |
| `SYNC_CONFIG` | 从本线打印主机获取当前线体、工位、产品资料、阈值、模板版本 |
| `READY` | 等待操作员放线束/启动测试 |
| `LEARNING` | 学习标准线序，保存 profile revision |
| `TESTING` | 扫描矩阵，实时显示进度 |
| `PASS_READY` | PASS 后等待 `PB8/HALL_SW` 或线体位置触发 |
| `PRINT_REQUESTED` | 已发送打印请求，等待 ACK/DONE 或超时重发 |
| `OFFLINE_CACHE` | 本地事件入队，网络恢复后补传 |
| `PAIRING` | 周期广播/上报 `PAIR_REQ`，等待绑定 |

## 打印主机状态机

```text
BOOT
  -> WIFI_CONNECTING
  -> REGISTERING
  -> LOAD_LINE_CONFIG
  -> LINE_READY
  -> RECEIVE_PRINT_REQUEST
  -> PRINTING
  -> PRINT_DONE / PRINT_ERROR
  -> LINE_READY

任意状态:
  -> PAIR_MENU
  -> OFFLINE_LINE_MODE
  -> PRINTER_ERROR
```

打印主机职责比测试机重：它是每条线的 AT32 本地主机。即使 MAS 暂时离线，打印主机也应保存 ST01-ST10 绑定表、当前产品资料和打印模板，继续完成本线打印，并在网络恢复后补传记录。

## 生产流程消息顺序

正常测试 + 打印：

```text
1. Tester ST03 -> Print Host: heartbeat READY
2. Tester ST03: 本地扫描
3. Tester ST03 -> Print Host: test_result PASS/NG
4. PASS 时 fixture 到打印位置，PB8/HALL_SW 有效
5. Tester ST03 -> Print Host: print_request
6. Print Host -> Tester ST03: print_ack QUEUED
7. Print Host -> Printer: 输出标签
8. Print Host -> Local Cache + MAS: print_status DONE/ERROR
9. MAS 更新统计、电脑/手机实时刷新
```

对码/换机：

```text
1. 新测试机长按 K1+K4，进入 PAIRING
2. Tester -> Print Host: pair_req(short_code, uid)
3. 打印主机 LCDM 显示申请设备
4. 操作员选择绑定/替换 STxx
5. Print Host -> Tester: pair_assign(line_id, station_id, product/profile)
6. Tester 保存并回 pair_ack
7. Print Host 更新本线绑定表和 binding_event
8. Print Host -> MAS: binding_sync，MAS 在线则立即保存，离线则恢复后补传
9. MAS 更新全车间绑定表，历史记录从新 UID 继续接到原工位
```

资料下发：

```text
1. PC/MAS 上传产品资料或标准线序 profile 到本线 AT32 打印主机
2. Print Host 校验 CSV/profile，生成 profile_id、profile_rev、crc32
3. Print Host 本地保存为当前生产 profile
4. Print Host -> Tester ST01-ST10: profile_push
5. Tester 回 profile_ack(profile_rev, crc32)
6. Print Host -> MAS: profile_sync，MAS 在线立即同步，离线则后补
```

## 数据库主表

第一版数据库至少保留这些表。

| 表 | 关键字段 | 用途 |
|---|---|---|
| `devices` | `device_uid`, `type`, `mac`, `fw`, `hw`, `last_seen` | 设备档案 |
| `line_bindings` | `line_id`, `station_id`, `device_uid`, `active` | 工位绑定 |
| `products` | `product_id`, `name`, `profile_rev`, `label_template_id` | 产品资料 |
| `test_results` | `event_id`, `line_id`, `station_id`, `result`, `ng_out`, `ng_in`, `duration_ms` | 测试记录 |
| `print_jobs` | `queue_id`, `station_id`, `serial`, `state`, `printer_error` | 打印记录 |
| `device_events` | `event_id`, `device_uid`, `event_type`, `payload` | 原始事件追溯 |
| `alarms` | `alarm_id`, `device_uid`, `level`, `code`, `ack_user` | 报警 |
| `profiles` | `profile_id`, `rev`, `matrix_hash`, `content` | 线序/学习资料 |

统计功能从这些表生成：

| 统计 | 来源 |
|---|---|
| 每条线产量 | `test_results` 按 `line_id`、时间聚合 |
| 工位 PASS/NG 率 | `test_results` 按 `station_id` 聚合 |
| 常见故障点 | `ng_out/ng_in/ng_type` 聚合 |
| 打印失败率 | `print_jobs.state/error` 聚合 |
| 设备在线率 | `devices.last_seen` + heartbeat |
| 单件追溯 | `serial` 关联测试和打印记录 |

## 断网和补传

| 场景 | 处理 |
|---|---|
| 测试机到打印主机断开 | 本地继续测试；事件写入测试机短缓存；普通测试机 LEDM 或高档测试机 LCDM 可显示 `OFFL` 提醒；恢复后先补给打印主机 |
| 打印主机到 MAS 断开 | 本线仍可打印；测试和打印记录进入打印主机本地缓存 |
| MAS 服务器离线 | 打印主机保持重连；不丢测试结果；恢复后按 `event_id` 补传 |
| 重复上报 | MAS 按 `device_uid + event_id` 去重 |
| 时间不同步 | 设备先用本地递增序号，恢复后由 MAS 接收时间修正 |

建议缓存容量：

| 设备 | 第一版缓存 |
|---|---|
| 测试机 | 最近 200-1000 条关键事件，作为短缓存；按 Flash 寿命设计写入策略 |
| AT32 打印主机 | 本线主要缓存位置，建议至少最近 5000-50000 条测试/打印/对码/资料事件；优先用 eMMC、SD、外部 Flash 或 FRAM |
| MAS 数据库 | 长期主数据库，恢复联网后由打印主机补传 |
| 打印机本体 | 不建议作为业务缓存，只作为打印缓冲/队列；通常无法可靠保存测试追溯数据 |

### 打印主机本地存储器规划

ESP32-C3-WROOM-02/02U 常见的 4 MB Flash 是 WiFi 模块自身程序、参数、分区和可选 OTA 使用的 Flash，不作为本线生产追溯记录库规划。即使 ESP32 固件开放一部分文件系统，也不建议让 AT32 打印主机把关键测试/打印记录长期写在 WiFi 模块内部 Flash 中，原因是容量、擦写寿命、故障边界和数据恢复都不可控。

打印主机第一版应在 AT32 侧预留独立非易失存储：

| 存储方案 | 建议容量 | 适合内容 | 结论 |
|---|---:|---|---|
| SPI NOR Flash | 16 MB 起，推荐 32 MB 或 64 MB | 测试结果、打印记录、对码记录、profile 版本、补传队列 | 第一版优先，BOM 和驱动复杂度可控 |
| microSD / SD | 1 GB 以上 | 长时间断网、大量详细日志、CSV 导入导出 | 空间充足，但卡座/接触可靠性要评估 |
| eMMC | 4 GB 以上 | 工业化长期记录 | 可靠但 PCB/驱动复杂度较高 |
| FRAM | 256 KB 到数 MB | 高频写入队列头、状态、少量关键记录 | 适合配合 Flash 保存索引，不适合大量日志 |
| AT32 内部 Flash | 少量参数和短缓存 | 绑定表、配置、很少量事件 | 不建议作为 10 台测试机的长期断网记录库 |
| WiFi 模块 4 MB Flash | 不作为业务缓存 | WiFi 固件、模块参数 | 不用于生产追溯记录 |

### SPI NOR Flash IO 规划

按 `SCH_FIXTURE_2026-07-07.pdf` 复核，打印主机本地缓存 Flash 已在原理图中放置为 `U7=W25Q128JVSIQ`，容量 128 Mbit / 16 MB。`PB13/PB14/PB15` 已分别连接 `FLASH_SPI_SCK/MISO/MOSI`，`PA6` 已连接 Flash 片选。`PB12` 已用于 `OUT_BMUX_C2`，不能复用为 SPI 片选。

注意：07-07 原理图网络名当前写成 `FALSH_CS_N` / `FALSH_WP_N`，PCB 前应改为 `FLASH_CS_N` / `FLASH_WP_N`。

| Flash 信号 | AT32 引脚 | LQFP100 脚号 | 25Q SOP8 脚位 | 连接建议 |
|---|---|---:|---|---|
| `FLASH_CS_N` | `PA6` | 31 | Pin 1 `CS#` | 10k 上拉到 3.3V，AT32 GPIO 拉低选中 |
| `FLASH_SPI_MISO` | `PB14` | 53 | Pin 2 `DO/IO1` | SPI2 MISO，串 0R/22R 预留 |
| `FLASH_WP_N` | 不占 MCU | - | Pin 3 `WP#/IO2` | 10k 上拉到 3.3V，留测试点 |
| `GND` | GND | - | Pin 4 `GND` | 接地平面 |
| `FLASH_SPI_MOSI` | `PB15` | 54 | Pin 5 `DI/IO0` | SPI2 MOSI，串 0R/22R 预留 |
| `FLASH_SPI_SCK` | `PB13` | 52 | Pin 6 `CLK` | SPI2 SCK，串 0R/22R 预留 |
| `FLASH_HOLD_N` | 不占 MCU | - | Pin 7 `HOLD#/RESET#/IO3` | 10k 上拉到 3.3V，留测试点 |
| `VCC` | 3.3V | - | Pin 8 `VCC` | 贴近 Flash 放 0.1uF，建议再预留 1uF |

`PB13/PB14/PB15/PA6` 一旦用于本地存储，就从 WiFi 备用脚池中移除。第一版不做 Quad SPI，只做普通 SPI NOR；如果以后要改 QSPI 或更高吞吐，需要重新分配专用 QSPI IO。

容量估算按紧凑二进制记录计算：

| 单条记录大小 | 5000 条 | 50000 条 |
|---:|---:|---:|
| 128 B | 0.64 MB | 6.4 MB |
| 256 B | 1.28 MB | 12.8 MB |
| 512 B | 2.56 MB | 25.6 MB |

如果用 JSON 文本直接保存，单条记录可能到 500-1000 B，容量会更快上涨。因此 AT32 打印主机本地缓存建议保存紧凑二进制事件，上传 MAS 时再由打印主机或 MAS 转成 JSON/数据库记录。

第一版 PCB 建议至少预留 32 MB SPI NOR Flash，最好兼容 16/32/64 MB 同封装型号；如果要求断网多天也不丢详细记录，则预留 microSD 或更大容量存储方案。

## WiFi 模块接口边界

WiFi 模块第一版建议采用 ESP32-C3-WROOM-02/02U 系列，AT32 仍作为业务主控，ESP32-C3 作为网络协处理器。PCB 连接、电源、EN/BOOT、天线和内部 GND 焊盘处理见 `docs/wifi_module_pcb_connection_plan.md`。

先固定 MCU 到网络层的抽象接口，后续即使从 WROOM-02U 改成其它 UART WiFi 模块，上层业务仍不改。

```c
net_status_t net_init(void);
net_status_t net_connect(void);
net_status_t net_is_online(void);
net_status_t net_publish(const char *topic, const uint8_t *payload, uint16_t len);
net_status_t net_poll_message(net_message_t *msg, uint32_t timeout_ms);
net_status_t net_get_time(net_time_t *time);
```

上层只关心：

| 能力 | 要求 |
|---|---|
| 连接 AP | 支持保存 SSID/密码，断线自动重连 |
| MQTT 发布/订阅 | 至少 QoS 1，支持遗嘱/离线通知更好 |
| HTTP GET/POST | 用于注册、配置、资料下载 |
| 时间同步 | NTP 或服务器时间下发 |
| OTA 预留 | 后续可选，不作为第一版必须 |

## WiFi 模块选型延伸

后续选模块时按架构倒推，不先被模块牵着走。

| 方案 | 优点 | 风险/限制 | 适合阶段 |
|---|---|---|---|
| ESP32-C3-WROOM-02U | 外接天线，车间环境更稳；WROOM 封装比 MINI 更适合焊接和返修 | 占板面积比 MINI 大；19-25 内部 GND 焊盘仍需按规格书回流焊处理 | 打印主机第一版推荐 |
| ESP32-C3-WROOM-02 | 板载天线，BOM 少；同属 C3 WROOM 系列 | 天线禁布区要求严格；金属外壳环境不如外接天线稳 | 测试机可选 |
| WiFi 子板 | 主板接口固定，模块由 SMT 厂贴好；返修和换型方便 | 多一个连接器和子板成本 | 开发/维修友好推荐 |
| ESP32-C3-MINI | 体积小，供应成熟 | 焊盘更小，手工返修不友好 | 空间不足时备选 |
| ESP8684/ESP32-C2 | 成本低 | 资源和余量较小；焊接便利性不明显优于 WROOM | C3 稳定后再评估降成本 |
| SPI WiFi 模块 | MCU 控制更强，吞吐较好 | 驱动复杂，AT32 RAM/Flash 压力更大 | 中后期 |
| SDIO WiFi | 性能最好 | MCU 驱动和协议栈复杂，不适合第一版快速落地 | 不优先 |

第一版建议优先评估两条线：

| 路线 | 说明 |
|---|---|
| 测试机 AT32 + ESP32-C3-WROOM-02/02U | AT32 专心扫描，WiFi 侧处理联网；金属环境优先 02U 外接天线 |
| 打印主机 AT32 + ESP32-C3-WROOM-02U | LCDM/触摸、绑定、队列、打印、缓存、MAS 上报都由 AT32 主导；02U 外接天线提升稳定性 |
| WiFi 子板 | 主板只留 `3V3/GND/UART/EN/BOOT/STATUS/WAKE`，后续换模块不大改主板 |

## 与现有固件的关系

现有 `line_comm_transport` 已经把物理通讯和业务分开，WiFi 后端应继续接在这个层下面。

| 现有对象 | WiFi 架构中的位置 |
|---|---|
| `line_comm_print_request_t` | 可作为 `print_request` 消息的 MCU 内部结构 |
| `line_comm_transport_send_print_request()` | PASS 后发送打印请求，WiFi 版本优先发给本线打印主机 |
| `line_comm_transport_poll_print_request()` | 打印主机接收请求；WiFi 版本改为本地订阅/队列，不依赖 MAS 在线 |
| `print_job_model` | 打印主机生成标签内容的基础字段 |
| `station_pairing_plan.md` | 对码流程继续沿用，传输从 IR/本地改为 WiFi/MAS |

## 第一阶段实现清单

| 顺序 | 工作 | 输出 |
|---:|---|---|
| 1 | 定义设备身份和绑定表 | `device_uid`, `line_id`, `station_id`, `short_code` |
| 2 | 增加 `net_service` 抽象 | 不绑定具体 WiFi 模块 |
| 3 | 增加 `event_queue` | 心跳、测试结果、打印请求可缓存补传 |
| 4 | 定义 MQTT topic 和 JSON 消息 | 与 MAS/APP/电脑统一接口 |
| 5 | 定义 CSV/Profile 格式和上传流程 | PC/MAS -> 打印主机 -> ST01-ST10 |
| 6 | 定义 WiFi 模块 PCB 连接 | ESP32-C3-WROOM-02U、UART、EN、BOOT、电源、天线、GND 焊盘 |
| 7 | 打印主机实现 line master | ST01-ST10 对码、profile 分发、队列、打印状态 |
| 8 | MAS 原型服务 | MQTT broker + 简单 API + 数据库 |
| 9 | Web/手机监控页面 | 先做车间/线体/设备状态和统计 |
| 10 | WiFi 模块验证 | ESP32-C3-WROOM-02U 直贴和 WiFi 子板两种 PCB 方案 |

## 当前结论

1. 组网采用集中式车间 LAN，不采用每条线独立热点作为正式方案。
2. MAS 服务是监控、统计和资料管理中心，手机/手持机/电脑优先连接 MAS，不直接控制本线实时生产。
3. 每条线保留一台打印主机作为本线 AT32 打印主机，负责对码、打印队列、本线缓存和 MAS 同步。
4. 测试规划 CSV/Profile 先到本线 AT32 打印主机，由打印主机校验、保存和分发给 ST01-ST10。
5. 测试机本地测试逻辑必须独立于网络，网络只负责上传、监控、资料同步和打印请求。
6. MAS 断线时，本线仍可测试和打印；数据先缓存在打印主机，测试机只做短缓存。
7. 第一版 WiFi 模块建议按 ESP32-C3-WROOM-02U 设计，测试机可视空间和外壳改用 WROOM-02 板载天线。
8. 固件应先抽象 `net_service` 和 `mas_protocol`，避免后续换模块重写业务。
