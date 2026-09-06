# 05 · Bot Status Protocol v1

> 本文为项目自定义协议，不是四家Agent或微雪现有API。唯一机器可读定义为 `contracts/device-message.schema.json`。同一协议服务USB；未来WebSocket仅换帧传输。

## 1. 帧

USB文本帧：`@bot ` + **一行紧凑UTF-8 JSON** + `\n`。JSON正文最多8192字节（不是字符），规范发送帧最多8198字节；接收兼容CRLF时可额外容纳1字节CR，不计入JSON正文预算。普通日志不带此前缀，接收者忽略。禁止把日志和JSON同时不加锁写stdout；序列化输出由一个TX owner处理。

接收器按字节累积；支持拆包、粘包、CRLF、UTF-8跨chunk；超长行丢到下一个换行重新同步，记录一次rate-limited错误。strict JSON：拒绝NaN/Infinity、重复关键键、无效UTF-8、数组根节点和过深嵌套（>12）。

USB波特率配置115200（原生USB并不等同物理UART吞吐限制）。日志颜色关闭；运行联动时Bridge独占串口。RX ring建议16KiB，发送队列有界。

## 2. 通用信封

```json
{"v":1,"type":"focus","link_id":"0123456789abcdef0123456789abcdef","seq":12,"body":{}}
```

`type`决定body。`link_id`为当前连接随机32位小写hex，不是密码。双方每方向独立递增seq（1..2147483647），按收到的更大seq接收；重复或旧seq丢弃。接近上限重新握手，禁止溢出回零。

例外：`hello` 使用 `link_id=null, seq=0`。新连接/复位只接受welcome前的hello流程。错误版本不进入online，不隐式兼容。

## 3. 握手

1. 设备未连接时每2秒发 `hello`，包含device_id、boot_id、handshake_id、固件版本、屏幕和支持协议范围。每次进入handshaking生成新的handshake_id，同一轮hello重试保持不变。
2. Bridge首次收到该handshake_id后生成新link_id，回 `welcome(seq=1)`，包括当前selected_agent、selection_rev和心跳阈值。
3. Host依次发 `catalog`、`focus`、`stats`（初始可全N/A）。双方开始ping/pong。
4. Device接收welcome只限handshaking，清空旧帧seq和pending action，不清空必要的缓存说明。
5. 新boot_id、串口重新枚举或link超时均重新握手。旧link的命令/ack不得生效。

只有新的boot_id/handshake_id或物理连接重建才使旧link失效。同一握手的重复hello重发缓存welcome及最新初始快照，不能不断生成新link造成握手风暴。在线后迟到的同handshake_id hello忽略。不要让上一次连接中排队的selected动作复活。

## 4. 消息与方向

| type | 方向 | body核心 | 更新频率 |
|---|---|---|---|
| hello | D→H | device_id、boot_id、handshake_id、firmware、display | 未握手每2s |
| welcome | H→D | selected_agent、selection_rev、timers | 每次握手 |
| catalog | H→D | 四个agent的health/state/capabilities | 变化时，最多1Hz |
| focus | H→D | 当前agent/run/state/quality/revision | 变化时最多5Hz；完整保活快照每2s |
| stats | H→D | 当前agent、scope、metrics、quotas | 变化时最多1Hz；切换/请求时缓存立即返回 |
| action | D→H | action_id、action、agent_id、base_selection_rev | 用户交互 |
| ack | H→D | action_id、status、权威选择、reason | 控制请求响应 |
| notice | H→D | notice_id、agent、kind、label、expires_in_ms | 去重/合并 |
| ping | H→D | monotonic_ms | 2s |
| pong | D→H | echo_monotonic_ms、device_uptime_ms | 响应ping |

只靠focus保活也必须正常更新link watchdog；只有合法当前link帧才能续命。错误帧/旧seq不能让设备假在线。

## 5. Catalog 与能力

顺序固定 codex/workbuddy/cursor/hermes。每项包含：`id,label,state,health,active_sessions,attention_count,capabilities`。

capabilities分三组：

- state：`observed | reported | manual | none`。
- usage：`automatic | import | manual | none`。
- quota：`official | import | manual | none`。

它们描述当前本机实际配置，不是“未来可能做到”。未安装/未登录也保留卡片，明确health，不藏掉用户要求支持的产品。

## 6. Focus

包含agent_id、selection_rev、session_key/run_id（可null）、state、reason、quality、source_age_ms、stale、tool、detail、run_elapsed_ms、active_sessions、progress。

- progress为0..1或null。没有真实进度时null，不用elapsed估算。
- detail/tool只允许短ASCII白名单标签；不传完整命令/路径。
- host selection_rev更旧的focus/stats忽略；更高revision只有welcome/ack确认选择后可应用，短暂缓存上限1份。
- 相同run/state的focus仅刷新数据，不能重启眨眼、done或toast。
- `stale`表示源证据旧，不等于串口断开；两者可同时发生。

## 7. Stats 与数据质量

每条metric：`key,label,value,unit,quality,coverage,source,as_of_ms,stale_after_ms`。

每条quota：`id,account_key,scope,label,kind,unit,used,limit,remaining,used_pct,resets_at_ms,quality,coverage,source,as_of_ms,stale_after_ms,availability,reason,shared_with`。

- quality：exact/estimated/manual/unavailable/simulated。
- coverage：complete/partial/since_bridge_start/unknown。
- `unavailable`必须value=null；不能使用0占位。
- quota没有数值且不是明确unlimited时，kind=unknown，所有数值null。
- 剩余额度不能依据字符串套餐名推导。完整口径见 `09_USAGE_QUOTA.md`。
- focus与stats中的agent_id/revision必须匹配，避免切到Cursor却仍显示Codex额度。
- Stats字段陈旧可留旧值+STALE，但过了reset的窗口显示 `RESET PENDING`，不能擅自补满。

最多6个metric、2条quota、24个sparkline bucket。大账号完整数据保留在Mac，只将当前屏需要的投影发送给设备。

## 8. Action/ACK

允许action：`select_agent`、`refresh_stats`、`open_agent`、`open_usage`。后两者默认关闭，只有Mac显式配置的allowlist才可用，不接受任意URL或shell命令。

- action_id：随机32hex；一次用户意图重试必须复用。
- base_selection_rev：防止旧UI对错误Agent刷新/跳转。
- select：Host事务提交，ack accepted，发送相同revision的focus/stats；设备才完成。
- refresh：ack accepted表示请求入队，不代表云数据已更新；缓存先呈现，服务端受provider cooldown约束。
- ack status：accepted/rejected；reason枚举ok/conflict/offline/unsupported/rate_limited/invalid/internal_error。
- 请求响应目标500ms，2s重试一次，4s失败。断线时不重试旧action。
- Bridge保存最近256个action结果，TTL60s；同key重复返回同结果。

`open_agent`不是启动任务；`open_usage`只能打开Host已配置官方域名页面。设备不具有批准工具权限。

## 9. 超载与优先级

Host TX：ack/welcome最高；等待/错误focus其次；catalog与notice再次；stats最低。focus按agent合并保留最新；stats按agent合并；队列上限32。不能因反复刷新阻塞pong/控制回应。

设备解析一次只处理一个受限JSON，提取定长结构后立即释放解析树；不要把8KiB整JSON复制到16个队列槽。UI mailbox为4个catalog、1个focus、1个stats和最多3个notice，latest-wins。

## 10. 跨版本

v1固定严格schema。未知字段由当前v1接收器拒绝并发诊断日志，不通过转发漏入下一层；协议小改也更新schema与契约样例，并同步Bridge/固件。为兼容新的计费类别可增加明确定义的schema版本，不能只在UI里猜字段。

## 11. 自检

样例见 `contracts/examples/`；拒绝样例见 `contracts/invalid/`。文档包校验工具检查shape、数值/单位一致性、有限数值、帧字节上限以及非正常值处理。真正的串口拆包/重连还需按T03/T10编写运行时测试。

## 12. 额外的强制语义校验

Quota含 `shared_with`（最多四个唯一agent_id），由Host提供共享提示。CanonicalEvent的 `source_health` 和UsageRecord的 `window_start_ms/window_end_ms` 见状态/统计文档；它们只在Mac内部，不扩大设备业务状态。

机器Schema覆盖结构，校验器还必须检查：catalog四个ID唯一、metric key与unit一致、scope.end>start、waiting/结束reason合法、source_status的health非空、authoritative窗口有效、unknown/unlimited额度数值为null且单位none、accepted ACK对应reason=ok。

## 13. 发送顺序和Host主动选择变化

seq必须在优先级队列**实际出队/写线之前**分配。不能先编号再让高优先级ACK插队，否则对端会把较低seq但仍有效的focus丢弃。一个TX owner串行编码与写入；重发旧ACK结果时信封使用新seq，body/action_id不变。

Host通过本地API改变选择时也发送带权威selected_agent/selection_rev的ACK，再发focus/stats。设备从任何当前link的合法ACK接受更高revision；只有匹配自己pending action_id的ACK用于完成该交互。外部更高revision先到时取消旧pending并同步Host选择，不锁死等待。welcome/ACK都不能将revision倒退。
