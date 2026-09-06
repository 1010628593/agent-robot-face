# 11 · 测试矩阵、实机验收与发布标准

**本文件是待执行测试计划。文档包内VERIFY报告只覆盖JSON/链接/合同，不覆盖开发板、macOS应用或真实账号。**

## 1. 四层验证

1. **合同层**：JSON Schema、语义约束、合法/非法样例，随文档可运行。
2. **纯逻辑层**：Mac原生C测试gesture/router/model/parser；Python reducer/ledger/adapter黄金样例；冻结时钟，不依赖实物。
3. **渲染层**：同一LVGL视图在SDL模拟器与466×466板上运行；网页草图不等于固件验证。
4. **实机与真实源层**：USB、重连、功耗/熄屏、真实Agent操作、实际账户同scope统计对照。

## 2. 需求追踪

| 需求 | 实现任务 | 核心验收 |
|---|---|---|
| R01 板型/IDF基线 | T00/T02 | 官方样例实机、备份/版本锁 |
| R02 Face表情 | T06/T09/T15–T18 | 状态有事实依据且动画可辨 |
| R03 手势 | T04/T05/T19 | 长按/滑动/唤醒不冲突 |
| R04 四源Picker | T07 | 四卡可预览，离线状态可见 |
| R05 选择确认 | T07/T12 | ACK/rev/超时/幂等 |
| R06 Usage/Quota | T08/T13 | N/A、估算、手填、过期、覆盖 |
| R07 USB | T05/T10 | 握手、日志分流、重连、坏包 |
| R08 四Adapter | T01/T15–T18 | 各源证据，WorkBuddy能力门 |
| R09 账本去重 | T09/T13 | 重复/历史/累计/共享scope |
| R10 跨源提醒 | T19 | Badge/Toast不抢当前观察对象 |
| R11 AMOLED/断连 | T19/T22 | 调暗、移位、关屏、首触只唤醒 |
| R12 隐私只读 | T11/T20/T21 | 无自动审批/凭证外发/配置覆盖 |
| R13 可复现 | T03/T21/T22 | 锁文件、证据、恢复、发布说明 |

具体需求编号以PRD表为准；若修改范围，同时更新追踪表，不只改release notes。

## 3. 手势测试

使用 `acceptance/gesture_cases.json`。单位px与ms，起点/路径都在466画布内。

| 用例 | 应有表现 |
|---|---|
| 轻点Face中心 | poke+2s详情，不切Agent |
| 按住649ms | 不进入Picker |
| 按住650ms | 进入Picker，松手无额外选中 |
| 650ms前移动13px | 取消长按，不误进Picker |
| Face横划 | 到Stats，记住子页 |
| Stats纵划 | usage/quota切换；不退出页面 |
| Stats右划/左划 | 右返回Face，左边界反馈 |
| Picker左右/下划 | 预览前后Agent；下取消 |
| dim/off后整次接触 | 只唤醒，不poke/选择 |
| 用户长按时业务更新 | 手势继续，背景更新不吞触摸 |
| 触摸失去UP或被系统cancel | 状态清理，不生成TAP |

人工每种常规手势20次、长按20次，记录成功/误触次数。目标正常操作成功率≥95%，未完成长按不能切Agent；失败保留视频定位，而非调整门槛掩盖驱动坐标问题。

## 4. 表情与业务状态

使用 `acceptance/state_cases.json`。必须覆盖并发工具集合、等待恢复、工具失败后继续、取消非错误、旧run结束、重复消息、未知源、新连接、done短动画结束。

截图要求：8业务状态、offline、reported、SIM；Face顶栏Agent和下方状态不矛盾。工具持续10ms可以合并，不要求每个状态都肉眼可见；等待与终止必须可靠到达最终视图。

## 5. 选择事务

测试accepted、conflict、timeout、同action重发、断线、切换后旧stats晚到。新Agent名称出现时其脸和stats不能仍是前Agent的内容。ACK已提交而新focus暂未到时，保留Loading，不先展示假idle。

设备选择不启动/停止Agent、不切账号、不修改模型。Switch界面的断线Agent可以被选择作为观察对象，但没有Host连接时不能提交任何选择。

## 6. 统计与额度

使用 `acceptance/usage_cases.json`。逐项对比0与null、累计差分、重复usage事件、counter reset、authoritative窗口修订、缓存token包含关系、active时间并集、跨日时区、共享额度池。

对真实账号使用相同时间范围、账号、入口和刷新时刻核对。UI `exact`只说明该字段来自明确数值，不意味着全天历史完整；coverage必须同时可见。无授权CursorAdmin/WorkBuddy额度时，N/A是合格真实性行为，**不是四源全功能自动接入合格**。

手工额度标MANUAL，估值标~，部分范围标PARTIAL/SINCE START，过期标STALE。重置时间到达不自行清零；unlimited不画100%。

## 7. 性能目标（必须实测，不当规格保证）

| 指标 | 目标和测法 |
|---|---|
| 稳态动画 | 约30fps，p95帧间隔≤50ms；按50%与28%亮度分别记录 |
| 状态开始响应 | 收到合法USB消息到首次UI变化≤200ms；独立记录源到设备延迟 |
| 手势反馈 | 触点到视觉反馈≤100ms，长按以单调时钟650ms为准 |
| Heap | 1000次切换后相对稳定基线无持续下降；记录min/最大块 |
| UI对象 | Face≤80、Picker≤140、Stats≤180为预算，确需增加须记录测量依据 |
| 启动 | 上电至本地Face目标≤3s，握手时间单列 |
| Bridge idle | 记录CPU/RSS，不让debug日志占用持续高CPU |
| Hook影响 | 200ms截止策略；原Agent继续执行，即使Bridge离线 |

性能不达标优先减少阴影/全屏invalidate/对象重建，保持状态语义和字体可读，不默认降低动画到“偶尔刷新”。

## 8. 故障注入清单

- USB无设备、占用、拔出、重新枚举、板重启、Host重启、旧link重复包。
- 8193字节、错误UTF-8、NaN、重复key、13层、任意额外字段、未知protocol version。
- Cloud quota 401/403/429/5xx/网络超时；仅该数据过期，不影响实时脸。
- Host磁盘满、spool容量满、SQLite锁/迁移失败；明示partial/诊断，不丢用户文件。
- 两个Bridge争用同一设备：后者退出并解释，不并发写。
- MAC睡眠/恢复：旧任务unknown/resync，断开关屏，不补播离线期间的旧done。

## 9. HIL连续运行

先30分钟冒烟，再8小时常用场景：50%时间idle、30%working/tool、10%Stats/Picker交互、10%等待/完成/重连。循环1000次状态切换不等于真实8小时，两个测试分别记录。

记录：板ID/PCB、固件SHA、IDF/BSP/LVGL版本、主机版本、全部源版本、输入数据来源、温升异常、USB重枚举次数、panic/reset原因、free/min heap、p95帧时、UI照片。

## 10. 发布门

`v1-ui-preview`只需合同/纯逻辑/三屏模拟及板上显示通过，SIM清楚，不声称真实四源。

`v1-integrated`需要：
- 四个profile、选择和统计页完整；Codex/Cursor/Hermes真实状态至少达到已确认能力。
- WorkBuddy满足A自动观测，或用户明确接受B=reported/limited；否则标BLOCKED_SOURCE，不无条件发完成结论。
- 至少一条真实usage链和一条官方quota链；无法取得授权时可以发`partial-integration`，不能用mock补验收。
- 其余数据无能力时明确N/A；每一处降级写进release notes。
- 安全/硬件/8h测试全部通过；无秘密进入日志或设备。

## 11. 结果文件

`evidence/acceptance/results.json`每条至少包含 `test_id,status(PASS|FAIL|NOT_RUN|BLOCKED),scope,measured,expected,evidence_file,verified_at`。没跑的就写NOT_RUN，不能预生成PASS。

报告结尾固定列：已通过、未通过、未测、用户接受的降级、已知限制、回退办法。不要以“整体基本完成”替代这些事实。
