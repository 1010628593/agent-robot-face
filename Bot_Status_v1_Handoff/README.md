# Bot Status v1 · 本地 Agent 实施文档包

**设备：Waveshare ESP32-S3-Touch-AMOLED-1.75-B（黑色带壳）**  
**开发环境：Apple Silicon macOS · EIM 已安装的 ESP-IDF v6.1 · LVGL 9**  
**文档版本：1.0.0 · 编制/公开资料核查：2026-09-06**

> 本包是实施规格、接口契约、测试样本及交接材料，不是已完成的固件或 Mac 应用。附带的校验工具只验证文档合同与示例；ESP-IDF 编译、板上显示、触摸和四个工具的本机接入，必须由实施者真实验证。

## 一句话目标

在 466×466 圆形 AMOLED 上实现一个有生命感的桌面 Agent 伙伴：**状态驱动的脸、长按选择 Agent、使用量与额度页**。Mac 汇聚 Codex / WorkBuddy / Cursor / Hermes；开发板只做本地动画、展示和低风险交互。

## 固定决策

- 项目独立于 Spring AI Alibaba 企业平台；不做机器人、语音、摄像头、任务执行或自动批准。
- 保留三个逻辑页面：`FACE`、`AGENT_PICKER`、`STATS`；统计内为 `usage/quota` 两个子视图，不增加导航层级。
- 主屏横滑进入统计；统计纵滑切换 usage/quota、右滑返回；长按 650ms 进入 Agent 选择。详见交互规范，禁止各模块自行重新定义。
- 切换 Agent = 切换**观察对象**，不是终止、启动任务，也不是切换账号或模型。
- 固件 C + ESP-IDF **v6.1** + 官方标准 1.75 BSP。不得偷偷降级，也不得刷 1.75C 固件。
- Mac Bridge 使用独立 Python 3.11+ 环境、asyncio、SQLite、pyserial。首版 USB；Wi-Fi 不作为 v1 发布前置条件。
- 状态与统计分通道；保活不重启动画，不重复累计 usage。没有额度来源就显示 `N/A`，不是 0，也不是 100%。
- 四个 Agent 都有页面/适配器/能力报告；实际自动观测覆盖必须单独验收。WorkBuddy 桌面 Hook 能力尚须本机探测，不能拿 CodeBuddy CLI 的文档直接代替。

## 阅读顺序

1. [交给本地 Agent 的启动提示](HANDOFF_PROMPT.md)
2. [项目约束 AGENTS.md](AGENTS.md)
3. [产品需求 PRD](docs/01_PRD.md)
4. [三屏 UX 和视觉规格](docs/02_UI_UX.md)
5. [状态、会话和选择规则](docs/03_STATE_MODEL.md)
6. [架构](docs/04_ARCHITECTURE.md) → [协议](docs/05_PROTOCOL.md)
7. [固件设计](docs/06_FIRMWARE.md) → [Mac Bridge](docs/07_MAC_BRIDGE.md)
8. [四个 Agent 的接入总览](docs/08_ADAPTERS.md) → `docs/adapters/`
9. [统计口径](docs/09_USAGE_QUOTA.md)
10. [按任务实施](docs/10_IMPLEMENTATION_PLAN.md) → [验收](docs/11_TEST_ACCEPTANCE.md)
11. [macOS 首日操作](START_HERE_macOS.md)、[故障与恢复](docs/12_OPERATIONS.md)、[安全](docs/13_SECURITY.md)

[跨模块接口签名](docs/14_INTERFACES.md) · [决策与风险](docs/DECISIONS_AND_RISKS.md)

附带：`contracts/` JSON Schema、协议样例与反例；`design/` 视觉/手势 token；`acceptance/` 状态和手势测试向量；`tools/validate_contracts.py` 文档合同校验。

## 文档优先级

`AGENTS.md` 的安全约束 > `docs/01_PRD.md` 范围 > `docs/05_PROTOCOL.md` + JSON Schema 契约 > 其他技术文档 > 任务步骤。发现冲突先记录并修正文档，不擅自猜测。实际供应商 BSP 是硬件引脚/电源初始化的依据。

旧文件 `Bot_Status_macOS_完整实施方案.md` 中的 v5.5.5、单 Codex、单屏范围被本包替代；不得将旧包混入本项目的最新规范。

## 本地校验文档包

```bash
python3 -m venv .venv-doccheck
.venv-doccheck/bin/python -m pip install 'jsonschema>=4.22,<5'
.venv-doccheck/bin/python tools/validate_contracts.py
```

这个校验不连接开发板、不修改 Agent 配置、不访问账号，也不证明真实设备可运行。

## 第一个实施检查点

完成 T00–T02：只读盘点 → 私有原厂备份 → v6.1 编译官方 LVGL 9 示例 → 人工确认后上板。不得一开始同时接四个 Agent 和统计接口。

## 可发布边界

- `v1-ui-preview`：三屏、手势、模拟数据和异常态完成，但不称真实集成完成。
- `v1-integrated`：四个适配器分别有真实证据或用户明确接受的降级模式；至少在一条真实链路上验证 usage 与 quota。任何缺项都写在 release notes，不能用模拟数据冒充。

外部事实的来源编号统一见 [SOURCES](docs/SOURCES.md)。未有外部来源的阈值、布局、协议字段均为本项目设计决策，不是厂商保证。
