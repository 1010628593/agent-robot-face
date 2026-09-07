# Current source capability evidence — 2026-09-07

| Product | Version | Actual structured source | Evidence boundary |
|---|---|---|---|
| Codex Desktop | bundled CLI 0.153.4 | ~/.codex/sessions rollouts; originator Codex Desktop, source vscode | current real task start/tool/output/end; source=vscode must not be mistaken for Cursor |
| Codex CLI | bundled executable 0.153.4; default standalone 0.152.0 incompatible with configured model | same rollouts; source=exec takes priority over inherited originator | actual exec start/tool/paired output/error-free completion validated; separate native error and user interrupt observed |
| Cursor IDE | 3.19.16 | official hooks; IDE globalStorage/state.vscdb composerHeaders and cursorDiskKV | actual IDE hook delivery start/tool/completed and user stop(aborted) validated; late tool failure after cancellation must not overwrite terminal |
| Hermes CLI | 0.21.0 | agent/shell_hooks.py native pre/post_tool_call and on_session_end | actual CLI paired terminal call/result and final stop validated; installed native start/tool/end delivery observed; canonical DB dedup validated. CLI double Ctrl-C exit130 produced no on_session_end, so cancellation classification remains unverified |
| WorkBuddy Desktop | 5.5.3 | workbuddy.db sessions plus matching project session JSONL | actual Desktop start/tool/success proven; see source-probe-live.md |

All waiting states require explicit input/approval events. Tool starts, idle timeouts and live processes do not establish waiting. WorkBuddy cancellation/error are not proven by DB completed alone. Official quota channels are currently unverified and must display unavailable.

Only allowlisted IDs, timestamps, status, tool names and lifecycle metadata enter the Bridge. No raw prompt/tool args/results/conversation/credentials. SQLite opened read-only without immutable mode against active WAL. Nested Codex subagents retain parent correlation and are excluded from root task counting.

Hermes post_tool_call extra carries task_id, tool_call_id, turn_id, api_request_id, status (ok/error/blocked), duration_ms. on_session_end carries completed/interrupted plus task and turn identity; it is turn finalization despite its name. Registry active_sessions is liveness only. Latest bounded 50 records were ACP/Desktop, not CLI.

Cursor composerData metadata has composerId/status/lastUpdatedAt/generatingBubbleIds/latestChatGenerationUUID. Rich text, conversations and encryption fields are excluded. Hook configuration alone is not delivery evidence.

## Installed observer acceptance update

Cursor fresh IDE task on 2026-09-07 16:11 delivered project-owned start/tool_start/tool_end/done hooks, generation64f79572-750c-4b9f-b9c0-b8e463960305. WorkBuddy fresh Desktop task delivered SessionStart/UserPromptSubmit/PreToolUse/PostToolUse/Stop; real transcript proved success. Stop has no status/error/end_reason/cancellation field, so native terminal classification remains unverified. No raw assistant body was retained. Hermes native receipt and sanitized queue prove installed CLI hooks executed; first60s cancellation attempt completed normally and was correctly retained as done. Second300s attempt was interrupted with doubleCtrl-C (exit130), but no native terminal hook arrived; it is not relabeled cancelled from process exit alone.

Physical firmware2.0.0 readback matched all four actual source projections (Agent/mode/revision/run/state). This validates transport and projection, not every source lifecycle. See hardware-projection.json and acceptance-2026-09-07.md.
