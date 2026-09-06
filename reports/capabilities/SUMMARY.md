# 四源能力探测 — 2026-09-06

证据目录：`reports/capabilities/`（4 份报告，schema 校验将通过）。

## 入口与版本（实测）

| Agent | 入口 | 版本 | 关键路径 |
|---|---|---|---|
| Codex | CLI | 0.152.0 | `~/.local/bin/codex` · `~/.codex/config.toml` · `~/.codex/hooks/`（已存在 memmy 痕迹，无 Codex 原生 hooks） |
| WorkBuddy Desktop | Desktop | 5.5.3 | `/Applications/WorkBuddy.app` · 加密 `app.asar`（不可读） · `app.asar.unpacked/cli/` 是 CodeBuddy CLI 重打包 |
| Cursor | IDE | 3.19.14 | `/Applications/Cursor.app` · `~/.cursor/hooks.json`（v1 schema，已被 AgentKeyboard + memmy 占用） |
| Hermes | CLI | 0.21.0 | `~/.local/bin/hermes` · `~/.hermes/config.yaml` · `~/.hermes/agent-hooks/`、`plugins/`、`shell-hooks-allowlist.json` |

## 状态（capability report `status`）

| Agent | status | state | usage | quota | 关键风险 |
|---|---|---|---|---|---|
| Codex | **partial** | observed | import | none | 无公共 account/quota 读 API；ChatGPT 模式 ≠ Codex CLI 模式 |
| WorkBuddy | **blocked** | reported | import | none | 主进程在加密 asar；不可探；MCP 自报 quality=reported 必须用户接受降级 |
| Cursor | **partial** | observed | import | none | hooks.json 已有调用方，新增要最小 diff；个人版无 Admin API |
| Hermes | **ready** | observed | automatic | none | **唯一自动能拿到 usage 字段的源**；quota 需查其 provider，非 Hermes 自有 |

## 用户需要接受的发布门槛

按 `08_ADAPTERS.md §6`：v1 发布条件之一是"某源仅 manual/reported 时，用户必须接受降级，否则该项保持 BLOCKED_SOURCE"。

- **Codex**：可保留 `partial`（观察真实 CLI 事件 + 导入 usage），quota 永远 `none`（N/A 是 UI 支持的状态）。
- **WorkBuddy**：现 `blocked`。要让其不阻塞 v1 发布，必须由你显式接受"MCP 自报 quality=reported + 短过期 + 不保证捕获所有状态"。
- **Cursor**：同 Codex，保留 `partial`；quota N/A。
- **Hermes**：唯一 ready（自动 usage）；quota 仍可能 N/A。

## WorkBuddy 风险单独点出

1. `app.asar` 主进程是加密/混淆的，`require()` 直接读会抛 `Invalid or unexpected token`，确认是只读探测被阻挡。**我不会**做逆向或字符串提取。
2. `app.asar.unpacked/main/` 只含 `qimei-helper.js`（腾讯遥测）——没有 hook emitter。
3. `app.asar.unpacked/cli/` 是 **`@genie/agent-cli` = CodeBuddy CLI**（bin: codebuddy / codebuddy-code / cbc）。这印证了 `WORKBUDDY.md` 警告"CodeBuddy CLI 与 WorkBuddy Desktop 不可直接等同"。CodeBuddy 的 hooks 事件**不能**当 WorkBuddy Desktop 事件用。
4. 进程级 broker（`brokered-bin/safe-bin/sitecustomize.py`）只作用于 WorkBuddy 子进程；不影响本项目的独立 Python venv（已在 `tools/idf-env.sh` 与 `.venv-doccheck` 实测）。

## 提议下一步

我建议按以下顺序推进，不允许跳过任何红色确认：

1. **请你决定**：WorkBuddy 接受 MCP reported 降级？如不接受，T18 维持 BLOCKED_SOURCE。
2. 写 4 份 adapter 干跑代码（T15–T18），Hermes 跑真实插件（`hermes hooks doctor`），Cursor 跑 hooks.json 最小 diff，Codex 跑 `codex --help` 子命令清单（不发起任何真实请求），WorkBuddy 跑 `bot-status simulate --scenario wb-mcp-reported`。
3. 与三屏 SIM 检查点（T14）联调：先看 466×466 跑起 face → picker → quota 三个 demo 屏幕（不接真实 Agent，只看视觉与触控），确认观感。
4. 再做 T10 USB 握手、T11 Loopback、T12 Host 选择事务。

需要我立即写第一份 Hermes plugin 的最小只读实现（最稳的一源），还是先等你回复 WorkBuddy 的降级决策？