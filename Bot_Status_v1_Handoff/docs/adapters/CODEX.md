# Codex Adapter

## 公开依据与边界

官方Hooks列出SessionStart、UserPromptSubmit、PreToolUse、PermissionRequest、PostToolUse、Stop、Interrupt等，配置可在用户/项目的hooks.json或config层；新Hook可能需要用户信任。官方也提醒transcript格式不是稳定接口。[S07]

App Server提供状态通知、`thread/tokenUsage/updated`、`account/rateLimits/read`及账号用量相关方法。[S08] **这些能力以连接的server实例/认证上下文为边界，不代表启动第二个进程就能窥见现有桌面会话。**

## 必做probe

记录 `codex --version`、`codex --help` 的受限输出，确认CLI是否安装、用户实际入口、是否允许Hook、App Server实际schema。不得扫描/复制auth.json的token；额度读取优先让官方本地服务自行管理认证。

先验证Hooks能观察用户真正使用的入口。若只覆盖CLI，明确标CLI-only，不把桌面App事件显示为观察到。

## 状态映射

| 官方事件 | Canonical |
|---|---|
| SessionStart | session_opened |
| UserPromptSubmit | run_started |
| PreToolUse | tool_started，不等于等待批准 |
| PermissionRequest | waiting_started(reason=approval) |
| PostToolUse | tool_finished；如果此前存在该工具的等待，再清waiting |
| Stop | 依据结局字段run_finished，不推断测试通过 |
| Interrupt | run_finished(reason=cancelled) |
| SessionEnd | session_closed，不把进程关闭当成功 |

字段和大小写依据实际schema验证后才生成Hook配置；不用未证实的固定JSON路径。stdout不输出debug文字，不返回continue/block/context字段来影响原任务。

## 用量与额度

优先通过当前入口可取得的结构化usage记录；只有tokenUsage累计数时，必须按同thread/model累计差分而非相加，保留baseline与partial coverage。Hooks仅能计数turn/tool时就只显示这些，不把一次tool当一次计费request。

账号查询只允许read类方法：`account/read`、当前支持的`account/rateLimits/read`、`account/usage/read`；方法不存在返回unsupported。完整schema由本机官方工具生成/文档核对，不在设备端拼接RPC。

窗口primary/secondary各成quota记录；使用实际windowDuration/resetsAt，不能硬编码5小时/一周。额度是账户级，不是当前会话专属。API-key模式与ChatGPT套餐不同，没有ChatGPT额度就N/A；禁止估计成“剩余token”。

禁止调用登录/登出、消费reset credit、发送额度邮件等写方法；Bridge只显示，不影响账号。

## 实施文件与证据

`bridge/src/bot_bridge/adapters/codex.py`；quota单独服务；`bridge/tests/adapters/test_codex.py`；`reports/capabilities/codex.json`；脱敏fixture包括并发工具、取消、账户未登录、窗口缺失和重复usage。

成功标准：真实入口至少一轮完整状态；一条明确waiting证据或明确标不可用；quota两窗口/缺失字段按实际数据渲染；拔掉板不会改变Codex运行。
