# Bot Status v1 · 实施日志

> 协议与验收口径以 `Bot_Status_v1_Handoff/contracts/` 与 `Bot_Status_v1_Handoff/docs/05_PROTOCOL.md` 为准。
> 只允许使用 5 种实证描述：`文档校验通过` / `主机测试通过` / `ESP-IDF 编译通过` / `已烧录` / `实机验收通过`。
> 任务状态表另见 `reports/task-status.md`（`Bot_Status_v1_Handoff/IMPLEMENTATION_STATUS.md` 的副本被复制出来更新，以免改动规格包导致 MANIFEST 失配）。

---

## T00 · 只读盘点与工程基线

**状态**：完成（硬件识别子项 `BLOCKED_HARDWARE`） · **实证等级**：`文档校验通过`

### 变更文件

| 文件 | 动作 |
|---|---|
| `.gitignore` | 新增 |
| `evidence/baseline.md` | 新增（本机采集输出） |
| `docs/LOCAL_ENVIRONMENT.md` | 新增 |
| `tools/idf-env.sh` | 新增（EIM 环境助手，见下） |
| `reports/implementation-log.md` | 新增（本文件） |
| Git 仓库 | 本任务在工作区初始化（此前无仓库，无既有改动被覆盖） |

### 执行命令与结果

```text
. "$HOME/.espressif/tools/activate_idf_v6.1.sh"
idf.py --version                 -> ESP-IDF v6.1
IDF_PATH                         -> /Users/hanxin/.espressif/v6.1/esp-idf
IDF_DESCRIBE                     -> v6.1 (HEAD fff9895c82d744c7237be8847347bdd1b07c6643)
python                           -> /Users/hanxin/.espressif/tools/python/v6.1/venv/bin/python 3.14.7
python -m esptool version        -> 5.4.0
uname -m / sw_vers               -> arm64 / macOS 27.0 (26A5425a)
ls /dev/cu.*                     -> Bluetooth-Incoming-Port, debug-console, XiaomiBuds5Pro, usbserial-214220
esptool --port ... flash-id      -> ERROR: No serial data received（非开发板，见下）
tools/validate_contracts.py      -> PASS（3 schemas / 18 正例 / 9 反例拒 / 0 failures）
```

### 关键发现

1. **v6.1 来自 EIM**，路径与激活脚本均存在，无重复安装、无降级。
2. **开发板未接入**：`ioreg -p IOUSB` 全量枚举无 Espressif / WCH / CP210x / FTDI / Waveshare 设备；`/dev/cu.usbserial-214220` 对应 ASUS 外设（`USB Product Name = "USB Serial"`）。T02 的 flash-id 核对与原厂备份 → `BLOCKED_HARDWARE`，需用户用数据线接线后重跑。
3. **EIM 激活脚本遗漏 cmake**（`idf.py` 报 `"cmake" must be available on the PATH`）。根因：EIM 把 cmake 装成 `tools/cmake/4.0.3/CMake.app/Contents/bin/cmake`，其静态 PATH 未包含该布局；且 `idf_tools.py export` 因 `esp-rom-elfs` 版本不匹配整体报错退出，不会补 PATH。**处置**：不改动 EIM 目录、不安装/升级工具链，新增 `tools/idf-env.sh` 在激活后补入 EIM 自带 cmake 4.0.3。已验证 `idf.py / cmake / ninja / python` 均可用。
4. 文档契约校验全绿，但 4 份 capability 模板为 `unprobed`；state/gesture/usage 用例"已定义未执行" —— 不能据此认为四源已接入或行为已验证。

### 未做 / 不改

- 未连接开发板；未 `erase-flash`；未写 eFuse；未读取任何账号凭证；未修改任何 Agent 的用户配置。

### 剩余风险

| 风险 | 说明 | 缓解 |
|---|---|---|
| 板子未接入 | T02 备份与实机验收无法进行 | 用户接线（数据线）→ 插拔对比确认端口后重跑 |
| v6.1 属官方未验证组合 | 示例 `sdkconfig.defaults` 由 IDF 5.4.0 生成，官方声明验证到 5.5/6.0.x | T02 如实记录首个真实编译错误，不猜测、不擅自降级 |
| 组件版本浮动 | BSP manifest 为 `"*"`，LVGL 为 `"9.4.*"` | 构建后记录 `dependencies.lock` 与解析版本 |

### 下个任务

T02（官方示例 v6.1 构建 + 只读硬件识别/备份）→ 完成后给用户一次 T00–T02 汇总报告。

---

## T02 · 原厂备份和 v6.1 官方样例

**状态**：构建完成（`ESP-IDF 编译通过`）；硬件部分 `BLOCKED_HARDWARE`
**证据**：`evidence/idf61-build.md`、`evidence/locks/02_lvgl_demo_v9.idf61.dependencies.lock`

- [x] 克隆标准 1.75 仓库（非 C 版）：`e4344e70c2fa78a13e8a06566507f1ba8af6672a`（2026-08-21）
- [x] `idf.py -B build-idf61 build` 官方 `02_lvgl_demo_v9` → **Project build complete.（2160/2160）**
      `lvgl_demo_v9.bin` 967,696 B · SHA256 `82fdf3c91dab5ecf…cce34e27`
- [x] 记录 `dependencies.lock`：BSP `waveshare/esp32_s3_touch_amoled_1_75 3.0.1`、LVGL `9.4.0`、CO5300 `2.1.0`、CST9217 `2.0.0`、idf `6.1.0`
- [x] `flash-id` 核对：ESP32-S3 (QFN56) rev v0.2 · **16MiB** · 8MB PSRAM · USB-Serial/JTAG · MAC `28:84:85:8d:71:e0` → 端口 `/dev/cu.usbmodem2142101`
- [x] 安全功能只读检查：Secure Boot **关**、Flash 加密 **关**、WR_DIS/RD_DIS=0、JTAG 未禁用 → 无异常，备份为明文
- [x] 完整 `read-flash` 私有备份：**16,777,216 B**（=16MiB）· SHA256 `4c020428c2fe5c7610a4694682ddc473427707ad553a21afe168c090d0b33c38`
      → `$HOME/Workspace/bot-status-private-backups/20260906-202608/`（仓库外，`umask 077`）
- [ ] 首次烧录 —— **待用户单独确认**（已给出待写固件哈希与恢复命令，见 `evidence/hardware-baseline.md`）
- [ ] 黑白红绿蓝短测 / 四方向触摸 / 15 分钟运行 —— 依赖烧录

补充发现：**原厂分区表与官方示例分区表布局不同**（原厂 `factory@0x110000`，示例 `factory@0x10000`）。烧录示例会覆盖分区表，原厂 factory/assets 成为孤儿分区；因此"原厂恢复"必须整片回写备份，不能只写 app。

补充：构建过程中新增 `tools/idf-env.sh` 解决 EIM 激活脚本缺 cmake 的问题（不改 EIM，不降级）。

---

## T01 预探测（只读，仅安装与版本，未做完整能力探测）

**实证等级**：`文档校验通过`（本机 `command -v` / Info.plist 读取，未读取任何凭证、未改任何配置）

| Agent | 本机实测 | 备注 |
|---|---|---|
| Codex | `~/.codex/` 存在（含 `hooks.json`、`config.toml`、`version.json`、`app-server-control/app-server-control.sock`）；`version.json` 记录 `latest_version 0.152.0`（2026-09-01 检查）；二进制在 `~/.local/bin/codex`（不在受限 PATH 内，需显式路径调用） | 已安装且有使用痕迹；**CLI-only vs 桌面覆盖待 T01 确认** |
| WorkBuddy | `/Applications/WorkBuddy.app` 版本 **5.5.3** | 无 CLI；桌面结构化事件源未知 → T01 最高风险项 |
| Cursor | `/Applications/Cursor.app` 版本 **3.19.14**；`cursor` CLI 不在受限 PATH | IDE Agent Hooks 需在 T01 核对实际入口 |
| Hermes | 未在 PATH 中找到；未进一步定位 | 待 T01 确认是否安装 |

未做：读取 `~/.codex/auth.json` 或任何含凭证的文件；修改任何 Agent 配置；安装 Hook/MCP。
下一步（T01）将按 `docs/adapters/*.md` 逐源填写 `observed/documented/not_available` 证据，并给出 WorkBuddy 的 A/B 选项。
