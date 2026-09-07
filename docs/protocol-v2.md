# Bot USB protocol v2 — implementation contract

This is the binding contract for the production firmware and Bridge. Decoder is `firmware/components/bot_core/bot_frame.c`; model is `bot_model.c`. v1 is rejected and the information page says to upgrade Bridge. No implicit v1 compatibility or SIM fallback.

Transport is USB Serial JTAG text: `@bot ` + one compact JSON object + LF. JSON max 8192 bytes, excluding prefix and LF; one trailing CR tolerated. Logs can precede/interleave complete lines. Invalid JSON, unknown keys, duplicate JSON keys, invalid enum/numeric types, oversized lines are discarded, next line resynchronizes. Integers are exact JSON integers; sequence/revision max 2147483647; timestamps max 9007199254740991. All wire strings are printable ASCII with the limits below; Chinese UI is local, raw user content must never be sent. Opaque identifiers contain sanitized identifiers or hashes, not prompts/paths/commands/secrets.

Every envelope contains exactly `v:2,type,link_id,seq,body`. Link ID is 32 lowercase hex; only hello uses null and seq0. Each direction has its own strictly increasing seq. Welcome must have seq1 and is accepted only while handshaking. Old link and duplicate/older sequence never replace snapshots. Host must resend complete catalog/focus/stats after each welcome.

Device emits hello every 2000ms while disconnected. Host chooses fresh link/epoch; heartbeat ping every 2000ms. Device echoes ping's monotonic_ms in pong with independent device seq. Device drops link after 6000ms without an applied valid frame or pending action acknowledgement. On queue overflow it drops partial data and reconnects. RX queue:32×256 bytes; up to8 chunks per UI tick; parse/model/LVGL and all protocol TX in UI owner. RX task never accesses LVGL. Queue enqueue waits at most50ms for the UI consumer; capacity8192 bytes admits a full allowed frame burst. No host serial console during firmware flash.

## Atomic selection

Selection is the triple `{mode,selected_agent,selection_rev}` where mode is `auto|pinned`, agent is `codex|workbuddy|cursor|hermes`. Welcome installs the triple. A host-originated selection message must have strictly newer revision and atomically installs all three. Focus and stats include agent/revision; older are ignored; newer are each buffered once and promoted only when selection/ACK confirms both agent and revision. Changing selection clears mismatched old focus/stats.

Device's only action is select; `mode:auto` lets Bridge choose agent according to waiting → unresolved error → retain active → recent active. `agent_id` is still required (current choice for auto, requested agent for pinned). Host must check expected revision, deduplicate action_id within link, and return ACK; UI remains pending until ACK. Accepted ACK contains actual authoritative triple. Rejected ACK retains device selection and provides retry feedback. A selection from another host control can supersede an in-flight action; late ACK cannot roll revision back. Device actions do not approve or control tasks.

```json
{"v":2,"type":"hello","link_id":null,"seq":0,"body":{"device_id":"agent-robot-face","boot_id":"11111111111111111111111111111111","firmware":"2.0.0","display":{"width":466,"height":466},"min_version":2,"max_version":2,"handshake_id":"22222222222222222222222222222222"}}
{"v":2,"type":"welcome","link_id":"33333333333333333333333333333333","seq":1,"body":{"bridge_epoch":"44444444444444444444444444444444","mode":"auto","selected_agent":"codex","selection_rev":1,"heartbeat_ms":2000,"offline_after_ms":6000,"max_frame_bytes":8192,"demo":false}}
{"v":2,"type":"selection","link_id":"33333333333333333333333333333333","seq":2,"body":{"mode":"pinned","selected_agent":"cursor","selection_rev":2}}
{"v":2,"type":"action","link_id":"33333333333333333333333333333333","seq":1,"body":{"action_id":"55555555555555555555555555555555","kind":"select","mode":"auto","agent_id":"cursor","expected_selection_rev":2}}
{"v":2,"type":"ack","link_id":"33333333333333333333333333333333","seq":3,"body":{"action_id":"55555555555555555555555555555555","status":"accepted","mode":"auto","selected_agent":"codex","selection_rev":3,"reason":"ok"}}
{"v":2,"type":"ping","link_id":"33333333333333333333333333333333","seq":4,"body":{"monotonic_ms":123456}}
{"v":2,"type":"pong","link_id":"33333333333333333333333333333333","seq":2,"body":{"monotonic_ms":123456}}
```

ACK statuses accepted/rejected; reason `ok|conflict|offline|unsupported|rate_limited|invalid|internal_error`; accepted iff reason ok. Action ID exactly32 hex. Hello device_id max64, firmware max32; boot/handshake32 hex. Welcome demo must false in production (true packets explicitly ignored).

## Catalog

Body exactly `{agents:[...]}` with all four unique agents. Each agent exactly `{id,label,state,health,active_sessions,attention_count,capabilities}`. label max16; active_sessions0..16, attention_count0..99. state: `idle|working|tool|waiting|done|error|cancelled|unknown`. health: `ready|partial|unavailable|needs_auth|disabled`. capabilities exactly `{state,usage,quota,open_agent,open_usage}`: state `observed|reported|manual|none`; usage `automatic|import|manual|none`; quota `official|import|manual|none`; open flags booleans. Production observed adapters must not substitute reported/manual sources for unavailable WorkBuddy. Unknown and unavailable remain explicit.

## Focus

Body exactly fields in this example. Strings session_key/run_id max64 (nullable), tool max24, detail max48. Use only allowlisted tool names and short sanitized metadata, never raw tool arguments/task messages. `quality`: observed/inferred/reported/manual/simulated. Production never renders simulated focus as an observed face. `reason`: none/approval/input/completed/failed/cancelled/unobserved. source_age_ms integer|null, elapsed integer≥0, active_sessions0..16, progress null or0..1. Set stale true when event evidence is expired; heartbeat does not make evidence fresh.

```json
{"v":2,"type":"focus","link_id":"33333333333333333333333333333333","seq":5,"body":{"agent_id":"codex","selection_rev":3,"session_key":null,"run_id":null,"state":"unknown","reason":"unobserved","quality":"observed","source_age_ms":null,"stale":false,"tool":"","detail":"","run_elapsed_ms":0,"active_sessions":0,"progress":null}}
```

## Stats

Body exactly `{agent_id,selection_rev,sent_at_ms,scope,metrics,quotas,sparkline}`. sent_at_ms is host Unix ms when projection emitted; enables device stale evaluation against each as_of_ms plus local elapsed. scope exactly `{kind:"today"|"session",timezone:<ASCII max64>,start_ms,end_ms}`; end>start. For Today page send kind today. Metrics0..6; quotas0..2; sparkline0..24 number|null (null means uncovered, no interpolation). Empty lists and null numeric values explicitly mean unavailable, never fabricated zero.

Each metric exactly `{key,label,value,unit,quality,coverage,source,as_of_ms,stale_after_ms}`. label≤20,source≤40. `quality`: exact/estimated/manual/unavailable/simulated. `coverage`: complete/partial/since_bridge_start/unknown. value number|null; unavailable requires null; other quality requires value and as_of_ms. as_of_ms Unix integer|null. stale_after_ms1000..86400000. Keys and units: turns→turn, tool_calls→call, requests→request, input_tokens/output_tokens/total_tokens/context_tokens→token, active_time_ms→ms, cost_usd_micros→usd_micros. Money always fixed-unit micros. Emit most useful metrics first; small display shows first three.

Each quota exactly `{id,account_key,scope,label,kind,unit,used,limit,remaining,used_pct,resets_at_ms,quality,coverage,source,as_of_ms,stale_after_ms,availability,reason,shared_with}`. id≤40,account_key≤64,label≤24,source/reason≤40. account_key is hash/sanitized opaque ID. scope account/provider/organization/local_budget. kind rate_window/credits/spend_budget/unlimited/unknown. unit percent/credit/usd_micros/token/request/none. used/limit/remaining/used_pct numbers|null. timestamps integers|null. stale_after_ms1000..604800000. availability available/needs_auth/unsupported/error. quality and coverage as metrics. shared_with0..4 unique other agent IDs (must exclude stats agent). unknown/unlimited require all numeric quota values null and unit none; unavailable quality requires all numeric quota values null. credits requires credit; spend_budget requires usd_micros. When used,limit,pct all supplied, limit nonzero and pct agrees with used/limit×100 within0.1. No inferred official quotas. Unlimited is explicit and never a zero bar.

```json
{"v":2,"type":"stats","link_id":"33333333333333333333333333333333","seq":6,"body":{"agent_id":"codex","selection_rev":3,"sent_at_ms":1788739200000,"scope":{"kind":"today","timezone":"Asia/Shanghai","start_ms":1788710400000,"end_ms":1788796800000},"metrics":[{"key":"turns","label":"Turns","value":null,"unit":"turn","quality":"unavailable","coverage":"unknown","source":"codex_adapter","as_of_ms":null,"stale_after_ms":60000}],"quotas":[],"sparkline":[]}}
```

## Notice

Body exactly `{notice_id,agent_id,run_id,kind,label,expires_in_ms}`. notice_id≤64,run_id≤64|null,label≤40; kind waiting/done/error/quota_low; expires500..10000. Reserved mailbox does not grant task-control behavior. Product primary attention is driven by focus state.

## Optional diagnostic side channel

When CONFIG_BOT_IMU_DIAGNOSTICS is enabled, the UI render callback publishes a one-slot measurement mailbox, drained by the UI timer through the same USB TX owner approximately every5s. Line example: `@diag {"render_updates":118,"window_ms":5020,"avg_us":9200,"max_us":17300}` followed by LF. These are actual dirty render submissions and CPU submission durations, not panel scanout FPS. Bridge may collect this bounded metadata for diagnostics but must never project it as source activity. @diag has no v2 sequence and no effect on link liveness. UART owns normal ESP logs; USB secondary console is disabled to prevent concurrent writers.

### Optional applied-device projection readback

`@diag` may additionally contain `projection`, a read-only device snapshot captured by the same UI timer/TX owner. Example:

```json
{"render_updates":118,"window_ms":5020,"avg_us":9200,"max_us":17300,"projection":{"agent_id":"codex","mode":"auto","selection_rev":7,"focus_selection_rev":7,"state":"working","run_id":"0123456789abcdef0123456789abcdef","screen":"face"}}
```

Prefix this JSON with `@diag ` and end with LF. projection fields: agent_id uses the four agent IDs; mode auto/pinned; selection_rev is the device's applied authoritative revision; focus_selection_rev is the applied focus revision or null when no focus exists; state is the actual UI-projected semantic state (idle/working/tool/waiting/done/error/cancelled/unknown), including disconnected/stale suppression, rather than a copy of incoming focus state. run_id is the applied focus ID only when exactly32 lowercase hexadecimal characters and has_run_id is true; every other value becomes JSON null. screen is the actual face/picker/stats screen. All serialized strings are fixed enum allowlists or the validated hash; no source text, arguments or commands are included. Absent projection remains valid for older firmware. Bridge may retain this only as diagnostic evidence for agreement checks; it must never feed business state or selection back from this readback. Incoming @bot v2 schema is unchanged.
