# 公开资料与证据索引

核查日期：2026-09-06。全部软件接口仍须按**本机版本**复验；网页出现接口不代表用户当前安装版支持，也不代表当前账号有权限。下列只用于硬件/接口事实；本包的手势、配色、协议、性能阈值是自行设计，不是厂商承诺。

| 编号 | 一手资料 | 用于确认 | 不足以证明 |
|---|---|---|---|
| S01 | [微雪1.75/1.75-B文档](https://docs.waveshare.com/ESP32-S3-Touch-AMOLED-1.75) | 板系列、显示/触摸/USB等资源 | 用户实物PCB批次、固件已经跑通 |
| S02 | [微雪仓库getting-started](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75/blob/main/docs/getting-started.md) | 02_lvgl_demo_v9、16MB布局、公开验证分支 | v6.1自动兼容 |
| S03 | [LVGL示例依赖](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75/blob/main/examples/esp-idf/02_lvgl_demo_v9/main/idf_component.yml) / [入口代码](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75/blob/main/examples/esp-idf/02_lvgl_demo_v9/main/main.c) | 官方BSP、LVGL9.4版本线、BSP锁使用 | 通配依赖永久不变 |
| S04 | [ESP-IDF6.1 ESP32-S3入门](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32s3/get-started/index.html) | 使用既有EIM/6.1工具链 | 第三方BSP已经验证 |
| S05 | [ESP32-S3 USB Serial/JTAG](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-guides/usb-serial-jtag-console.html) | 原生双向串口与睡眠边界 | 任意USB数据线都支持数据 |
| S06 | [LVGL9.4线程说明](https://lvgl.io/docs/open/9.4/details/integration/overview/threading) | GUI线程安全/锁要求 | 可以从任意任务直接画UI |
| S07 | [Codex Hooks](https://developers.openai.com/codex/hooks) | 生命周期事件、配置和trust机制 | 所有本地Codex入口自动覆盖 |
| S08 | [Codex App Server](https://developers.openai.com/codex/app-server) | 状态通知、只读账号/usage/rate-limit能力 | 新开实例能观察现有App；ChatGPT额度等于API余额 |
| S09 | [Cursor Hooks](https://cursor.com/docs/hooks) | IDE Agent生命周期、工具/结束事件、上下文字段 | 所有版本可观测权限等待；上下文=消耗 |
| S10 | [Cursor Admin API](https://docs.cursor.com/en/account/teams/admin-api) | 受授权团队管理员的用量接口和聚合粒度 | 个人账号天然有Admin key或实时quota |
| S11 | [腾讯WorkBuddy文档](https://www.workbuddy.ai/docs/workbuddy/) / [中文入口](https://www.workbuddy.ai/docs/zh/) | 产品身份、桌面产品范围 | 这是用户自建平台或CodeBuddy CLI |
| S12 | [WorkBuddy MCP指南](https://www.workbuddy.ai/docs/zh/workbuddy/From-Beginner-to-Expert-Guide/Function-Description/MCP-Guide) | 桌面MCP接入路径 | MCP自报能完整自动观察生命周期 |
| S13 | [WorkBuddy价格与积分](https://www.workbuddy.ai/docs/zh/workbuddy/pricing) | 配额/积分池概念和官方显示入口 | 稳定公开余额读取API、固定不变的价格 |
| S14 | [Hermes Event Hooks](https://hermes-agent.nousresearch.com/docs/user-guide/features/hooks/) | Gateway、Plugin等入口区别；工具/审批/usage钩子 | Gateway Hook适用于CLI；存在统一Hermes钱包 |
| S15 | [esptool Basic Commands](https://docs.espressif.com/projects/esptool/en/latest/esp32s3/esptool/basic-commands.html) | 只读识别、整片备份、写入命令规则 | 已验证某份原厂备份可恢复 |

## 核查结果与本机验证门

**官方确认的硬件起点**：标准1.75系列BSP用于本设备；C版工程不混刷。公开示例依赖可锁定，但用户本机尚需编译验证。原厂备份属于私有数据，不随本包传递。

**明确的API依据**：Codex与Cursor有官方状态接口资料；Hermes有CLI/Gateway入口不同的扩展机制。使用前仍需真实事件样本、版本、授权和去重验证。

**未确认的能力**：当前查阅资料未确证腾讯WorkBuddy Desktop覆盖完整生命周期的稳定公开Observer API，以及适用于用户账户的自动余额端点。因此实施任务使用能力探测门，不写虚构命令。WorkBuddy/CodeBuddy网站部分页面可能互相跳转，核对产品标题和适用入口后才使用。

**本包不会交付账号数据**：所有JSON示例是synthetic，所有四产品能力报告初始not_probed。没有证据不能改写成ready。

## 如何固定实现时的来源

T00–T02记录：source_url、访问日期、仓库commit SHA、安装产品版本、目标入口、当前账号权限类型（不含密钥）、脱敏证据路径。复制到项目的第三方源码保留许可证与版权声明；本包不分发任何字体文件。

发生官方文档变化时更新此页与对应Adapter，而不是在设备固件里补特殊字符串规则。
