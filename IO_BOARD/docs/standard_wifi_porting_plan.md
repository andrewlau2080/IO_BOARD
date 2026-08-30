# 原项目按标准 WiFi 通讯模组修改方案（2026-08-19）

> 标准 WiFi 通讯模组（闭环消息流版）已上传 GitHub：
> 提交 `7f18196`，标注 **"标准WIFI通讯模组供后续调用"**。
> 本文件记录将该标准移植回原项目（T 侧/ P 侧）的修改方案。

## 一、标准模组的通信模型（已验证）

```
标准 TCP/IP 请求-响应模式（行业通用）：
- 传输层：TCP 自动 ACK + 重传（ESP 固件内部）——保证消息必达
- 应用层：请求-响应（响应消息即确认，无应用层双向 ACK）
  T 发 PRINT RQ → P 回 PRINTING（收到确认）→ P 发 PRINT COMPLETE（最终响应）
  T 收不到 COMPLETE → 超时 NG → 重发 RQ（标准超时重试）
- 无双向确认交织（只有发送方在等，接收方不等发送方回执——不死锁）
```

验证结果：T OK=25 NG=0 持续运行，消息流对向一致（T 发=P 收显示、P 发=T 收显示）。

## 二、原项目现状与问题（Phase A 补丁堆叠）

| 角色 | 现状 | 问题 |
|---|---|---|
| T 侧（tester_wifi_print.c） | 发请求→等 ACK→等 DONE；补丁：超时→EN 复位、dup→立即 EN、job_fail→EN、DONE 超时 15s | EN 复位循环、时序耦合、两侧规则不一致（CIPSEND 无 link） |
| P 侧（print_host_wifi.c） | 收请求→回 ACK→处理→DONE；补丁：dup 累计、at-net 错误→EN | 与 T 侧规则不一致、确认时序耦合 |

## 三、修改方案（先 T 后 P，改完即下载烧录测试）

### T 侧（tester_wifi_print.c）——标准请求-响应

1. **传输规则两侧一致**：`AT+CIPSEND=%u` → `AT+CIPSEND=0,%u`（CIPMUX=1 + link 0，与标准模组一致）
2. **去掉 EN 复位循环/补丁**（EN 只上电 init 用一次——用户原则）：
   - `wifi_job_fail()` 内的 `wifi_esp_en_reset()` 移除
   - `wifi_esp_session_note_end()` / `wifi_esp_session_note_end_if_silent()` 的累计 EN 移除（或仅保留纯计数诊断）
   - dup 检测的立即 EN 移除
3. **标准重试**：等响应超时 → 同连接重发（快）→ 连续 3 次失败 → 报错（EVENT_ERROR）→ 下一请求（不 EN）
4. 保留原业务协议（事件 ID / JSON 帧 / 请求队列——生产语义不动）

### P 侧（print_host_wifi.c）——标准响应

1. 收请求 → 回 **PRINTING**（收到确认，替代旧 ACK 时序）→ 处理 → 回 **PRINT COMPLETE**（最终响应，替代旧 DONE 时序）
2. **去掉 EN 复位补丁**（at-net 错误 → EN 移除；EN 只上电一次）
3. 传输规则与 T 侧一致（CIPSEND 带 link）
4. 保留原业务协议（请求队列 / seen 去重 / JSON 帧）

### 验证方式

- 改完 T 侧 → 烧录 T（407）→ 与标准模组 P 对测（或双板联测）→ 通过后改 P 侧
- 烧录 P（807）→ 双板闭环验证（Test Qty OK/NG 计数）
- 两侧显示一致（用户定稿布局）

## 四、原则（用户重申）

- EN 复位（PA8）**只上电 init 用一次**——"如果常用就是程序没写好"
- 两侧 AT 配置/显示规则**逐项一致**，不许一侧一个做法
- 不加多余机制（"不要自作聪明加插件"）——标准模式即可
- 改完即烧录测试，不空谈

记录时间：2026-08-19
