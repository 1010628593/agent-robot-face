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

## 用户决策（2026-09-06）

**选项 B**：WorkBuddy 维持 `blocked`，不接受 MCP reported 降级。

- T18 不写 WorkBuddy adapter；三屏里 WorkBuddy 行显示 N/A（BLOCKED_SOURCE）
- v1 发布条件之一（WorkBuddy 真实接入）标记为**已知不满足**，由用户在发布说明中确认
- 其他三源（Codex / Cursor / Hermes）按 T15/T16/T17 正常推进
- 若未来 WorkBuddy 官方公开稳定 hook/MCP 生命周期接口，T18 可重新启用，capability-report 需重新 probe

## 下一步（按依赖排序）

1. T03 合同模型与测试地基
2. T04 手势 → T05 帧解码/路由 → T06–T08 三屏 + LVGL 模拟器
3. T14 三屏 SIM 检查点
4. T15/T16/T17 三源真实 adapter 干跑（T18 跳过）
