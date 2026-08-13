# WiFi 双向打印通讯学习档案（2026-08-13）

> 目的：记录本次调试的教训，供后续会话学习。结论先行，避免重蹈覆辙。

## 一、核心事实：PASS 程序是一次做好的

- 2026-08-06 双向打印通讯 PASS 的程序（commit `9aa5daa`）是 **CODEX 一次做好、零反复修改** 的版本。
- 用户对 WiFi 固件本身不熟悉，依赖验证过的版本；**验证过的程序不要动**。
- PASS 组合（本档案记录时的可运行组合）：
  - 测试侧：`626b17d`（8-06 20:44，样式版；WiFi 逻辑与 9aa5daa 相同）
  - 打印侧：`9aa5daa`（8-06 20:26 PASS 版）
  - 双向通讯实测通过（用户实测 3 次 + 模拟 HALL 验证第 4 次）

## 二、2026-08-13 调试教训（对照）

### 教训 1：打印请求链路被换到"新引擎"，方向就错了
- 验证过的链路：`first_gen_4051_scan.c` 调 `tester_wifi_print_request()`（旧引擎 `tester_wifi_print.c`）。
- 错误链路：调 `tester_host_wifi_request_print()`（新引擎 `tester_host_wifi.c`，未验证）——在未验证的链路上反复改
  MUX/CIPSTART link id/CIPSEND/连接保持/禁用周期测试，全部白费。
- 恢复旧引擎调用后立即正常。

### 教训 2：打印侧"监听失效"= ESP 异常状态，不是固件缺陷
- 现象：ESP 在线（reconnect=0）、回环自检 OK、外部 TCP refused、二层 ping 通。
- 实际原因：**反复烧录（under-reset 会打断 ESP 在线会话）导致 ESP 进入异常态**。
- 正确处置：**断电重启两侧（ESP 冷启动）→ 自动恢复**。不要加保活代码、不要重烧 ESP-AT。
- 错误处置（本日做过，全无效）：周期自检（回环测不到外部）、周期 CIPSERVER=0/1 重启、
  AT+RST（ESP 重启后固件不重跑 ONLINE 链，ESP 裸奔无监听）。

### 教训 3：错误归因（全部被用户否决，均非根因）
1. "AP 隔离拦端到端通信" —— 错（同网段 ping 通，二层正常）。
2. "ESP-AT 需要重烧/重写" —— 错（ESP-AT 验证过没问题，拒绝重烧是对的）。
3. "WiFi 模块有问题" —— 错（模块正常，断电重启即恢复）。

### 教训 4：流程错误
- 遇到异常先断电重启（ESP 冷启动）再排查，不要先怀疑代码/硬件/烧 ESP-AT。
- 不要反复烧录/反复实验（用户明确反对无用功）。
- 探针通道：Mac/Ubuntu 自由切换（Parallels 的 autoconnect 会把探针切到 Mac；
  407 探针 Mac 侧 SWD 不可用，需 `prlctl set 'Ubuntu Linux' --device-connect 'CMSIS-DAP'`
  切回 Ubuntu，407 用 100kHz + under-reset）。

## 三、正确做法（以后照此执行）

1. **程序验证过就不要动**。8-06 PASS 程序（9aa5daa 系列）双向通讯正常。
2. **烧录后异常 → 先断电重启两侧**，再排查其它。
3. 打印请求链路保持旧引擎 `tester_wifi_print_request`（验证过）。
4. 测试侧是**客户端**：MUX=0、无 link id 的 CIPSTART/CIPSEND（settings 手动测试验证格式）。
   打印侧是**服务器**：CIPSERVER 监听。
5. 烧录务必 `--uid` 对应（807=打印侧、407=测试机侧），不得混用。

## 四、关键变量（626b17d/9aa5daa 构建）

- 测试侧：`g_first_gen_hall_active@0x200001ae`、`g_first_gen_print_state@0x200001af`
  （0=IDLE 1=WAIT_HALL 2=WAIT_WIFI_ACK 3=WAIT_WIFI_DONE 4=WAIT_REMOVE 5=ERROR）、
  `g_first_gen_print_request_counter@0x20000190`、`g_first_gen_print_ack_counter@0x20000194`、
  `g_first_gen_print_trigger_count@0x200001ad`、`g_first_gen_print_ready@0x200001a7`、
  `g_tester_wifi_print_ready@0x2001f1cf`、wifi_engine_state@0x2001904b（14=ONLINE）。
- 打印侧：`host_state@0x20007385`（5=ONLINE）、`g_print_terminal_state@0x20007d04`、
  `g_print_terminal_print_count@0x20007cf8`、`g_print_host_wifi_reconnect_count@0x20007380`。
- 跨构建读 RAM 前必须重新 `nm` 当前 ELF（地址会移位）。

## 五、模拟 HALL 触发闭环方法（自测用）

1. 真实 HALL 是 GPIO 电平（固件每周期覆盖 RAM 写入），直接写 RAM 无效。
2. 先写 `g_first_gen_print_state=1`（WAIT_HALL）绕过 WAIT_REMOVE 卡位（真实 HALL 保持时）。
3. 观察：request_counter 递增 → 打印侧 print_count 递增 → state 推进到 4（WAIT_REMOVE=打印完成）。
4. 完整闭环 = 测试侧请求发出 + 打印侧完成打印 + 测试侧显示完成。

## 六、当前固件状态（2026-08-13 晚，双向通讯实测通过）

| 角色 | 版本 | 构建目录 | 向量 | DAP |
|---|---|---|---|---|
| 测试侧 1# | 626b17d（8-06 20:44） | build-update-tester | 0x080145bd | 407 |
| 打印侧 2# | 9aa5daa（8-06 20:26 PASS 版） | build-update-print-host | 0x0800b7e1 | 807 |

- 网络：eSIX_6502CC_2.4G（192.168.1.x），flash 配置区未动（eSIX 配置保留）。
- 验证：用户实测 3 次 + 模拟 HALL 第 4 次，完整闭环 PASS（请求→打印→回执→完成）。
- 源码工作树 = 626b17d（git checkout 626b17d 的 staged 差异未提交）；git HEAD 仍为 6e747e7（8-12 提交链）。
  tester_host_wifi.c/h 为未跟踪文件（8-13 迁移的新引擎，当前构建不包含）。

## 七、后续待办（用户计划继续完善）

- 待用户明确后续需求（当前基线组合 626b17d/9aa5daa 已验证可用）。
- 程序层改动一律基于验证过的版本进行，改动前先确认不影响双向通讯闭环。

