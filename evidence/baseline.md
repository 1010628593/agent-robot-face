# T00 · 只读盘点与工程基线

**采集时间**：2026-09-06 (Asia/Shanghai)
**采集方式**：EIM 激活脚本 `$HOME/.espressif/tools/activate_idf_v6.1.sh` 提供的 shell
**结论等级**：`文档校验通过`（本文件所有条目均为本机实测输出，未连接开发板，未做任何硬件写入）

## 1. 主机与工具链

| 项 | 实测值 |
|---|---|
| 架构 | `arm64`（Apple Silicon） |
| 系统 | macOS ProductVersion 27.0 / BuildVersion 26A5425a |
| `idf.py --version` | **ESP-IDF v6.1** |
| `IDF_PATH` | `/Users/hanxin/.espressif/v6.1/esp-idf` |
| `IDF_PYTHON_ENV_PATH` | `/Users/hanxin/.espressif/tools/python/v6.1/venv` |
| IDF Python | `/Users/hanxin/.espressif/tools/python/v6.1/venv/bin/python` · 3.14.7 (Clang 21.0.0) |
| IDF git HEAD | `fff9895c82d744c7237be8847347bdd1b07c6643` · `git describe`: `v6.1` |
| esptool | `esptool v5.4.0`（来自 EIM 的 IDF Python 环境，未另装、未升级） |
| EIM 工具目录 | `$HOME/.espressif/tools/`（含 xtensa-esp-elf、openocd-esp32、cmake、ninja 等） |

**判定**：v6.1 来自当前 EIM 安装，符合要求（不降级、不迁移 Arduino/PlatformIO、不重复安装）。

## 2. 工作区

| 项 | 状态 |
|---|---|
| 项目根 | `/Users/hanxin/Workspace/llm/agent-robot-face`（Git 仓库本任务初始化，此前无仓库） |
| 文档包 | `Bot_Status_v1_Handoff/`（76 个文件：28 md / 44 json / sha256 / toml / txt） |
| `docs/10_IMPLEMENTATION_PLAN.md` 建议的 `$HOME/Workspace/bot-status` | **不存在**，无既有项目冲突 |
| vendor | `vendor/waveshare-1.75`（T02 克隆，标准 1.75 仓库，非 C 版） |
| 既有固件/Bridge 代码 | 无（本次为全新实现，无覆盖风险） |

代码放在当前工作区而非 `~/Workspace/bot-status`：用户明确要求"按当前目录文档包实施"。若后续要迁移到 `bot-status`，vendor 与构建产物可整体移动，不损失证据文件。

## 3. USB / 开发板识别

`ls /dev/cu.*` 结果：

```text
/dev/cu.Bluetooth-Incoming-Port
/dev/cu.debug-console
/dev/cu.XiaomiBuds5Pro
/dev/cu.usbserial-214220
```

对唯一可疑端口做**只读**识别：

```bash
python -m esptool --chip esp32s3 --port /dev/cu.usbserial-214220 flash-id
```

结果：`ERROR: Failed to connect to ESP32-S3: No serial data received.`

`ioreg -p IOUSB` 枚举结果（去重）：USB 总线上仅有 ASUS ROG STRIX Arion 硬盘盒、Realtek USB LAN、VIA/Genesys/Intel/Fresco 集线器、ROG STRIX SCOPE II RX、ROG PELTA 2.4GHz、ROG OMNI RECEIVER、Dynastion ANT USBStick2、Billboard Device。**没有 Espressif、WCH/CH34x、CP210x、FTDI 或 Waveshare 设备。**

**判定**：`/dev/cu.usbserial-214220` 属于 ASUS 外设（对应 `USB Product Name = "USB Serial"`），**不是目标开发板**。开发板当前未以可传数据的 USB-C 线接入 Mac。

> T02 的"原厂备份 + flash-id 核对"因此标记 **BLOCKED_HARDWARE**。需要用户用数据线（能传数据的，不是纯充电线）直连 Mac 后重跑；端口需通过插拔对比确认，不能凭首个串口猜测。

## 4. 文档契约校验

```bash
/Users/hanxin/.workbuddy/binaries/python/versions/3.13.12/bin/python3 -m venv .venv-doccheck
.venv-doccheck/bin/python -m pip install 'jsonschema>=4.22,<5'
cd Bot_Status_v1_Handoff && ../.venv-doccheck/bin/python tools/validate_contracts.py
```

结果 **PASS**（本机实测）：

| 项 | 数量 |
|---|---|
| schemas | 3 |
| 合法 device 示例 | 18 |
| 合法 event 示例 | 3 |
| 未探测 capability 模板 | 4 |
| 反例被拒（结构） | 9 |
| 反例被拒（原始 JSON） | 7 |
| state / gesture / usage 用例（已定义未执行） | 12 / 12 / 15 |
| 最大样例 JSON 字节 | 1732（帧上限 8192，余量充足） |
| failures | 无 |

注意：4 份 capability 模板为 `unprobed` 状态，仅为模板，不代表 Codex/WorkBuddy/Cursor/Hermes 已接入。state/gesture/usage 用例为"已定义未执行"，需要 T04/T05/T09/T13 的真实测试来兑现。

## 5. 本任务变更

- 新增：`.gitignore`（含 `.venv*/`、`.build/`、`**/build/`、`**/managed_components/`、`*.bin`、`*.elf`、`*.map`、`evidence/private/`、`.DS_Store`、原始 handoff zip；保留源码、锁文件、默认配置、脱敏样例）
- 新增：`evidence/baseline.md`（本文件）
- 新增：`docs/LOCAL_ENVIRONMENT.md`
- 未修改任何 Agent 的用户配置；未读取账号凭证；未做硬件写入。

## 6. 验收对照（T00 checklist）

- [x] 在 EIM 激活 shell 采集 `pwd / uname -m / sw_vers / idf.py --version / IDF_PATH / IDF_PYTHON_ENV_PATH / python / ls /dev/cu.*`
- [x] 确认 v6.1 来自当前 EIM，复用不重装
- [x] `.gitignore` 已建，私有/二进制/构建产物不入库
- [x] `tools/validate_contracts.py` 输出完整通过（无失败项）
- [x] 确认原厂备份、登录数据、Hooks 原始 prompt 不会被纳入 Git（备份在仓库外，本任务尚未产生备份）
- [ ] 板型/Flash/PCB 核对 —— **阻塞**：板子未接入（见第 3 节）
