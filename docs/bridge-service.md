# Independent macOS Bridge

Production runtime is `bridge/src/bot_bridge`, launched through `tools/bridge serve`. USB is exclusively owned by one asyncio coroutine. It opens with DTR/RTS false, negotiates protocol2, uses a new link epoch and full snapshots on every reconnect, rejects stale links/actions/sequences, and closes the port while paused. `@diag` lines are sanitized measurement metadata only. Production never injects simulator events.

## Setup and lifecycle

Use Python3.13.12 and independent `.venv-bridge`; current environment already has required packages. To rebuild, run a trusted local Python3.13 interpreter with `-m venv .venv-bridge`, then `.venv-bridge/bin/python -m pip install -r bridge/requirements.lock`. The existing venv's base interpreter is under the local WorkBuddy-managed Python installation; removing that interpreter requires recreating the venv first. Packages and service execution are isolated; Bridge does not run inside a source-agent process or use its credentials.

From project root:

```
tools/bridge doctor
tools/bridge install
tools/bridge repair-hooks
tools/bridge state
tools/bridge pause
tools/bridge resume
tools/bridge pin cursor
tools/bridge auto
tools/bridge flash -- /path/to/authorized/flash-command arguments
tools/bridge uninstall
```

Install merges official observer hook entries while preserving unrelated integrations. JSON entries are structurally deduplicated. Hermes YAML receives only marked command-list entries, with executable private wrappers, preserving other text. Private timestamped backups and an exact owned-entry manifest support rollback/uninstall. Installer never sets Hermes global auto-accept, changes credentials or disables security hooks. First-use native observer trust may need source UI confirmation; configured is distinct from delivered. Uninstall stops/removes only own LaunchAgent and hook entries; the private ledger and backups remain for diagnosis. Repair is idempotent and does not restart source applications. Installer file changes roll back on merge failure; LaunchAgent startup failure is surfaced for repair.

`tools/bridge flash` first awaits API pause acknowledgement and closes USB, then runs the supplied authorized command with argv semantics, then restores previous pause state even on failure. If Bridge is unavailable, wrapper refuses rather than assuming serial was released.

## Event ledger and source boundaries

SQLite WAL persists hashed source/session/run/call identities, sanitized categories, time, counters, cumulative baselines, provenance, cursors, selection and pending attention. No raw messages/prompts/tool arguments/results/credentials enter ledger or diagnostic API. Source records are decoded in process, allowlisted, then discarded. File cursor context contains only lifecycle IDs and offsets. Readers use recent date directories, a200-file discovery cap, per-poll80ms budget,100records per file and round-robin progress. Source coverage is always partial. Active record parsing is bounded2MiB; oversized records are skipped, never retained. Cursor native log fallback is bounded256KiB windows,30records per poll. Source errors remain machine-readable gaps.

Codex: native current rollouts distinguish source exec from vscode; originator alone is insufficient. Nested source objects/parent threads are excluded. task_complete with non-null error means failure. turn_aborted is cancellation. Tool call/output IDs deduplicate. Token totals use cumulative deltas; no official quota is inferred. Desktop and current bundled CLI were independently validated; old global CLI0.152 may fail newer model compatibility.

Cursor: official native hook generation+conversation+tool IDs; stop completed/aborted classify success/cancellation. postToolUseFailure does not rewrite terminal cancellation. Native Cursor hook-log metadata fallback uses the same IDs and deduplicates cross-adapter deliveries. afterAgentThought suffix IDs are ignored. Generic composer DB aborted status is never treated as cancellation.

Hermes: read-only SQLite selects source cli and excludes child sessions; user message ID identifies root turn. Paired tool metadata and final finish_reason stop prove tool/success. Native shell hook turn IDs map to matching CLI DB human-message identity; on_session_end completed/failed/interrupted flags provide terminal evidence. DB agent_close alone never means success. Gateway/ACP excluded. Native hook receipt is separate from business semantics and still needs post-install validation.

WorkBuddy: actual Desktop structured JSONL parent graph; human user event starts run, isMeta notifications do not create human turns or rewrite their ancestors. Paired function call/results and completed terminal assistant prove activity/success. Incomplete assistant means unknown terminal, not guessed cancelled/error. Candidate native observer hooks currently emit only bounded sanitized diagnostic field names/IDs/status booleans for live validation; no unverified hook business mapping. Quota, waiting and ambiguous cancellation remain gaps.

## Focus and accounting

Within each session, newest run by start time owns current state; late old terminal cannot replace it. Each run keeps call states: Aend cannot clear Bworking or Bapproval. Terminal proofs win over late tool completions. Auto uses explicit pending waiting, then pending unresolved error, retains active agent, then chooses a recent active agent. Equal-priority attention retains current representative; stable task ties use start time and hashed identity. Manual pin stays on the agent. New turn resolves previous same-session attention; waiting resolves only when its own call resumes/ends or run terminates. Historical backfill creates no new pending attention; pending state survives service restart. No timeout is interpreted as success/failure/cancellation.

Active events older120s have uncertain liveness and show stale; this is an explicit observation gap. Terminal done/error/cancelled remains a valid known historical state until superseded, regardless of event age. Reader/process availability is reported separately. No claim installed source implies running task.

Daily counters cover observed root starts/calls only, marked partial. Cumulative usage establishes a baseline on first sample and at midnight; only subsequent same-day deltas enter today. Counter reset establishes a new baseline. Missing usage stays null; measured zero delta is true0. USD is converted once into integer microdollars. Quotas remain unavailable until an official channel is proven. No full-day total, active duration or request count is fabricated from incomplete data.

## Verification

`.venv-bridge/bin/python tools/bridge-integration.py` runs isolated ledger/adapters/PTY/installer/API scenarios and requires local socket permission for the ephemeral port17941. It never reads production source files or connects physical hardware. API docs are [host-api.md](host-api.md). Existing v1 contract models remain for historical fixture checks only; production USB is the independent strict v2 parser. There are no new unit tests.
