# 08 · 四个 Agent 接入总览与能力门禁

## 1. 产品身份

- **Codex**：OpenAI Codex，CLI/IDE/桌面入口分别检测。
- **WorkBuddy**：腾讯 WorkBuddy 桌面工作台。不是“用户自建平台”，也不是另一家 WorksBuddy。
- **Cursor**：Cursor本机IDE内的原生Agent，排除Tab自动补全和跑在Cursor里的Codex扩展。
- **Hermes**：Nous Research Hermes Agent；区分CLI与Gateway。

以上工作与用户企业Spring AI平台分离。公开资料核查日期2026-09-06，具体本机覆盖以probe/fixture为准。

## 2. 能力矩阵（公开路径，不是实机已通过）

| Agent | 状态首选 | 使用量首选 | 额度首选 | 必須验收的风险 |
|---|---|---|---|---|
| Codex | 官方Hooks；必要时已连接的App Server事件 | 支持入口的tokenUsage / account usage；否则本地turn/tool | account/rateLimits/read（ChatGPT模式） | 单独启动app-server不自动监听另一桌面进程 |
| WorkBuddy | Desktop可用Hook先探测；只读本地结构化事件或MCP自报降级 | 官方授权导出/实际暴露字段；本地观察计数 | 官方接口/导出/人工值 | CodeBuddy CLI与WorkBuddy Desktop不可直接等同 |
| Cursor | 官方本地Hooks | 可观察turn/tool；授权计费导出或Admin API | 有权限的官方Admin/usage数据；否则N/A/人工预算 | 个人账号不能假定有团队Admin API |
| Hermes | CLI可用plugin/shell观察Hook | post_api_request.usage等实际版本字段 | 对应模型provider的官方只读接口 | Gateway-only hook不能覆盖CLI；token不是余额 |

Codex/Cursor/Hermes文档有明确扩展面；WorkBuddy官方桌面文档提供MCP路径，但当前未确认稳定的**桌面全生命周期观测+个人额度API**。[S07][S08][S09][S10][S11][S12][S13][S14]

## 3. Probe输出

为每产品生成 `reports/capabilities/<id>.json`，使用本包capability schema。至少记录：

```text
product, installed_version, entry_point, observation_mode,
state_events, usage_fields, quota_source, last_verified_at,
evidence_files, limitations, auth_required, status
```

status：not_probed/ready/partial/blocked/not_installed。不能在没有本机fixture时写ready。probe不能读出凭据，只确认客户端登录状态/权限是否足够。

## 4. 接入阶段

1. Dry probe：只读版本、公开能力、现有配置位置，输出将安装内容。
2. 授权后增加旁路Hook/plugin；事件白名单，不改现有permission策略。
3. 用用户允许的临时任务采集“提交→工具→等待/取消→结束”的脱敏样本。
4. 将样本转canonical fixtures并写Adapter tests；和UI投影核对。
5. 加usage再加quota；不要把不同产品的quota当共同单位。
6. 每产品出一页结果，明确哪些来自本地观察、哪些来自官方账号、哪些不可用。

## 5. 降级策略

降级顺序：正式接口 → 已授权只读结构化本地记录 → 用户授权导出 → 人工输入/可选MCP自报 → N/A。禁止静默抓浏览器Cookie、拷贝登录token、截屏OCR或网络MITM作为默认实现。

MCP自报不是可靠监控：Agent可能不调用、重复或忘记发结束，必须quality=reported，显示较短过期时间。它可提供WorkBuddy最小联动，但不能宣称自动捕获所有状态。无生命周期源时等待/错误不可猜。

## 6. 发布条件

v1的范围包含四源，但受外部接口约束：每源至少完成probe、adapter、异常/缺失显示和真实证据报告。若某源仅manual/reported，发布前必须让用户接受该能力降级；否则该项保持BLOCKED_SOURCE，不得用SIM取代。

四者都具备usage/quota界面；不意味着四者都有官方可自动读取的额度。配额不可得属于支持的UI状态，不是省略统计屏。

详细方案见 `adapters/CODEX.md`、`WORKBUDDY.md`、`CURSOR.md`、`HERMES.md`。
