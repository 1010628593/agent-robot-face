# 14 · 实施接口与跨模块边界

本文件规定需要实现的接口，不表示这些源文件已包含在文档包中。机器数据结构以 `contracts/*.schema.json` 为准；下列C结构是渲染所需的内存投影，不是另一套线上协议。

## 1. 固件纯逻辑层

文件：`firmware/components/bot_core/include/bot_types.h`。

```c
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    BOT_IDLE, BOT_WORKING, BOT_TOOL, BOT_WAITING,
    BOT_DONE, BOT_ERROR, BOT_CANCELLED, BOT_UNKNOWN
} bot_state_t;

typedef enum { BOT_FACE, BOT_AGENT_PICKER, BOT_STATS } bot_route_t;
typedef enum { BOT_USAGE, BOT_QUOTA } bot_stats_tab_t;
typedef enum { BOT_TOUCH_DOWN, BOT_TOUCH_MOVE, BOT_TOUCH_UP,
               BOT_TOUCH_CANCEL, BOT_TOUCH_TICK } bot_touch_phase_t;
typedef enum { BOT_GESTURE_NONE, BOT_TAP, BOT_SWIPE_LEFT,
               BOT_SWIPE_RIGHT, BOT_SWIPE_UP, BOT_SWIPE_DOWN,
               BOT_HOLD, BOT_WAKE_ONLY } bot_gesture_kind_t;

typedef struct {
    bot_touch_phase_t phase;
    int16_t x, y;
    int64_t monotonic_ms;
} bot_touch_sample_t;

typedef struct {
    bot_gesture_kind_t kind;
    int16_t x, y;
    int64_t monotonic_ms;
} bot_gesture_event_t;

typedef struct {
    char agent_id[17];
    char session_key[65];   // JSON null -> 空字符串；不用于推断连接
    char run_id[65];
    char tool[25];
    char detail[49];
    uint32_t selection_rev;
    bot_state_t state;
    bool stale;
    bool progress_known;
    float progress;        // 仅progress_known=true时可用
    uint64_t run_elapsed_ms;
    uint8_t active_sessions;
} bot_focus_t;
```

其余 `quality/reason/health` 使用显式枚举并保存在model内；不得丢弃后把reported显示成observed。字段容量含C字符串终止符；所有拷贝截断/UTF-8清理在decode处完成，`strcpy`禁止用于外部字符串。

### GestureRecognizer

在 `bot_core/gesture.c` 实现，头文件在 `include/bot_gesture.h`。结构体在头文件完整声明或提供create/destroy；不能让测试使用不完整类型栈分配。

```c
void bot_gesture_reset(bot_gesture_t *g);
void bot_gesture_set_wake_only(bot_gesture_t *g, bool enabled);
bot_gesture_event_t bot_gesture_feed(bot_gesture_t *g,
                                    bot_touch_sample_t sample);
float bot_gesture_hold_progress(const bot_gesture_t *g, int64_t now_ms);
```

一个接触序列最多输出一次语义事件。HOLD用TICK在650ms触发，不等待松手；移动超12px立即取消HOLD；长按后UP不能再生成TAP。触摸驱动未报告可靠UP时用CANCEL清理，不补造点击。

### DeviceModel与路由

文件：`bot_core/device_model.c`、`bot_core/router.c`。

```c
void bot_model_init(bot_model_t *m, int64_t now_ms);
bool bot_model_apply(bot_model_t *m, const bot_message_t *msg, int64_t now_ms);
void bot_model_tick(bot_model_t *m, int64_t now_ms);
bot_ui_effect_t bot_router_handle(bot_model_t *m, bot_gesture_event_t event);
```

`bot_message_t`是经过Schema等价检查后的固定上限tagged union；不同type不共用不受控字典。`bot_ui_effect_t`描述 `NONE/POKE/SHOW_DETAIL/SEND_ACTION/WAKE` 等，不直接执行网络。每次apply失败返回false、保持原model不变并记录reason计数。

Model保存 `link_id/boot_id/handshake_id/host_seq/selection_rev/selected_agent/preview_agent/route/stats_tab/pending_action/focus/stats`。网络线程不能直接写Model；通过UI队列提交到LVGL拥有线程。

### 串口帧边界

文件：`bot_core/frame_parser.c`、`include/bot_frame_parser.h`。

```c
typedef void (*bot_frame_callback_t)(const uint8_t *json, size_t len, void *ctx);
void bot_frame_parser_init(bot_frame_parser_t *p);
void bot_frame_parser_feed(bot_frame_parser_t *p,
                           const uint8_t *bytes, size_t len,
                           bot_frame_callback_t on_frame, void *ctx);
```

允许任意分片及一包多行。遇到超长行进入discard状态直到换行，再恢复；不会为了等待换行继续realloc。`@bot `前缀5字节，JSON最多8192字节，换行1字节。普通日志只计数/限速，不送入JSON解析。

## 2. Face/Picker/Stats渲染接口

头文件：`firmware/components/bot_ui/include/bot_ui.h`。

```c
void bot_ui_create(lv_obj_t *parent, const bot_ui_config_t *config);
void bot_ui_apply_model(const bot_model_t *model, int64_t now_ms);
void bot_ui_tick(int64_t now_ms);
void bot_ui_destroy(void);
```

所有接口在BSP LVGL线程或对应锁内执行。`apply_model`比较业务状态/run/选择版本，而不是每个seq都重新启动动画。`tick`更新眼形插值、视线和动画时钟，不在其中读USB或调用HTTP。页面对象复用，Toast只有一个固定容器。

初版不承诺完整C++运行库，不把Python协议代码嵌到固件。统计格式化使用定点/整数规则；未知值由presence flag表达，不用-1/NaN充当业务值。

## 3. Mac模块接口

文件与Python包统一使用 `bridge/src/bot_bridge/`；构建为可编辑安装包，CLI入口 `bot-status`。

```python
# models.py: Pydantic v2 strict model，字段由JSON Schema一对一生成/实现
class CanonicalEvent: ...
class DeviceMessage: ...
class CapabilityReport: ...

# reducer.py
class SessionStore:
    def apply(self, event: CanonicalEvent) -> bool: ...
    def focus(self, agent_id: str, now_ms: int) -> dict: ...
    def catalog(self, now_ms: int) -> list[dict]: ...
    def mark_sources_unknown(self, now_ms: int) -> None: ...

# ledger.py
class UsageLedger:
    def ingest(self, event: CanonicalEvent) -> bool: ...
    def summarize(self, agent_id: str, start_ms: int, end_ms: int) -> list[dict]: ...
    def close(self) -> None: ...

# selection.py
class SelectionService:
    def current(self) -> tuple[str, int]: ...
    def select(self, link_id: str, action_id: str,
               agent_id: str, base_revision: int) -> dict: ...

# adapters/base.py
class AgentAdapter:
    async def probe(self) -> CapabilityReport: ...
    async def start(self, emit_event) -> None: ...
    async def stop(self) -> None: ...
    async def read_quota(self) -> list[dict]: ...
```

上述省略号仅表示Python **接口签名**，不是交付实现。返回的dict必须通过本包相应Schema/字段验证，不允许散落的额外key。

`SessionStore.apply`返回false表示重复、过期或不合规事件，不能据此阻断Agent。`UsageLedger.ingest`可独立处理usage，不要求改变face。账本由单写协程所有；SQLite操作在受控工作线程/短事务执行，不能阻塞串口心跳。

`read_quota`失败返回有availability/reason的未知对象，不能用全零覆盖最后成功缓存；缓存另保留as_of，UI以stale提示。

### Test fixture helper

测试文件 `bridge/tests/conftest.py` 定义 `event(kind, *, run_id="r1", tool_call_id=None, reason="none", source_seq=None, event_id=None, at_ms=1000, usage_record=None)`，默认合法Codex模拟事件，source_health=null，必填字段与CanonicalEvent Schema完全一致。未给event_id时由测试确定性计数生成，不能随机。所有适配器golden test使用冻结时间与固定ID。

### Host transport

`serial_link.py`拥有唯一串口句柄，`protocol.py`负责Schema/语义/序号，`publisher.py`节流与组合focus/stats/catalog。业务层只提交结构化消息，不直接写串口。

- `encode_message(message: dict) -> bytes`：验证并返回含前缀换行的UTF-8帧。
- `decode_json(raw: bytes) -> dict`：长度/深度/重复key/有限数/Schema/语义验证。
- `FrameDecoder.feed(chunk: bytes) -> list[dict]`：流式帧解析，非法帧只记录原因。
- `SerialLink.send(message: dict) -> None`：入有界队列，不等待设备UI绘制。

## 4. 测试命令约定（实施者需要创建对应工程）

```bash
# 文档包现在即可执行
python tools/validate_contracts.py

# 以下命令在T03建立相应测试工程之后使用
cmake -S tests/native -B .build/native -DCMAKE_BUILD_TYPE=Debug
cmake --build .build/native
ctest --test-dir .build/native --output-on-failure
.venv-bridge/bin/python -m pytest bridge/tests -q
```

Native C测试使用CTest与标准assert即可，不为v1额外引入大型测试框架。State/Gesture/Parser可在Mac运行，不依赖实际LVGL屏幕。真正布局/性能测试仍需SDL模拟器和开发板。
