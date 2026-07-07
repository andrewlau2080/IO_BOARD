# CSV 测试规划与结果记录格式说明

本文定义测试机、打印主机、MAS、电脑上位机之间统一使用的 CSV 文件格式。以后无论是电脑/MAS 下载测试规划到测试机，还是测试机/打印主机保存测试结果，都按本文格式处理。

说明：用户口头提到的 `CVS` 在本文统一写作 `CSV`，即逗号分隔文本文件。

## 设计目标

| 目标 | 定义 |
|---|---|
| 一个格式 | 测试规划、测试结果、打印记录使用同一个 CSV 表头 |
| 可离线 | MAS 断线时，打印主机可按同一格式缓存和后补上传 |
| 可人工编辑 | 电脑端可用 Excel/WPS/文本编辑器制作，但必须按字段规则保存 |
| 可追溯 | 每份文件带产品、线体、工位、版本、结果、时间、设备 UID 和 CRC |
| 可校验 | 打印主机和测试机必须检查点位范围、重复、冲突、版本和校验值 |

## 文件基本要求

| 项目 | 要求 |
|---|---|
| 文件扩展名 | `.csv` |
| 编码 | `UTF-8`，建议带 BOM，方便 Excel 正常打开中文 |
| 换行 | `CRLF` 或 `LF` 均可，设备导出默认 `CRLF` |
| 分隔符 | 英文逗号 `,` |
| 引号 | 字段内包含逗号、双引号或换行时，用英文双引号包住；双引号本身写成两个双引号 |
| 小数 | 当前不使用小数 |
| 日期时间 | `YYYY-MM-DD HH:MM:SS`，无 RTC 时可留空，由打印主机/MAS补 |
| 点位编号 | 固定三位：`OUT001`...`OUT128`，`IN001`...`IN128` |
| 空字段 | 未使用字段留空，不写 `NULL` |

## 行类型

同一个 CSV 文件用 `row_type` 区分行用途。

| row_type | 用途 | 由谁生成 | 由谁读取 |
|---|---|---|---|
| `HEADER` | 文件头和产品资料 | 电脑/MAS/打印主机 | 打印主机/测试机/MAS |
| `PLAN` | 单个测试点规划 | 电脑/MAS/打印主机/学习功能 | 打印主机/测试机 |
| `RESULT` | 单个测试点结果 | 测试机/打印主机 | 打印主机/MAS/电脑 |
| `SUMMARY` | 一次完整测试汇总 | 测试机/打印主机 | 打印主机/MAS/电脑 |
| `PRINT` | 打印状态记录 | 打印主机 | MAS/电脑 |
| `COMMENT` | 注释 | 人工/上位机 | 设备可忽略 |

下载测试规划时，文件至少包含 `HEADER + PLAN`。保存测试结果时，文件包含 `HEADER + RESULT + SUMMARY`，也可以同时保留原 `PLAN` 行。

## 统一表头

第一行必须完全使用下面的字段名和顺序：

```csv
row_type,file_ver,line_id,station_id,device_uid,product_id,product_name,profile_id,profile_rev,profile_crc,work_order,batch_no,serial_no,test_id,event_id,point_no,out_point,in_point,expected_type,expected_group,required,open_limit,short_limit,result,ng_type,actual_out,actual_in,actual_value,print_required,print_state,operator_id,created_at,started_at,ended_at,remark
```

字段说明：

| 字段 | 必填范围 | 说明 |
|---|---|---|
| `row_type` | 全部必填 | `HEADER/PLAN/RESULT/SUMMARY/PRINT/COMMENT` |
| `file_ver` | `HEADER` 必填 | 当前固定 `1` |
| `line_id` | 建议必填 | 产线编号，例如 `L01` |
| `station_id` | 测试机相关行必填 | 工位编号，例如 `ST01`...`ST10`；整线资料可留空 |
| `device_uid` | 结果行必填 | AT32 唯一 ID 或设备序列号 |
| `product_id` | `HEADER/PLAN/RESULT/SUMMARY` 必填 | 产品型号，例如 `MODEL-A` |
| `product_name` | 可选 | 产品名称 |
| `profile_id` | `HEADER/PLAN/RESULT/SUMMARY` 必填 | 测试规划编号，例如 `P-MODEL-A-001` |
| `profile_rev` | `HEADER/PLAN/RESULT/SUMMARY` 必填 | 测试规划版本，建议递增整数 |
| `profile_crc` | `HEADER/SUMMARY` 必填 | 对所有有效 `PLAN` 行计算的 CRC32，8 位十六进制 |
| `work_order` | 可选 | 工单号 |
| `batch_no` | 可选 | 批次号 |
| `serial_no` | 结果/打印建议必填 | 产品流水号或条码 |
| `test_id` | 结果/汇总必填 | 单次测试 ID，可由打印主机分配 |
| `event_id` | 结果/打印必填 | 事件 ID，断网补传去重用，单设备递增或全线唯一 |
| `point_no` | `PLAN/RESULT` 必填 | 测试点序号，从 `1` 递增 |
| `out_point` | `PLAN/RESULT` 按类型填写 | 输出点，例如 `OUT017` |
| `in_point` | `PLAN/RESULT` 按类型填写 | 输入点，例如 `IN017` |
| `expected_type` | `PLAN/RESULT` 必填 | 期望关系类型，见下表 |
| `expected_group` | 可选 | 多点短接组编号，例如 `G001` |
| `required` | `PLAN` 必填 | `1` 必测，`0` 参考/可选 |
| `open_limit` | 可选 | 开路判定阈值，第一版可留空 |
| `short_limit` | 可选 | 短路判定阈值，第一版可留空 |
| `result` | `RESULT/SUMMARY/PRINT` 必填 | `PASS/NG/SKIP/ERROR/DONE/FAIL` |
| `ng_type` | NG 时必填 | 故障类型，见下表 |
| `actual_out` | `RESULT` 可选 | 实测输出点 |
| `actual_in` | `RESULT` 可选 | 实测输入点 |
| `actual_value` | 可选 | 电压、电阻、ADC值或其它测量值 |
| `print_required` | `SUMMARY` 建议必填 | `1` 需要打印，`0` 不打印 |
| `print_state` | `PRINT/SUMMARY` 可选 | `NONE/QUEUED/PRINTING/DONE/ERROR` |
| `operator_id` | 可选 | 操作员编号 |
| `created_at` | `HEADER` 建议必填 | 文件/规划创建时间 |
| `started_at` | `SUMMARY` 建议必填 | 测试开始时间 |
| `ended_at` | `SUMMARY` 建议必填 | 测试结束时间 |
| `remark` | 可选 | 备注 |

## expected_type 定义

| expected_type | 含义 | `out_point` | `in_point` | 判断逻辑 |
|---|---|---|---|---|
| `CONNECT` | 指定 OUT 应接到指定 IN | 必填 | 必填 | 扫描到该连接为 PASS |
| `OPEN` | 指定 OUT 不应接到任何 IN | 必填 | 可空 | 扫描到任意连接为 NG |
| `NC` | 指定 IN 或 OUT 不参与测试 | 可填 | 可填 | 设备忽略此点，仅记录 |
| `SHORT_GROUP` | 多点允许互通的一组线 | 必填 | 必填 | 同一 `expected_group` 内允许互通，组外短路为 NG |
| `GND` | 指定点应接地 | 可填 | 可填 | 后续有 GND 检测硬件时启用 |
| `SHIELD` | 屏蔽线/外壳连接 | 可填 | 可填 | 后续按专用规则检测 |

第一版主逻辑优先支持 `CONNECT`、`OPEN`、`NC`。`SHORT_GROUP/GND/SHIELD` 先保留字段，不影响文件格式。

## ng_type 定义

| ng_type | 含义 |
|---|---|
| 空 | PASS 或未测试 |
| `OPEN` | 应连接但未连接 |
| `SHORT` | 出现不允许的短路 |
| `WRONG_WIRE` | 接到错误 IN/OUT |
| `EXTRA` | 多出未规划连接 |
| `GND_FAIL` | 接地检测失败 |
| `VALUE_FAIL` | 模拟量/电阻/ADC 阈值失败 |
| `PROFILE_ERROR` | 规划文件错误 |
| `DEVICE_ERROR` | 测试机硬件或通讯错误 |

## 测试规划 CSV 示例

```csv
row_type,file_ver,line_id,station_id,device_uid,product_id,product_name,profile_id,profile_rev,profile_crc,work_order,batch_no,serial_no,test_id,event_id,point_no,out_point,in_point,expected_type,expected_group,required,open_limit,short_limit,result,ng_type,actual_out,actual_in,actual_value,print_required,print_state,operator_id,created_at,started_at,ended_at,remark
HEADER,1,L01,,,,MODEL-A,主线束A,P-MODEL-A-001,3,7A91C2E5,WO20260707001,B20260707,,,,,,,,,,,,,,,,,,OP01,2026-07-07 10:00:00,,,规划文件
PLAN,1,L01,ST01,,,MODEL-A,,P-MODEL-A-001,3,7A91C2E5,,,,,,1,OUT001,IN001,CONNECT,,1,,,,,,,,,,,,,
PLAN,1,L01,ST01,,,MODEL-A,,P-MODEL-A-001,3,7A91C2E5,,,,,,2,OUT002,IN002,CONNECT,,1,,,,,,,,,,,,,
PLAN,1,L01,ST01,,,MODEL-A,,P-MODEL-A-001,3,7A91C2E5,,,,,,3,OUT003,,OPEN,,1,,,,,,,,,,,,,
COMMENT,1,L01,,,,MODEL-A,,P-MODEL-A-001,3,,,,,,,,,,,,,,,,,,,,,,,,示例：OUT003 不允许接到任何输入
```

## 测试结果 CSV 示例

```csv
row_type,file_ver,line_id,station_id,device_uid,product_id,product_name,profile_id,profile_rev,profile_crc,work_order,batch_no,serial_no,test_id,event_id,point_no,out_point,in_point,expected_type,expected_group,required,open_limit,short_limit,result,ng_type,actual_out,actual_in,actual_value,print_required,print_state,operator_id,created_at,started_at,ended_at,remark
HEADER,1,L01,ST01,AT32-0011223344556677,MODEL-A,主线束A,P-MODEL-A-001,3,7A91C2E5,WO20260707001,B20260707,A1B1-000001,T202607070001,10001,,,,,,,,,,,,,1,DONE,OP01,2026-07-07 10:00:00,2026-07-07 10:05:01,2026-07-07 10:05:04,
RESULT,1,L01,ST01,AT32-0011223344556677,MODEL-A,,P-MODEL-A-001,3,7A91C2E5,WO20260707001,B20260707,A1B1-000001,T202607070001,10002,1,OUT001,IN001,CONNECT,,1,,,PASS,,OUT001,IN001,,1,DONE,OP01,,2026-07-07 10:05:01,2026-07-07 10:05:04,
RESULT,1,L01,ST01,AT32-0011223344556677,MODEL-A,,P-MODEL-A-001,3,7A91C2E5,WO20260707001,B20260707,A1B1-000001,T202607070001,10003,2,OUT002,IN002,CONNECT,,1,,,NG,OPEN,OUT002,,,1,NONE,OP01,,2026-07-07 10:05:01,2026-07-07 10:05:04,应接 IN002 未检测到
SUMMARY,1,L01,ST01,AT32-0011223344556677,MODEL-A,主线束A,P-MODEL-A-001,3,7A91C2E5,WO20260707001,B20260707,A1B1-000001,T202607070001,10004,,,,,,,,,NG,OPEN,,,,1,NONE,OP01,,2026-07-07 10:05:01,2026-07-07 10:05:04,fail_count=1
```

## 制作测试规划的步骤

1. 固定第一行表头，不得改字段名和字段顺序。
2. 新增一行 `HEADER`，填写 `file_ver/product_id/profile_id/profile_rev/profile_crc` 等基本信息。
3. 每一个需要测试的连接写一行 `PLAN`。
4. 普通一对一线序使用 `expected_type=CONNECT`，填写 `out_point` 和 `in_point`。
5. 不允许有连接的点使用 `expected_type=OPEN`。
6. 暂不测试但需要保留说明的点使用 `expected_type=NC`。
7. 点位必须在当前 profile 范围内：96 点机不能写 `OUT097` 以后；128 点机允许到 `OUT128/IN128`。
8. 同一个 `out_point + in_point + expected_type` 不得重复。
9. 同一 `out_point` 若是普通一对一 `CONNECT`，原则上只能对应一个 `in_point`；多点互通必须使用 `SHORT_GROUP`。
10. 保存为 UTF-8 CSV 后，由电脑上位机或打印主机计算 `profile_crc` 并写回 `HEADER` 和所有 `PLAN` 行。

## 下载与分发流程

```text
电脑上位机 / MAS
    -> 导入 CSV
    -> 检查格式、点位范围、重复和冲突
    -> 计算 profile_crc
    -> 下发给本线 AT32 打印主机
    -> 打印主机保存到本地 NOR Flash
    -> 打印主机按 station_id/profile_id 下发给 ST01-ST10 测试机
    -> 测试机保存当前 profile_rev/profile_crc
```

MAS 离线时，电脑可以直接连本线 WiFi/AP 或打印主机维护口，把 CSV 交给打印主机。打印主机先本地保存，MAS 恢复后再补传 `profile_sync`。

## 测试结果保存流程

```text
测试机完成测试
    -> 生成 RESULT 行和 SUMMARY 行
    -> 发给本线 AT32 打印主机
    -> 打印主机写入本地 NOR Flash 缓存
    -> 如需打印，生成 PRINT 记录并驱动打印机
    -> MAS 在线时上传
    -> MAS 离线时保留缓存，恢复后按 event_id 补传
```

打印主机保存结果时必须保留 `profile_id/profile_rev/profile_crc`，这样以后即使测试规划已更新，也能追溯当时按哪个版本测试。

## 设备校验规则

| 校验项 | 打印主机 | 测试机 |
|---|---|---|
| 表头字段完全一致 | 必须检查 | 可检查 |
| `file_ver=1` | 必须检查 | 必须检查 |
| 点位范围 | 必须检查 | 必须检查 |
| `row_type` 合法 | 必须检查 | 必须检查 |
| `expected_type` 合法 | 必须检查 | 必须检查 |
| 重复 `point_no` | 必须检查 | 必须检查 |
| 一对一冲突 | 必须检查 | 必须检查 |
| `profile_crc` | 必须计算并比对 | 必须比对 |
| `profile_rev` 是否比本机旧 | 必须提示/禁止降级，除非人工确认 | 必须按打印主机命令处理 |

## CRC32 计算范围

`profile_crc` 只对有效 `PLAN` 行计算，不包括 `HEADER/RESULT/SUMMARY/PRINT/COMMENT`。计算前使用规范化文本：

```text
point_no,out_point,in_point,expected_type,expected_group,required,open_limit,short_limit\n
```

按 `point_no` 从小到大排序后逐行拼接，再计算标准 CRC32，输出 8 位大写十六进制，例如 `7A91C2E5`。

这样做的目的：同一份测试规划即使修改了产品名称、备注、工单号，`profile_crc` 不变；只有测试点定义变化时，`profile_crc` 才变化。

## 文件命名建议

测试规划：

```text
PROFILE_<product_id>_<profile_rev>_<profile_crc>.csv
```

例：

```text
PROFILE_MODEL-A_003_7A91C2E5.csv
```

测试结果：

```text
RESULT_<line_id>_<station_id>_<serial_no>_<test_id>.csv
```

例：

```text
RESULT_L01_ST03_A1B1-000001_T202607070001.csv
```

整线导出：

```text
LINELOG_<line_id>_<date>.csv
```

例：

```text
LINELOG_L01_2026-07-07.csv
```

## 第一版实施边界

第一版必须实现：

| 项目 | 第一版要求 |
|---|---|
| 规划导入 | 支持 `HEADER + PLAN` |
| 结果保存 | 支持 `HEADER + RESULT + SUMMARY` |
| 点位 | 支持 `OUT001`...`OUT128`、`IN001`...`IN128` |
| 测试类型 | 支持 `CONNECT/OPEN/NC` |
| 故障类型 | 支持 `OPEN/SHORT/WRONG_WIRE/EXTRA/DEVICE_ERROR` |
| 缓存 | 打印主机保存到外部 SPI NOR Flash |
| 补传 | 按 `event_id + test_id` 去重 |

后续可扩展：

| 项目 | 扩展方向 |
|---|---|
| `SHORT_GROUP` | 支持多点互通线束 |
| `GND/SHIELD` | 增加接地、屏蔽层测试 |
| 阈值 | 使用 `open_limit/short_limit/actual_value` 保存模拟量 |
| 签名 | 增加文件签名，防止人工误改 |
| 压缩 | 大批量结果可打包为 ZIP 后上传 MAS |
