# 任务状态（副本，本地 Agent 更新用）

> 源表：`Bot_Status_v1_Handoff/IMPLEMENTATION_STATUS.md`。此处为副本，避免在规格包内改动导致 `MANIFEST.sha256` 失配。

| 任务 | 内容 | 状态 | 证据 |
|---|---|---|---|
| T00 | 只读盘点与工程基线 | **DONE**（硬件识别子项 BLOCKED_HARDWARE） | `evidence/baseline.md` |
| T01 | 四个 Agent 能力探测 | PARTIAL（仅只读版本探测） | `reports/implementation-log.md` T01 预探测 |
| T02 | 原厂备份和 v6.1 官方样例 | **PARTIAL**：构建 DONE + 硬件识别/备份 DONE；**烧录待用户确认** | `evidence/idf61-build.md`、`evidence/hardware-baseline.md` |
| T03 | 合同模型、构建边界与测试地基 | NOT_STARTED | — |
| T04 | 独立手势识别 | NOT_STARTED | — |
| T05 | 设备帧解码、Model 与路由 | NOT_STARTED | — |
| T06 | Face 三层动画与 LVGL 模拟器 | NOT_STARTED | — |
| T07 | Agent Picker 与选择确认 UI | NOT_STARTED | — |
| T08 | Usage/Quota 界面与格式化 | NOT_STARTED | — |
| T09 | SessionStore 与账本基础 | NOT_STARTED | — |
| T10 | USB 端到端握手 | NOT_STARTED | — |
| T11 | Loopback 入口和安全 Hook emitter | NOT_STARTED | — |
| T12 | Host 选择事务与动作白名单 | NOT_STARTED | — |
| T13 | Usage/Quota 统计引擎 | NOT_STARTED | — |
| T14 | 三屏 SIM 联调检查点 | NOT_STARTED | — |
| T15 | Codex 真实适配 | NOT_STARTED | — |
| T16 | Cursor 真实适配 | NOT_STARTED | — |
| T17 | Hermes 真实适配 | NOT_STARTED | — |
| T18 | WorkBuddy 桌面适配与显式能力门 | NOT_STARTED | — |
| T19 | 通知、亮度、睡眠与防烧屏 | NOT_STARTED | — |
| T20 | 安全与故障注入 | NOT_STARTED | — |
| T21 | 安装、doctor 与恢复 | NOT_STARTED | — |
| T22 | HIL、全链路验收与发布标记 | NOT_STARTED | — |

## 检查点

- [x] T00：环境盘点（硬件未接入，阻塞项已标注）
- [ ] T02：备份 + 官方样例上板 —— **等用户接线 + 单独确认烧录**
- [ ] T14：三屏 SIM 板上预览，用户确认触控和观感
- [ ] T15–T18：四源真实能力报告，WorkBuddy 降级须明确接受
- [ ] T22：长稳/隐私/统计同口径对照与发布说明
