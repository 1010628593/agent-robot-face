# 09 · 使用量、额度与可信度

## 1. 不能混为一谈的六类数值

| 数据 | 含义 | 常见误用 |
|---|---|---|
| turns | 用户提交的一轮任务 | 误称请求数、扣费次数 |
| API requests | 实际provider调用 | 将一次tool误计为一次请求 |
| token usage | 已观察/返回的消耗 | 把context占用当累计消耗 |
| context occupancy | 当前上下文窗占用 | 当成套餐剩余额度 |
| quota / credit | 账号/窗口允许资源 | 用token换算所有厂商积分 |
| local budget | 用户自行设置预算 | 装成官方余额 |

所有指标必须含单位和scope。v1默认Usage主数字是**今日已观察turns**，没有全量历史则显示PARTIAL，不显示“今日全部使用”。

## 2. 四维元数据

每个metric/quota都保留：

- `quality`：exact / estimated / manual / unavailable / simulated。
- `coverage`：complete / partial / since_bridge_start / unknown。
- `source`：固定短来源ID，例如codex_app_server、cursor_export、hermes_plugin、manual。
- `as_of_ms` + `stale_after_ms`：最后成功观测时间与展示过期阈值。

精确的本次API usage仍可能只覆盖部分今日活动，因此 `exact + partial` 完全合法。统计缺失不是0；official API报错不能用假值兜底。SIM与MANUAL在每个相关页面都可见。

## 3. 时间范围

Bridge配置IANA timezone；可从系统获得，无法可靠取得则明确使用UTC，不静默猜用户时区。示例配置Asia/Shanghai只是示例。

存储UTC毫秒。今日范围按所选时区的实际日界线计算，夏令时一天可能23/25小时；区间统一[start,end)。设备只显示今日/会话简写，详情显示时区与覆盖起点。

首次启动只收到从现在起的事件时coverage=since_bridge_start。导入更早数据后只有明确补齐覆盖才改complete。

## 4. 去重与累计

### 事件去重

唯一键 `(agent_id, source_instance, event_id)`；相同任务的不同Hook映射不能重复create run。上游通用tool Hook和Shell专用Hook二选一为主流；不可两条都累计。

### Usage快照

按 `(account_key, provider, model, counter_key, counter_epoch)` 建检查点。累计快照1000→1300产生增量300；1300再次到达增量0；旧seq1200迟到忽略。累计值变小不默认截成0继续，必须识别counter reset/correction或新epoch。

Bridge可保存native_usage_id的authoritative记录，用upsert替换旧记录，重算聚合，而不是给用户不断累加正差。两种来源（本地Hook和官方日统计）不能放同一个sum里；选择优先级并记录reconciliation。

### Token分类

保存input/output/cache_read/cache_write/reasoning等独立字段与其包含关系。总token只有provider明确总量，或已验证互斥分项才能相加。`context_tokens`永远不进入计费消耗累计。

### 成本

只存整数 `usd_micros` 等明确币种最小单位。estimated cost必须有price_source、版本、模型映射、估价时间；不得以同名模型猜不同服务商价格。v1允许无cost只显示token/turn。

## 5. 时间统计

`run_elapsed_ms`是代表run开始到当前的墙钟时长，可包含等待。`active_time_ms`是当日该Agent处于working/tool的时间区间并集；并发会话重叠不重复增加“我工作了多久”。需要资源累计时另定义sum_run_time，不用同一标签。

用户睡眠/休眠跨度由Host事件明确排除不可靠区间；失去观测期间标partial，不能拿系统时间差补成有效工作时间。

## 6. Quota模型

kind：rate_window / credits / spend_budget / unlimited / unknown。

- rate_window：使用实际window名称与resets_at。某些源只有usedPercent，没有token上限，应unit=percent、used/limit/remaining=null，仅used_pct有效。
- credits：unit=credit；知道remaining但不知道limit时不画百分比。
- spend_budget：官方spend/limit或用户local预算，scope必须分account/local_budget。
- unlimited：所有数值null，明确UNLIMITED，不能用9999999。
- unknown：数值null；availability解释unsupported/needs_auth/error。

`used_pct`定义为“已用百分比”，UI可显示`100-used_pct`的剩余，但必须label=LEFT，不能把68%已用显示成68%剩余。超额时环clamp100，文字显示超额，不产生负剩余。

只有同单位、同scope、同时间窗、同可用资源池，used/remaining/limit之间才可校验相等关系。买了新的积分包不能简单把旧percent补到100。

## 7. 多窗口、多池、共享账号

当前屏最多两条记录；默认显示最紧张的短/长窗口，或即将过期与常规积分池。其余完整数据保留在Mac。

account_key是加盐hash，只用于去重与共享提示。Codex和Hermes使用同一账户时展示SHARED，不能相加；WorkBuddy不同地区/账号不合并；Cursor团队pool只显示获授权的当前用户scope。

跨Agent全局余额/总token不是v1需求，刻意不提供，避免重复、单位和覆盖误导。

## 8. 刷新与陈旧

- 本地usage最多每1秒更新projection。
- quota通常60秒轮询；来源更慢时服从其限制。Cursor hourly API最多1小时一次。[S10]
- 手动refresh最短30秒且尊重provider更长min interval；请求入队时界面显示loading，不清除旧值。
- 到reset时间不自动重置数值，标RESET PENDING并请求新数据。
- 网络失败保留as_of与旧值；过stale_after标STALE。
- 401/403标SIGN IN/NO ACCESS，不无限尝试重新认证或读出浏览器Cookie。

## 9. 手工/导入格式

用户主动提供的quota导入必须带account_key、scope、unit、观测时间、有效期和quality=manual；官方导出可以quality=exact、source=authorized_export，但coverage视导出范围而定。

导入前dry-run显示数据条数、时间范围、去重数，不输出身份与敏感信息。N/A与manual都在v1支持范围内，但不能将manual称作自动额度同步。

## 10. 必测场景

0值/缺失/unlimited；两个window；100%以上；reset后缓存；部分历史；累计1300重复；counter归零；断线恢复；UTC与夏令时；共享account；同一任务双Hook；API失败重试；cost缺价格；上下文占用误入quota的拒绝。

## 11. 计数器首样本与时间窗

累计计数器的counter_key必须包含真实作用域标识（例如thread哈希），不能把不同thread都命名成同一个thread-total后共享水位。首次接入已经运行的会话时，若不知道counter起点与时间窗，把第一条累计值作为基线，delta=0、coverage=since_bridge_start；不能把历史总量归到今天。只有明确观测到窗口开始或获得同窗口权威记录，才可从0计算首样本增量。

UsageRecord新增 `window_start_ms/window_end_ms`：authoritative_window必须有合法时间窗；其他模式可null，但不能凭as_of把跨天累计量一次归进今日。重放用事件时间，导入用明确窗口；跨日但无法拆分的数据保持较粗粒度和partial。

QuotaRecord的 `shared_with` 由Mac按授权已知账号关联计算，设备据此显示SHARED；空数组表示没有已证实关联，不表示保证独占。不会暴露账号邮箱/登录ID。
