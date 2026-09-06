# 本地环境（T00 实测）

本文件记录本机真实环境，供后续所有任务复用。所有内容为实测输出，不是推测。

## 1. 激活方式

**不要新装 ESP-IDF，也不要换版本。** 每个新 shell 先激活 EIM 提供的 v6.1 环境：

```bash
. "$HOME/.espressif/tools/activate_idf_v6.1.sh"
```

（fish 用户用 `activate_idf_v6.1.fish`。脚本不存在就回 EIM 打开终端，不拼接其他版本路径。）

激活后：

| 变量 | 值 |
|---|---|
| `IDF_PATH` | `/Users/hanxin/.espressif/v6.1/esp-idf` |
| `IDF_PYTHON_ENV_PATH` | `/Users/hanxin/.espressif/tools/python/v6.1/venv` |
| `idf.py --version` | ESP-IDF v6.1 |
| IDF git | `fff9895c82d744c7237be8847347bdd1b07c6643` · tag `v6.1` |
| IDF Python | 3.14.7 |
| esptool | v5.4.0（esptool 命令用 `python -m esptool`，v5 连字符语法） |

## 2. 三条环境互不污染

| 用途 | 环境 | 说明 |
|---|---|---|
| 固件构建（idf.py） | EIM v6.1 Python venv | 不往这里装应用依赖；不升级 EIM 内部工具 |
| 文档契约校验 | 项目根 `.venv-doccheck/` | 仅 `jsonschema>=4.22,<5`；由 WorkBuddy managed Python 3.13 创建 |
| Mac Bridge（T03 起） | 独立 venv，Python 3.11+ | 与上面两个隔离，不使用 EIM 的 Python |

## 3. 目录约定

```text
/Users/hanxin/Workspace/llm/agent-robot-face/   # 项目根（Git 仓库）
├── Bot_Status_v1_Handoff/    # 规格文档包（只读参考，contracts/ 与 docs/ 为准）
├── vendor/waveshare-1.75/    # 微雪官方标准 1.75 BSP 仓库（非 C 版）
├── docs/LOCAL_ENVIRONMENT.md # 本文件
├── evidence/                 # 实测证据；evidence/private/ 已 gitignore
├── reports/implementation-log.md
├── .venv-doccheck/
└── .gitignore
```

原厂固件备份**不入库**：写往仓库外的 `$HOME/Workspace/bot-status-private-backups/<时间戳>/`。

## 4. 硬件连接现状与确认要求

**已识别（2026-09-06 实测，见 `evidence/hardware-baseline.md`）**

| 项 | 值 |
|---|---|
| 端口 | `/dev/cu.usbmodem2142101`（原生 USB CDC，插拔对比确认） |
| 芯片 | ESP32-S3 (QFN56) revision v0.2 |
| Flash | 16MB（eFuse: quad, 3.3V） |
| PSRAM | 8MB 内置 (AP_3v3) |
| USB 模式 | USB-Serial/JTAG |
| 安全 | Secure Boot 关 / Flash 加密 关 / 无读保护 |

`esptool` 调用统一用 `python -m esptool`（v5 连字符语法，来自 EIM 环境）。

> 注意：另一端口 `/dev/cu.usbserial-214220` 是 ASUS 外设，**不是**开发板；不要凭"第一个串口"匹配。
- 端口只做两件事可无确认执行：只读 `flash-id` 核对、只读 `read-flash` 备份。
- **需用户单独确认才可执行的硬件操作**：首次烧录、原厂固件恢复、任何 `erase-flash`、写 eFuse、启用 Secure Boot/Flash Encryption。

## 5. 外部依赖（T02 实测）

| 依赖 | 来源 | 说明 |
|---|---|---|
| BSP `waveshare/esp32_s3_touch_amoled_1_75` | IDF Component Manager，manifest 声明 `"*"` | 浮动版本，构建后须记录 `dependencies.lock` 锁定结果 |
| `lvgl/lvgl` | IDF Component Manager，manifest 声明 `"9.4.*"` | 同上 |
| 官方仓库 HEAD | `e4344e70c2fa78a13e8a06566507f1ba8af6672a`（2026-08-21，"Refresh ESP-IDF v5.5 compatibility matrix"） | 官方声明验证到 5.5/6.0.x，v6.1 需本机编译验证 |

示例 `sdkconfig.defaults` 由 IDF 5.4.0 生成，因此 v6.1 构建属于"官方未验证组合"，首个真实编译错误必须如实记录（见 `evidence/idf61-build.md`）。
