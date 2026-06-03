# 有效中间握手段时序说明
本文只保留 `.logicdata` 中真正有用的中间握手段。前后重复出现的打印机周期轮询帧不参与协议推导，只作为背景。

## 时间零点
- `T0 = 5.795426 s`，定义为触发测试机响应的那一帧打印机 TX 起点。
- 后文所有时间均为相对 T0 的毫秒。

## 有效步骤
|步骤|相对起点 ms|设备|信号|持续 ms|说明|
|---:|---:|---|---|---:|---|
|1|0.000|打印机|trigger_poll_tx|36.693|触发测试机响应的轮询包，71 段 MARK/SPACE|
|2|0.215|测试机|rx_trigger_poll|36.693|测试机 RX 看到打印机触发包|
|3|24.786|测试机|tester_response_tx|2151.005|测试机开始发长响应包，3321 段 MARK/SPACE|
|4|2036.779|打印机|resume_or_ack_tx|36.693|打印机暂停约 2 s 后恢复发轮询包，可视为确认/清除条件|
|5|2175.791|测试机|response_tx_end|0|测试机响应结束，进入等待清除/下一轮|

## 直接用于 MCU 的文件
- `useful_printer_trigger_tx_envelope.csv`：打印机触发包，完整 71 段。
- `useful_tester_response_tx_envelope.csv`：测试机应发出的长响应包，完整 3321 段。
- `useful_printer_resume_ack_tx_envelope.csv`：打印机恢复/确认包，包络与触发包相同。
- `useful_middle_timeline.csv`：上述几组包在中间有效段中的相对时间位置。

## MCU 状态机按此实现
1. 线路测试合格后置位 `print_pending`，测试机 TX 保持关闭，RX 等待打印机触发包。
2. RX 识别到 `trigger_poll_tx` 包后，延时到相对触发包起点约 24.786 ms。
3. 开始按 `useful_tester_response_tx_envelope.csv` 开关 38.8 kHz 左右红外载波。
4. 打印机在约 2036.779 ms 恢复发同一类 71 段包；测试机识别到恢复包后清除 `print_pending`。
5. 若未识别到恢复包，保留 `print_pending` 或按超时策略重发，不要提前清除测试通过状态。
