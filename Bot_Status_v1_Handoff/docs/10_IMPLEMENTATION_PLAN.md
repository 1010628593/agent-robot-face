# Bot Status v1 Implementation Plan

> **For agentic workers:** 按本文件逐任务执行；可采用 superpowers 的 executing-plans 或 subagent-driven-development。没有这些技能的本地 Agent 直接遵循本文件即可，不把安装技能作为阻塞项。所有完成项附命令、结果和证据，不以“代码已写”代替验收。

**Goal:** 实现1.75-B的Face、Agent Picker、Usage/Quota三屏，并通过Mac只读汇聚四个桌面Agent。  
**Architecture:** Mac掌握会话/统计/选择权威，USB发送有限语义快照；ESP32基于官方BSP和LVGL运行独立动画及手势；未知能力明确降级。  
**Tech Stack:** ESP-IDF v6.1、C、LVGL 9、官方BSP；Python 3.11+、asyncio、pyserial、SQLite、aiohttp、Pydantic 2。  
**Spec:** `docs/01_PRD.md`、`docs/02_UI_UX.md`、`docs/03_STATE_MODEL.md`、`docs/05_PROTOCOL.md`、`docs/14_INTERFACES.md`。

## Global Constraints

- 设备固定1.75-B；不刷C版固件；v6.1不擅自降级。
- USB v1；三逻辑页；Agent选择仅切观察对象。
- 四产品：Codex、腾讯WorkBuddy桌面、Cursor原生Agent、Nous Hermes。
- unknown/N/A不是0；exact与coverage独立；不造配额、进度或权限等待。
- 不执行任务、不自动审批、不读取Cookie、不修改现有安全策略。
- 所有外部配置写入与烧录需明确确认；无理由删除用户代码禁止。

## 任务依赖和检查点

```text
T00 → T01（四源探测，与UI并行）
T00 → T02 → T03 → T04/T05 → T06 → T07/T08
T03 → T09 → T10/T11/T12/T13 → T14
T01 + T14 → T15/T16/T17/T18 → T19 → T20 → T21 → T22
```

T00–T02之后先给用户一次报告。T14产出标有SIM的UI预览。T15–T18可并行开发，但共享CanonicalEvent、账本和配置安装器，不各自发明服务。WorkBuddy阻塞必须在T01就提出，不能最后伪装为完成。

每个编码任务遵守：**添加失败测试 → 执行看失败原因 → 最小实现 → 跑正/反例 → commit或提交可审diff**。已有工作区不强制新建仓库；提交前确认用户改动不被纳入。

---

## T00 · 只读盘点与工程基线

**文件**：创建 `evidence/baseline.md`、`docs/LOCAL_ENVIRONMENT.md`、`.gitignore`。  
**输入**：当前Mac工作区和已安装EIM；**产出**：真实版本、目录、USB设备候选，不改任何Agent配置。

- [ ] 在EIM激活终端运行并保留输出：
  ```bash
  pwd; git status --short
  uname -m; sw_vers
  idf.py --version
  printf '%s\n' "$IDF_PATH" "$IDF_PYTHON_ENV_PATH"
  python -c 'import sys; print(sys.executable)'
  ls /dev/cu.*
  ```
- [ ] 确认v6.1来自当前EIM；复用而非重复安装。读取板标签，不凭图片推断SKU。
- [ ] `.gitignore`至少加入 `.venv*/`、`.build/`、`**/build/`、`**/managed_components/`、`*.bin`、`*.elf`、`*.map`、`evidence/private/`、`.DS_Store`；保留源码、锁文件、默认配置和脱敏测试样例。
- [ ] 文档校验：`python tools/validate_contracts.py`。输出完整通过；失败先修文档，不进入固件修改。
- [ ] 验收：不会把原厂备份、登录数据、Hooks原始prompt纳入Git。

## T01 · 四个 Agent 能力探测（优先排风险）

**文件**：创建 `evidence/capabilities/{codex,workbuddy,cursor,hermes}.json`、`evidence/adapters/*-probe.md`。  
**输入**：`acceptance/capability-*.template.json`、官方资料；**产出**：每产品本机版本、入口、state/usage/quota覆盖。

- [ ] 只读检查安装应用及CLI版本；命令先经`command -v`确认存在，WorkBuddy没有CLI就记录桌面应用版本。
- [ ] 对照四份adapter文档确定真实入口。为每个必需字段填写 `observed/documented/not_available` 的证据，不读取账号密钥。
- [ ] 需要修改Hooks/MCP/插件时先给配置diff与恢复方式并申请确认；未确认阶段报告not_probed，不假填ready。
- [ ] 每个产品记录开始、工具、等待、结束、取消、usage的可观察来源；检查额度权限类型和账号scope。
- [ ] 验收：四份报告通过Schema；WorkBuddy无官方桌面事件源时输出 `BLOCKED_SOURCE` 与选择A/B，而不是套CodeBuddy CLI接口。

## T02 · 原厂备份和v6.1官方样例

**文件**：读取vendor官方样例；创建 `evidence/idf61-build.md`、`evidence/hardware-baseline.md`。备份只在仓库外私有目录。  
**输入**：真实PORT和标准1.75工程；**产出**：可恢复备份、固定依赖、屏幕/触摸基线。

- [ ] 完整执行 `START_HERE_macOS.md` 的只读识别和备份步骤；检查16MiB/哈希，安全功能异常时停止。
- [ ] 编译官方 `examples/esp-idf/02_lvgl_demo_v9`，保留Git SHA和`dependencies.lock`。v6.1不兼容先定位首个API/依赖错误。
- [ ] 人工确认后烧录同一build的输出。构建成功与上板成功分别记录。
- [ ] 显示黑/红/绿/蓝短测、四方向触摸、15分钟运行；不长亮白屏。
- [ ] 验收：无重启/白屏，触点方向一致；记录不支持或未测的硬件，不擅自初始化音频/IMU。

## T03 · 合同模型、构建边界与测试地基

**文件**：创建 `bridge/pyproject.toml`、`bridge/src/bot_bridge/models.py`、`bridge/tests/conftest.py`、`bridge/tests/test_contracts.py`、`tests/native/CMakeLists.txt`、`firmware/components/bot_core/include/bot_types.h`。  
**输入**：本包Schema；**产出**：CanonicalEvent/DeviceMessage/CapabilityReport及独立C测试工程。

- [ ] 写测试：所有`contracts/examples/*.json`可解析；`invalid`全部拒绝；第06/07是语义错误，也必须拒绝。
- [ ] 首次测试失败应为模型/validator未实现，不能修改反例让它通过。
- [ ] 实现严格Pydantic模型和语义验证；所有HTTP/串口共用这一入口。依赖写入pyproject并锁定。
- [ ] 验收命令：`.venv-bridge/bin/python -m pytest bridge/tests/test_contracts.py -q`；native C最小测试可编译。

## T04 · 独立手势识别

**文件**：创建 `firmware/components/bot_core/gesture.c`、`include/bot_gesture.h`、`tests/native/test_gesture.c`。  
**输入**：TouchSample与interaction tokens；**产出**：单接触最多一个GestureEvent。

- [ ] 用 `acceptance/gesture_cases.json` 建表驱动测试；长按必须用TICK触发。
- [ ] 先测试关键边界：649ms不是HOLD，650ms是；13px移动取消长按；HOLD后的UP没有TAP；唤醒接触只产生WAKE_ONLY。
- [ ] 实现有限状态机：IDLE/PRESSED/SWIPE_LOCKED/HOLD_FIRED/CONSUMED；数值来自tokens，不散落magic number。
- [ ] `ctest --test-dir .build/native -R gesture --output-on-failure` 全部通过，含快速轻触、斜划、cancel。

## T05 · 设备帧解码、Model与路由

**文件**：创建 `bot_core/frame_parser.c`、`device_model.c`、`router.c`、对应头文件；测试 `test_frame_parser.c`、`test_device_model.c`、`test_router.c`。  
**输入**：原始字节、已校验消息、GestureEvent；**产出**：单线程可预测Model与UIEffect。

- [ ] 测试按每个字节拆分合法帧、多帧合并、日志夹杂、8193字节超长、重复key、过深对象。
- [ ] 先写路由表测试：Face横滑到Stats；Stats上/下切tab；Stats右返回；Picker长按取消；Stats左不导航。
- [ ] 模型测试旧link/旧seq/错误rev都不更新；新welcome后清理pending与旧stats。
- [ ] 解码到固定长度结构后入队；无效消息不得部分更新Model。固件不能动态加载/执行消息里的代码。
- [ ] 验收：native全部通过，超长帧之后下一合法帧能恢复。

## T06 · Face三层动画与LVGL模拟器

**文件**：创建 `firmware/components/bot_ui/{ui.c,face.c,face_anim.c}`、`include/bot_ui.h`、`simulator/`、`evidence/ui/face/`。  
**输入**：BotModel和视觉tokens；**产出**：Face及真实LVGL SDL预览。

- [ ] 写纯参数测试：state→eyePose映射、done保持3000ms、same-state不重置animation_epoch。
- [ ] 沿官方BSP创建display，只替换demo调用；不改CO5300/触摸/电源引脚。
- [ ] 创建静态双眼→眨眼→gaze→8种状态；对象复用，不以GIF作为主动画。
- [ ] 加详情/轻触反馈；waiting/error被触摸时保持业务语义；SIM标签永久可见。
- [ ] 验收：记录全部状态截图及10秒视频；466圆形边界不裁切；实测30fps目标/帧耗时而非凭感觉。

## T07 · Agent Picker与选择确认UI

**文件**：创建 `bot_ui/picker.c`、`tests/native/test_selection_ui.c`、`evidence/ui/picker/`。  
**输入**：4条catalog、selected与preview、ack/focus；**产出**：Carousel预览和pending反馈。

- [ ] 测试滑动仅改变preview；点击发一次action；同action重试不换ID；accepted ack但未到对应focus时不能显示新Agent旧状态。
- [ ] 按契约实现650ms长按进度、四卡循环、15s无操作取消、down/hold取消。
- [ ] 选择请求2s重试一次、4s超时恢复；offline只能预览，不发送选择。
- [ ] 验收：断开USB选择不会假成功；快速划过4项只提交最后点击项；Mac权威选择覆盖本地缓存。

## T08 · Usage/Quota界面与格式化

**文件**：创建 `bot_ui/stats.c`、`bot_core/format.c`、`test_format.c`、`evidence/ui/stats/`。  
**输入**：Stats消息；**产出**：双子页，大数字+2行指标/2个额度窗口。

- [ ] 写格式测试：null→N/A，0→0；exact+partial必须仍显示partial；110%不能假显示100%；unlimited→Unlimited而非100%。
- [ ] 展示范围TODAY/SESSION、as_of、过期标记、~估算/手填标签；多窗口不求平均。
- [ ] 无可信quota显示能力说明；未知分母只显示balance，不画伪百分比环。
- [ ] 验收：18个消息例的相关stats在模拟器可见，长label按规定截断，最小字号20px，图表无未来假数据。

## T09 · SessionStore与账本基础

**文件**：创建 `bridge/src/bot_bridge/{reducer.py,ledger.py,storage.py}`；测试 `test_reducer.py`、`test_ledger.py`。  
**输入**：CanonicalEvent；**产出**：代表会话、去重usage与SQLite事务。

- [ ] 实现测试helper后写：
  ```python
  def test_parallel_tools_are_a_set(event):
      from bot_bridge.reducer import SessionStore
      s = SessionStore()
      s.apply(event('run_started', source_seq=1))
      s.apply(event('tool_started', tool_call_id='a', source_seq=2))
      s.apply(event('tool_started', tool_call_id='b', source_seq=3))
      s.apply(event('tool_finished', tool_call_id='a', source_seq=4))
      assert s.focus('codex', 2000)['state'] == 'tool'
  ```
- [ ] 执行FAIL→最小实现；遍历 `acceptance/state_cases.json`。不同run的迟到结束不能盖住新run。
- [ ] 账本累计值100,160,160仅记100,60,0；进程重启或counter_epoch改变不跨epoch做差。相同event_id重复只入一次。
- [ ] SQLite schema迁移明确版本；事件事实与累计水位在同一事务更新。
- [ ] 验收：断电/重开SQLite去重有效，重启不恢复旧working为当前工作。

## T10 · USB端到端握手

**文件**：创建 `bridge/src/bot_bridge/{protocol.py,serial_link.py,publisher.py}`、`firmware/components/bot_transport/usb_serial.c`；测试 `test_serial_link.py`、`test_publisher.py`。  
**输入**：消息Schema，虚拟串口/真实USB；**产出**：唯一串口拥有者、双向在线态。

- [ ] 伪串口先测试任意分片、日志、读超时、EOF、相同handshake_id重复hello不换link、新handshake_id才换link。
- [ ] 固件USB读写与日志共用一个受控拥有者；选择官方IDF支持的读取方式，不能VFS与driver双读。
- [ ] Host收到hello生成link；welcome/catalog/focus/stats按顺序发布；seq分方向，控制队列优先。
- [ ] 2s ping，6s离线；旧帧不复活新连接。端口仅在确认后绑定到device_id，不能匹配第一块串口。
- [ ] 验收：USB上模拟状态准确渲染；关闭Bridge后设备离线；拔插20次无需重刷。

## T11 · Loopback入口和安全Hook emitter

**文件**：创建 `bridge/src/bot_bridge/{server.py,auth.py,spool.py,hook_emitter.py}`；测试 `test_http_security.py`、`test_spool.py`、`test_hook_emitter.py`。  
**输入**：脱敏事件，私有token；**产出**：受控HTTP与200ms旁路发送器。

- [ ] 写拒绝测试：缺token/错误Origin/非localhost Host/64KiB以上body/额外prompt字段。
- [ ] HTTP只绑定127.0.0.1:17875，默认不开放LAN；token自动生成0600，health不泄露会话。
- [ ] emitter先白名单提取，再发HTTP，失败放有界spool；不保存完整stdin；stdout为空且不返回审批指令。
- [ ] 模拟Bridge不存在、服务器500、spool满、权限拒绝，原Agent流程仍不被阻断。
- [ ] 验收：源进程观测延迟在目标范围，失败路径无秘密日志；event_id在重放时保持不变。

## T12 · Host选择事务与动作白名单

**文件**：创建 `selection.py`、`actions.py`；测试 `test_selection.py`、`test_actions.py`。  
**输入**：action+base_revision；**产出**：权威选择、ack、幂等与明确拒绝。

- [ ] 测试两端同时选择、旧rev、重复action、断线后不重放、非法agent。
- [ ] select在SQLite事务提交后递增rev；同link/action缓存60秒最多256条。
- [ ] refresh按源限频；open_agent/open_usage默认disabled，即使设备请求也返回unsupported。
- [ ] 用户启用open动作后只能用固定应用/URL白名单，不执行拼接命令。
- [ ] 验收：同action重试两次只有一次状态改变；pending焦点/统计不会串源。

## T13 · Usage/Quota统计引擎

**文件**：创建 `usage.py`、`quota.py`、`imports.py`、`tests/test_usage.py`、`tests/test_quota.py`、`tests/test_imports.py`。  
**输入**：usage账本、官方快照、授权导入；**产出**：有scope/provenance的新鲜度统计。

- [ ] 使用 `acceptance/usage_cases.json` 写累计差分、cache包含关系、不同口径不可加、窗口超额等测试。
- [ ] 今日边界按IANA时区，存UTC；运行时间取活动区间并集而不是每会话直接加。
- [ ] 缺token/cost不补0，费用使用USD微单位整数；估价仅按版本化价表且quality=estimated。
- [ ] 额度更新失败保留上次快照并stale；重置时刻过了只标Reset pending，不把余额重置为满。
- [ ] 共享账号各Agent只展示同一个scope，禁止“所有Agent剩余额度合计”。
- [ ] 验收：相同官方窗口导入两次不翻倍；manual和exact并排保留来源，互不伪装。

## T14 · 三屏SIM联调检查点

**文件**：创建 `bridge/src/bot_bridge/simulate.py`、`bridge/tests/test_demo_flow.py`、`evidence/ui-preview.md`。  
**输入**：synthetic消息/冻结时间；**产出**：可演示三屏且始终标SIM的v1-ui-preview。

- [ ] 实现CLI `bot-status simulate`，不得默认接真实账号。模拟序列基于状态机重新发合法seq，而不是按独立样例文件名盲重放。
- [ ] 演示：Codex工作→Cursor等待toast→长按选择Cursor→quota N/A→返回Face→断线。
- [ ] 长按过程中业务状态更新不吞触控；stats回包延迟不阻塞眨眼。
- [ ] 验收：完整录屏、真实板视频、内存/帧耗时记录；用户确认布局后再精修图形。

## T15 · Codex真实适配

**文件**：创建 `adapters/codex.py`、`installers/codex_hooks.py`、`tests/adapters/test_codex.py`、`evidence/adapters/codex-verified.md`。  
**输入**：本机入口与受信任Hook样本；**产出**：真实state及能力允许的usage/quota。

- [ ] 先写脱敏golden test：提交→工具→权限→工具结束→Stop；重复/并发/取消分别断言。
- [ ] Hook安装仅合并自己的配置块，先diff和备份，不覆盖原hooks、不绕过trust。
- [ ] App Server按本地schema选择 `account/rateLimits/read` 或可用只读usage查询；不为状态观察启动无关实例冒充当前App。
- [ ] 额度账号与CLI/App来源一致且认证模式可用才标official；失败needs_auth/N/A。
- [ ] 验收：一轮真实任务的屏幕与源事件时间对应；Stop只显示Turn completed，不显示测试成功。

## T16 · Cursor真实适配

**文件**：创建 `adapters/cursor.py`、`installers/cursor_hooks.py`、`tests/adapters/test_cursor.py`、证据文件。  
**输入**：本机IDE Agent Hooks；**产出**：state、来源覆盖和可用导入路径。

- [ ] 测试preToolUse与beforeShellExecution同调用去重；工具结束不直接done；afterAgentResponse不当总结束。
- [ ] 以stop结果归约终止；没有确证waiting事件就partial，不用终端静默判权限等待。
- [ ] preCompact上下文占用只进context字段，不进今日消耗/账号quota。
- [ ] 团队Admin API只有已授权管理员才接；默认个人账户不发管理员请求。授权CSV按scope覆盖导入，不假造实时性。
- [ ] 验收：不会观察成Tab补全或Cloud Agent；至少真实开始/工具/结束和未知quota可见。

## T17 · Hermes真实适配

**文件**：创建 `adapters/hermes.py`、`integrations/hermes_bot_status/`、`tests/adapters/test_hermes.py`、证据文件。  
**输入**：CLI/Gateway实际版本插件Hook；**产出**：state/usage及Provider归属。

- [ ] 根据本机插件API注册观察回调；CLI场景不使用仅Gateway的HOOK.yaml代替。
- [ ] 测试pre/post approval、工具并发、completed/failed/interrupted结束；回调return None，不输出控制指令。
- [ ] 从post_api_request.usage等有证据源获取消耗，保留缓存字段语义。
- [ ] Hermes走Codex/OpenRouter等不同后端时按Provider展示配额；没有统一Hermes余额。
- [ ] 验收：断开设备仍正常执行；同底层请求不因Hermes与Codex双观测合计两次。

## T18 · WorkBuddy桌面适配与显式能力门

**文件**：创建 `adapters/workbuddy.py`、可选 `integrations/workbuddy_status_mcp/`、`tests/adapters/test_workbuddy.py`、`evidence/adapters/workbuddy-verified.md`。  
**输入**：T01能力报告和已授权桌面数据源；**产出**：A自动观测，或用户接受的B自报降级。

- [ ] 若当前桌面版本有已确认事件/结构日志，写golden tests后读取白名单字段；版本不匹配立即partial。
- [ ] 不能找到来源时停止本任务的“自动接入”承诺，发出BLOCKED_SOURCE。不得发明Hook路径或API。
- [ ] 用户接受MCP方案后才安装：只暴露 `report_bot_status` 的状态参数，不提供shell执行、权限审批或凭据访问；所有结果quality=reported、60秒过期。
- [ ] WorkBuddy官方钱包按实际池/有效期展示；无可用只读接口则manual/import/N/A。不把CodeBuddy CLI套餐额度搬过来。
- [ ] 验收：报告明确区分observed/reported/manual；若仍blocked，v1-integrated不得无条件标四源全部完成。

## T19 · 通知、亮度、睡眠与防烧屏

**文件**：创建 `bot_ui/notice.c`、`bot_core/power_policy.c`、`tests/native/test_power.c`、`bridge/tests/test_notices.py`。  
**输入**：用户交互时间、业务变化、link、跨Agent事件；**产出**：不中断操作的通知及可预测电源策略。

- [ ] 冻结时钟测试180s调暗、6s离线、30s关屏；heartbeat不得重置idle计时。
- [ ] waiting/error优先通知；Picker手势进行中不抢焦点；同notice_id去重；最多短Toast+badge，不自动选择另Agent。
- [ ] 唤醒首触整个接触消耗；关屏不进影响USB的深睡眠；调亮沿官方BSP。
- [ ] 1–2px周期pixel shift、低亮背景；持久状态不固定满亮白；reduced-motion保留语义。
- [ ] 验收：2分钟视频核查无误触/抢屏，30分钟待机测试记录亮度和温升主观异常。

## T20 · 安全与故障注入

**文件**：创建 `tests/faults/`、`bridge/tests/test_redaction.py`、`evidence/security.md`。  
**输入**：恶意/无效消息、账户失效、速率限制；**产出**：无副作用的安全边界证据。

- [ ] 测试重复JSON key、深度13、NaN、8193字节、超大hook、伪造link、随机控制字段。
- [ ] 将 `DUMMY-NOT-A-SECRET` 放入模拟prompt/cookie/APIkey，确认设备帧、日志、spool均无该原始字段；只保留测试特定脱敏标志。
- [ ] 本地网页跨站POST到loopback被Origin/token挡住；GET也不允许未授权读状态。
- [ ] 模拟quota 401/429/timeout、USB断开、队列满；无无限重试、内存不增长、源Agent继续工作。
- [ ] 验收：无账号登录/注销/充值/额度消耗写操作，无高风险配置变动。

## T21 · 安装、doctor与恢复

**文件**：创建 `bridge/src/bot_bridge/cli.py`、`installers/launchd.py`、`tests/test_cli.py`、`docs/USER_GUIDE.md`、`docs/RELEASE_NOTES.md`。  
**输入**：已通过的服务与协议；**产出**：用户可启动/停止/排错/卸载。

- [ ] CLI至少实现 `doctor`、`run`、`simulate`、`status`、`probe`、`hooks install --dry-run`、`hooks uninstall --dry-run`、`usage import`、`quota import`。命令实现后再在USER_GUIDE公布为可运行。
- [ ] doctor默认只读：板型、IDF、依赖、端口占用、状态源、token权限；不输出token。
- [ ] launchd只在用户确认后写入当前用户LaunchAgents；暂停Bridge可立即释放串口；卸载仅删除本项目精确配置块。
- [ ] 发布锁定的源码/依赖/分区/二进制哈希/示例配置；不打包账号文件、系统字体或原厂私有备份。
- [ ] 验收：全新venv从锁文件安装；重启Mac自动启动（仅用户启用后）；卸载保留原有Hooks和其他应用。

## T22 · HIL、全链路验收与发布标记

**文件**：创建 `evidence/acceptance/{summary.md,results.json}`、`evidence/compatibility.md`、release notes。  
**输入**：全部前置任务；**产出**：有证据的v1发布或明确未通过清单。

- [ ] 执行 `docs/11_TEST_ACCEPTANCE.md` 所有必选测试。SIM与真实数据分开录制。
- [ ] 1000次状态切换、20次USB拔插、10次Mac睡眠唤醒、8小时连续运行；记录重启计数、heap最低值、p95帧时、延迟。
- [ ] 每Agent展示一次实际覆盖，WorkBuddy能力门未过则不把reported写成automatic。
- [ ] 统计对照官方同scope/时间数据；不匹配记差异原因，不强行抹平。
- [ ] 出具PASS/PARTIAL/BLOCKED项，用户接受降级需有书面记录。只有真实满足才标 `v1-integrated`。

## 时间安排（计划估计，非实测）

硬件/v6.1基线0.5–2天；三屏/协议/模拟器4–7天；Bridge/账本3–5天；四源接入3–8天（WorkBuddy探测可能阻塞）；打磨和验收2–4天。首次嵌入式开发按4–6周业余时间更稳妥，不承诺四个账号权限都自动可用。

优先级：**Face和触摸 > 可靠选择与离线 > 真实四源状态 > 可信统计 > 炫酷动画打磨**。有可靠性问题时降低动画复杂度，不降低真实性。
