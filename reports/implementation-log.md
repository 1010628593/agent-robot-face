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
- [x] 首次烧录 —— **已执行（用户已单独确认）**：`Wrote 967696 bytes @0x10000` + `Hash of data verified.`
      只写 3 个分区，**未 `--erase-all`**、未写 eFuse。命令用构建产物的 `flash_args`（dio/80m/16MB）
- [x] 启动日志抓取 → **实机跑的是 ESP-IDF v6.1**；`co5300 2.1.0` 面板创建成功；`CST9217` 报 **466×466**；8MB PSRAM 测试 OK；无 Panic、无复位行
- [x] 屏幕目测确认 —— **用户上传实机照片**（`evidence/ui/official-demo/display-benchmark-photo.jpg`），可见 LVGL 9 性能基准页：All scenes 49 FPS / Empty 29 FPS / Wallpaper 31 FPS / Rotating rectangle 66 FPS；无花屏、无错位
- [ ] 四方向触摸 —— 待用户手测回报
- [ ] 15 分钟无重启 —— 后台抓 900 s 日志进行中（~21:17 完成），回填复位/Panic 计数

**实证等级升级**：`ESP-IDF 编译通过` + `已烧录`（`实机验收通过` 待目测与长跑补齐）

补充发现：**原厂分区表与官方示例分区表布局不同**（原厂 `factory@0x110000`，示例 `factory@0x10000`）。烧录示例会覆盖分区表，原厂 factory/assets 成为孤儿分区；因此"原厂恢复"必须整片回写备份，不能只写 app。

补充：构建过程中新增 `tools/idf-env.sh` 解决 EIM 激活脚本缺 cmake 的问题（不改 EIM，不降级）。

---

## T01 · 四个 Agent 能力探测（只读，未读凭证、未改配置）

**状态**：4 份报告写入 `reports/capabilities/`，对 `capability-report.schema.json` 全 PASS。
**证据**：`reports/capabilities/{codex,workbuddy,cursor,hermes}.json` + `reports/capabilities/SUMMARY.md` + `evidence/probes/`

- [x] 实机探测版本与入口
  - Codex CLI 0.152.0，`~/.local/bin/codex`，`~/.codex/config.toml`（notify 已指向 SkyComputerUseClient），无 Codex hooks dir
  - WorkBuddy Desktop 5.5.3，`/Applications/WorkBuddy.app`；主进程在加密 `app.asar`（不逆向）；`app.asar.unpacked/main/` 仅 `qimei-helper.js`；`app.asar/cli/` 是 `@genie/agent-cli`（CodeBuddy CLI 重打包）→ **CodeBuddy CLI 与 WorkBuddy Desktop 不可等同** 已实测确认
  - Cursor 3.19.14，`~/.cursor/hooks.json` v1 已用 AgentKeyboard + memmy 占用；任何新增必须最小 diff
  - Hermes 0.21.0，`~/.local/bin/hermes`，plugin model 完整；`hermes hooks list` 6 个 hooks 全部已有调用方（AgentKeyboard + mnemon）
- [x] 4 份 capability-report 通过 schema 校验（0 errors）
- [x] 用户决策（2026-09-06）：**选项 B** —— WorkBuddy 不接受 MCP reported 降级，T18 维持 BLOCKED_SOURCE
- [ ] T15–T17 三源真实 adapter 干跑（T18 跳过）

替换了先前"T01 预探测"占位（旧版本只看了版本，未做完整能力门）。

## T03 · 合同模型、构建边界与测试地基

**状态**：DONE（TDD：测试先行 → 先失败后实现 → 20/20 PASS）
**证据**：`bridge/`、`firmware/components/bot_core/include/bot_types.h`、`tests/native/`

- [x] `bridge/pyproject.toml` + `bridge/requirements.lock`（pydantic 2.13.5 / jsonschema 4.26.0 / pytest 8.4.2，全锁定）
- [x] `bridge/src/bot_bridge/models.py` 三层校验：
  1. `strict_load` 解析器层（8192B / UTF-8 / 拒重复键 / 拒 NaN·Infinity / 深度 ≤12，与 `validate_contracts.py` 同规则）
  2. Pydantic 严格模型（10 种 DeviceMessage body + AgentEvent + UsageRecord + CapabilityReport，全部 `extra="forbid"`）
  3. 语义层（metric key/unit、终态 reason、quota 禁止伪造数字、used_pct 一致性、ACK 配对、event id 规则、hello 唯一 link_id=null/seq=0 例外）
- [x] `bridge/tests/conftest.py` + `test_contracts.py` 20 例：18 正例解析 / 9 反例全拒（含 06 单位错误、07 重复 agent、09 link 缺失）/ 解析器 8 例 / 语义抽查 5 例
- [x] TDD 过程留痕：首次运行 `ModuleNotFoundError` → 实现后 2 失败（09 link 规则 + source_health 字段）→ 修复后 20/20 PASS，**未修改任何反例**
- [x] `firmware/components/bot_core/include/bot_types.h`（466×466 / 8192B / hold 650ms / swipe 56px 合同 token）
- [x] `tests/native/CMakeLists.txt` + `test_bot_types.c`：macOS clang 编译 + ctest PASS（`-Wall -Wextra -Werror`）
- [x] 验收命令复核：`.venv-bridge/bin/python -m pytest bridge/tests/test_contracts.py -q` → **20 passed**；`validate_contracts.py` → **PASS**（未回归）

## T04 · 独立手势识别

**状态**：DONE（TDD：先生成表 + 构建失败 → 实现 → 12/12 PASS）
**证据**：`firmware/components/bot_core/gesture.c`、`include/bot_gesture.h`、`tests/native/test_gesture.c`、`tests/native/gesture_cases.inc`（由 `tools/gen_gesture_cases.py` 从 `acceptance/gesture_cases.json` 生成）

- [x] 表驱动：12 用例 36 样本全部由 JSON fixture 生成，JSON 保持唯一事实源
- [x] FSM：IDLE / PRESSED / HOLD_FIRED / CONSUMED；单接触最多一个事件
- [x] 边界全过：649ms 非 HOLD（G02）；650ms TICK 触发 HOLD（G03，UP 不触发）；13px 取消长按（G04）；HOLD 后 UP 无 TAP（G03）；唤醒接触只产 WAKE_ONLY（G10）；cancel 静默（G11）；斜划被 axis_ratio=1.4 拒绝（G09）；55px 低于 swipe_min_px 拒绝（G12）
- [x] 数值全部来自 interaction_tokens.json（tap 250ms/12px、hold 650ms、swipe 56px/120-700ms/1.4），轴比用整数乘除无浮点
- [x] 与 bot_types.h 已有枚举合并（bot_gesture_kind_t 单一定义，event struct 留给 T05 路由）
- [x] 验收：`ctest --test-dir build -R gesture` PASS；全量 ctest 2/2 PASS（-Wall -Wextra -Werror）

## T05 · 设备帧解码、Model 与路由

**状态**：DONE（5/5 native 套件 PASS）
**证据**：`firmware/components/bot_core/{bot_json,bot_frame,bot_model,bot_router}.c` + 对应头文件 + `tests/native/test_{frame_parser,device_model,router}.c`

- [x] `bot_json.c` 严格 JSON 解析器（固定节点池 512 + 字符串池，零堆分配）：拒 NaN/Infinity/1e999 溢出、重复键、深度>12、非法 UTF-8（含代理对/超长编码）、非对象根、尾随垃圾
- [x] `bot_frame.c` 字节流状态机：`@bot ` 前缀 + 8192B 预算；拆包/粘包/CRLF/日志夹杂/UTF-8 跨 chunk；超长行丢到下一换行重同步并计数
- [x] 解码器：envelope + 7 种 H→D body 全字段校验（schema 形状 + validate_contracts.py 语义层：key↔unit 映射、unavailable→null、quota 禁伪造数字、used_pct ±0.1、ACK accepted⟺ok、目录四唯一 agent、shared_with 不含自身）；v1 严格拒未知键；解析树立即释放，只出定长 bot_msg_t
- [x] 修复：capabilities 有 open_agent/open_usage 字段；sparkline 允许 null 桶（缺段留空，NAN 占位不插值）
- [x] `bot_model.c`：仅当前 link + 严格递增 seq 可改 Model；welcome 仅握手期（seq=1）接受并清 pending/旧 stats；新 rev 单槽缓冲，ACK accepted 才提升；旧 link 命令永不生效
- [x] `bot_router.c`：02_UI_UX §5 手势表纯函数（Face 横滑→Stats / Stats 上下切 tab / Stats 右→Face / Picker 长按取消 / Stats 左仅边界反馈）
- [x] 测试：15 个真实契约正例字节级过帧；9 个反例全拒；6 严格 JSON 用例；7 语义反例；hello link_id=null/seq=0 唯一例外；流式 3 用例；model 16 断言；router 19 断言
