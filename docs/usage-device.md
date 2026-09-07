# Device usage Dashboard (USB v3)

The device boots in `current / today / page 0`. Usage browsing has its own `usage_rev`; it never sends a business selection action. A tool, period or page change sends one 32-hex `usage_request` through the existing UI-owned USB writer. The page displays pending feedback until the matching accepted ACK. A conflict rejection adopts the ACK’s authoritative browsing view/revision so the following usage snapshot and retry can recover, while retaining failure feedback and never entering the rejected target. Timeout clears pending feedback after six seconds; reconnect clears per-link usage state. Link ID and increasing envelope sequence validation apply before usage messages reach the model.

The production page uses one persistent LVGL tree. The initial overview has four tools (Codex, Cursor, Hermes, WorkBuddy), their existing monochrome artwork and thin grid dividers. Tool and dimension modes are local presentation choices. The header switches current/all summary scope; selecting a tool changes the independent usage scope and opens its list after ACK. The dimension overview offers total tokens, actual fees, quota and cache read. It opens the corresponding three-row list. Lists expose quota windows, model totals, actual fees or cache read/write. Tapping a row opens its individual detail. Today, 7-day and 30-day choices stay visible. Horizontal swipes page quota/model lists; a swipe from detail returns to its list. Downward swipe pops one level, while a root downward swipe dismisses the Stats panel to Face.

Nested Stats claims the entire contact before global panel navigation, after the existing wake-only guard. It reuses the production gesture recognizer for tap duration/slop, swipe classification, multitouch and primary-contact replacement rejection. Each accepted downward contact pops only on UP. Root panel edges continue to use the existing spatial navigation and springs. The face top-down picker entrance, bottom-up Stats entrance, continuous pet behavior, fixed palette, face geometry and IMU behavior are unchanged by this usage implementation.

Quota arcs encode **used** percentage with a single solid stage color: below 25% blue `#2563EB`, below 50% green `#22C55E`, below 75% yellow `#EAB308`, below 90% orange `#F97316`, otherwise red `#EF4444`. The progress value is the source percentage, quantized to 0.01 percentage points for the LVGL integer arc range. The 270-degree detail track and full-circle overview/list tracks use that fraction, without gradients. Reset countdowns use the dedicated bounded host clock `host_now_ms`, independently of source collection timestamps. Unknown or stale quotas retain a gray track and dash with no colored progress. Unlimited/unbounded token totals are numeric with no progress ring. Stale and unavailable source states are explicit.

Fees are source-reported actual fees in the provided ISO currency, with cost coverage and provenance on the detail. No currency conversion or token-based estimate is computed. Cache labels retain the source-specific definition and never imply that missing data is zero. The decoded model retains up to 30 dated nullable history rows; the current compact detail presents totals rather than a synthetic curve. Full history remains available to the Mac dashboard/API.

## Bounded protocol and diagnostics

See [protocol-v3.md](protocol-v3.md). The production parser uses 8192 bytes, 640 nodes and nesting depth 12. It rejects unknown fields, wrong types, non-finite/negative/out-of-range values, invalid 32-hex identifiers, incorrect fixed agent ordering and oversized arrays. Usage carries at most three quota rows, three model rows and thirty history rows; quota total is capped at 32, model total at 100 and page at 33. The paired hello/envelopes are version 3 and the firmware identifies as `3.2.7`.

`@diag.usage` optionally projects `{usage_rev,data_rev,subject,period,page,level}` using bounded numeric values and compile-time subject/period names. `level` is 0 (overview), 1 (list), or 2 (detail). This metadata is emitted by the UI owner and does not become a business fact.

## Reproducible review evidence

`firmware/tools/render_usage.c` is a standalone diagnostic based on the existing real LVGL replay harness. It links the production firmware objects and uses explicitly selected sample review data; it is not a unit test or a device capture. Its `fixture` command cannot be included in the ESP-IDF production component source list. `reports/usage-dashboard/device-render/review.trace` records the sample review and nested-back interactions; `device-render/replay.csv` reports actual navigation phase and usage depth. PNGs named `device-*.png` are actual LVGL software renders of that code, with sample figures.

`firmware/tools/decode_usage.c` feeds stdin through the production decoder and its real pool budgets. `device-wire-real.frame` wraps the Bridge's `usage-wire-real.json` fixture; `device-wire-decode.txt` records the observed decode. Compile the decoder with:

```sh
cc -std=c99 -Wall -Wextra -Werror -I firmware/components/bot_core/include firmware/tools/decode_usage.c firmware/components/bot_core/bot_frame.c firmware/components/bot_core/bot_json.c -lm -o /tmp/decode_usage
/tmp/decode_usage < reports/usage-dashboard/device-wire-real.frame
```

The legacy LVGL tests still encode retired gestures and are not an acceptance claim for touch contract 2.1. Actual hardware flash, handshake, usage projection and physical touch verification are performed by the main task after the paired Bridge and firmware build.

### 3.2.4 布局修正

工具总览采用相同的两行单元尺寸，原生 24px 单色图标，不再运行时缩放。额度详情显示“已用”，重置文本与弧线端点分离；页签选中条放在文字下方。费用直接显示主数字和独立币种，说明仍从右上角进入。切换页签先回到对应指标入口，避免额度详情直接变成第一个模型详情。左右滑动在单页时切换指标；多页列表优先翻页，越过边界后切换指标。下滑和返回逐层退出。

3.2.5 边界修正：小型总览环的百分比四舍五入显示整数，详情保留原精度；小环数字使用已内置的 20px 字体，100% 不压弧线。费用页不显示分页标签，避免从维度入口进入时出现 LVGL 默认占位文字。
