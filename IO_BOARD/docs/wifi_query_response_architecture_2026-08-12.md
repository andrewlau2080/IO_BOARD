# WiFi 连网与 HOST/PORT 同步：查询-响应架构决策档案（2026-08-12）

> 本文档记录 2026-08-12 测试机侧 WiFi 稳定性修复与"连接 WiFi / 同步 HOST/PORT 解耦"
> 架构改动的决策、代码状态、验证结果与待办，供后续会话（含 Codex）无缝接手。
> 结论先行，表格优先。关联文档：`wifi_print_link_status_2026-08-06.md`（上一轮状态）、
> `wifi_mas_network_architecture.md`（一线一 AP + 一打印主机 + ~10 测试机拓扑）。

## 一、用户架构要求（2026-08-12 原话归纳）

1. **连接 WiFi 是一件事**：测试侧、打印侧都先连同一个 AP（互不依赖）。
2. **PD LINE 配对**：两侧均手工输入拉线定义（如 L04），作为配对键。
3. **同步 HOST/PORT 是另一件事**：测试机 WiFi 正常后，**主动要求**同条生产线的打印侧
   给出 HOST 和 PORT（请求-响应，不是打印侧周期广播）。
4. **后续维持**：拿到 HOST/PORT 后建 TCP，测试机所有结果（打印请求、查打印状态等）
   都走这条链路；一个打印侧（TCP server）可关联 10+ 测试机（CIPMUX=1 多客户端）。

## 二、当前固件状态（HEAD: `e042f64`，2026-08-12）

| 角色 | 构建目录 | ELF 入口 | 烧录状态 | DAP 唯一ID |
|---|---|---|---|---|
| 测试机 1# | `build-update-tester` | `0x08015165` | 已烧录，向量匹配/CFSR/HFSR=0 | 尾号 407 `913575030040A0401D149407` |
| 打印侧 2# | `build-update-print-host` | `0x0800c549` | 已烧录，向量匹配/CFSR/HFSR=0 | 尾号 807 `08703D0500C0643C1014A807` |

现场配置（测试机 RAM `current_config` 实读）：machine_id=IOB-0319, station=WT-01,
PD LINE=L04, SSID=TESTER-ROUTER, 手动 HOST=192.168.31.153（PORT=137 为旧值，用户已把
打印侧监听端口改为 5006 做自动更新验证）。

## 三、本次提交链（2026-08-12，均未推送）

| 提交 | 内容 |
|---|---|
| `813b31e` | 测试机侧按打印侧方案修复 ONLINE 期间断线：WIFI DISCONNECT/CLOSED 在
  ONLINE 期间只 JOIN 级重连/轻量重连 TCP，不杀会话；重连序列中忽略伴随 CLOSED |
| `4921e75` | 打印侧信标改常开链路（已废弃，见下）；测试机侧 DISCOVER 超时兜底建 TCP |
| `e042f64` | **当前方案**：查询-响应式 HOST/PORT 同步（见第四节） |

## 四、查询-响应架构（commit `e042f64`，当前生效）

### 为什么改
- 打印侧周期 UDP 广播信标（CIPSTART→CIPSEND→CIPCLOSE 每 3s）实测每 ~5s 打掉 ESP WiFi
  （mode 0/2 均验证），652f941 起禁用 → 测试机侧永远等不到信标 → HOST/PORT 无法自动更新。
- 用户明确方向：**测试机侧主动查询**，打印侧按需回复；连接 WiFi 与同步 HOST/PORT 解耦。

### 工作流程
```text
[打印侧] ONLINE 后开 link4 UDP 通配监听 5002（mode2 纯接收，常开，不周期广播）
[测试机] 连 AP → READ_IP → 开 UDP 广播链路(255.255.255.255:5002 mode0)
          → CIPSEND 查询帧 {"type":"query","line_id":"L04"}
          → 关广播 → 重开 UDP 通配监听 5002（mode2）
[打印侧] 收到 +IPD,4 查询 → 解析 line_id → 匹配自身 → 临时切广播链路
          → 广播回复 {"type":"beacon","line_id":"L04","ip":"...","port":5006}
          → 回复完自动关广播、重开监听
[测试机] DISCOVER_WAIT 收到匹配 beacon → 更新 HOST/PORT 字段 → 关监听
          → SET_MUX → SET_MODE → CONNECT_HOST → ONLINE
[兜底]   查询/监听超时（5s）→ 关 UDP 用现有手动配置直接建 TCP（先建连后更新）
```

### 打印侧改动（`print_host_wifi.c`）
- `HOST_CMD_BEACON_START` 参数改为 `AT+CIPSTART=4,"UDP","0.0.0.0",0,5002,2`（通配监听）。
- 新增 `HOST_CMD_BEACON_TX`（`AT+CIPSTART=4,"UDP","255.255.255.255",5002,5002,0` 临时广播）。
- 新增 `host_handle_query()`：+IPD,4 载荷含 `"type":"query"` → 解析 line_id →
  匹配 `print_terminal_store_get()->line_id` 才回复，非本线忽略。
- 回复序列：关监听(BEACON_CLOSE) → 切广播(BEACON_TX) → CIPSEND(host_queue_beacon)
  → SEND OK(host_beacon_tx_done) → 关广播 → 重开监听。由命令 OK / tx_done 驱动。
- 周期服务 ONLINE 分支：**不再周期广播**；只保证监听链路开着 + 驱动回复流程。
- `host_queue_beacon()` 抽为公共函数（信标 JSON 构造：line_id/ip/wifi_listen_port）。

### 测试机侧改动（`tester_wifi_print.c`）
- 枚举新增 `WIFI_ENGINE_DISCOVER_QUERY` / `DISCOVER_QUERY_RESULT` / `DISCOVER_LISTEN`
  （原 DISCOVER_WAIT/CLOSE 保留，语义调整为监听/关链路）。
- `DISCOVER` 命令改为广播链路（255.255.255.255:5002 mode0）；`DISCOVER_LISTEN` 为重开
  通配监听（0.0.0.0:5002 mode2）。
- 查询帧：`{"type":"query","line_id":"%s"}\n`（运行时按配置 line_id 构造）。
- 状态流：DISCOVER OK → 构造查询帧+CIPSEND → `>` 发帧 → SEND OK → CLOSE(广播)
  → LISTEN → WAIT(等回复) → 命中/超时 → CLOSE(监听) → SET_MUX 建 TCP。
- `wifi_discover_listening` 标志区分"关的是广播（去 LISTEN）还是监听（去建 TCP）"。
- 查询失败/超时均兜底：用现有手动配置直接建 TCP（不无限等信标）。

### 复用与兼容
- 测试机侧 `wifi_handle_discover_beacon()` 不变（beacon JSON 格式相同）。
- 设置页 `wifi_discovered_*` 接口不变（HOST/PORT 自动填充、多控制器选择照常工作）。
- 打印侧业务 TCP（CIPMUX=1, link0-3 客户端 + link4 UDP）不受影响。

## 五、验证状态

| 项 | 结果 |
|---|---|
| 两侧编译（-Wall -Wextra） | PASS（仅已知 RWX 链接警告） |
| 烧录 + 向量表 | PASS（测试机 0x08015165 / 打印侧 0x0800c549，读回一致） |
| 干净启动 | PASS（两侧 CFSR/HFSR=0, Core Running） |
| 源码结构/二进制核对（ad-hoc 脚本 15 项） | PASS（查询/监听命令、查询帧、beacon 模板、状态流齐全） |
| **端到端（查询→回复→端口 5006 自动更新→TCP 建连）** | **未验证**：两侧 ESP 间歇性静默阻塞 |

## 六、已知问题与下一步

1. **ESP 间歇性静默（既有 open item，本次改动未引入）**：两侧 ESP boot 后对 AT 无响应
   （rx_frame=0、rx_error 偶涨、边沿速率异常低），EN 热复位只能短暂激活。观察特征与
   652f941 会话末记录一致。**下一步：两侧物理断电 10s 冷启动**（EN 复位是热复位救不了）。
   若冷启动后仍静默，再对比两侧软件流程差异（用户坚持勿先归因硬件）。
2. **验证目标**（冷启动后采样）：
   - 打印侧：约 6s 深绿 ONLINE → link4 监听开（`host_beacon_link_open`=1）
   - 测试机侧：DISCOVER 广播查询 → 收到回复 → `wifi_discovered_port` 应变为 5006
     （用户改的端口）→ 设置页 PRINT PORT 自动更新 → TCP 建连 ONLINE
   - 采样地址以**重新 nm 当前 ELF** 为准（跨构建地址会变，勿用本文档旧地址）。
3. **端口旧值 137**：测试机侧手动 PORT=137 是信标机制残留旧值；验证时应以信标自动
   更新结果为准（打印侧当前 5006）。
4. 提交 `4921e75`（常开链路周期广播）已被 `e042f64` 取代其打印侧部分，保留在历史中。
