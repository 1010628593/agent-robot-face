# Cursor Adapter

## 已确认路径

官方本地Hooks通过JSON stdin/stdout工作，覆盖beforeSubmitPrompt、pre/postToolUse、stop等。Tab事件与Agent事件分开，cloud/本地支持也不同。[S09]

官方Admin API是团队管理能力，需相应权限；聚合usage与个人额度不是同一个概念。[S10] 不能把Cloud Agents管理API当作个人账单API。

## Probe

确认本机Cursor版本、入口（IDE Agent / CLI / Cloud）、已有hooks配置与信任；首版只承诺测试过的本机IDE入口。记录是否使用Cursor原生Agent，避免把内嵌Codex扩展事件重复归属。

## 状态映射

- beforeSubmitPrompt → run_started。
- preToolUse/postToolUse → tool_started/tool_finished；选择通用Hook作为主通路，不能再把before/afterShellExecution重复计为另一个工具。
- stop → 根据实际status产生done/error/cancelled；afterAgentResponse可能只是中间回复，不能提前done。
- sessionStart/sessionEnd管理会话，不每个sessionStart都新增turn。
- 未有明确permission等待事件时，waiting能力标partial；preToolUse不等于等待用户。
- preCompact中的context_tokens/context_usage_percent是上下文占用，不是每日消耗或套餐余额。

默认排除beforeTabFileRead/afterTabFileEdit，不把自动补全高频事件污染Agent状态。

## Usage

本地Hook先给出turns/tool_calls与运行时长，scope=observed-local、coverage依实际启动时间。请求/token只有真实使用字段才入账。授权导出数据作为独立来源，不和本地观察counter重复求和。

## Quota

1. 有合法Admin权限：读取官方账号/成员scope，过滤当前用户与相关账户。不得显示同事数据到设备。
2. 个人账号：用户主动导出的账单数据，或手工预算；未确认正式API时不得调用猜测的内部endpoint。
3. 没有额度数据：N/A，详情显示NO API；允许open_usage打开官方dashboard供用户核对。

聚合API遵从官方刷新频率，不能触摸一次就重新请求。若只提供spend与user-defined budget，标LOCAL BUDGET，不装成官方套餐余额。

## 交付

`adapters/cursor.py`、独立test/fixtures、capability report。测试普通回复/中间回复/并发工具/取消、重复的通用与专用Hook、缺少用量、上下文占用不混入quota。
