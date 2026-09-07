# Native host API v1

Bridge binds `http://127.0.0.1:17940` only. Native application is a metadata client and cannot control source tasks. Never use the AgentKeyboard port7420.

`GET /v1/state` returns `{api_version:1, bridge:{version:"3.0.0",started_at_ms,paused,launch_at_login,demo:false},device:{connected,port,firmware,protocol_version:3,last_seen_ms,error,diagnostics},selection:{mode:"auto"|"pinned",selected_agent:"codex"|"cursor"|"hermes"|"workbuddy",selection_rev},sources:[{id,label,installed,configured,observed,running,health,last_event_ms,last_scan_ms,stale,active_sessions,capabilities,validated_coverage,gaps}],focus,stats,diagnostics:{event_count,recent_events,errors}}`.

focus and stats use exact protocol-v2 body shapes. Unavailable metrics value is null, quotas empty. `active_sessions` in API is null when unobserved; wire compatibility uses0 plus unavailable health. `running` means fresh observed active run, not a process name. Configured hooks are never observed delivery. `stale` is event age over120s. Each recent event is metadata only `{agent,session,run,kind,ts,tool,channel}`; IDs are hashes. No chat/prompt/tool arguments/results or credentials.

`POST /v1/control`, JSON Content-Type, native client without Origin, local Host exactly `127.0.0.1:17940` or `localhost:17940`, with `Authorization: Bearer <token>`. Read the token from `~/.local/share/agent-robot-face/control.token` (0600). Never log/display/copy token into bundles. Browser Origins are rejected. GET is also local Host guarded. No CORS.

Actions:
- `{action:"auto",expected_selection_rev:integer}` selects auto and recomputes focus.
- `{action:"pin",agent_id:"cursor",expected_selection_rev:integer}` pins that agent. New source events do not steal focus.
- `{action:"pause"}` releases serial ownership, persists paused state, returns only after serial closes.
- `{action:"resume"}` restores connection attempts.
- `{action:"launch_at_login",enabled:boolean}` enables/disables the user LaunchAgent for next login (does not terminate the serving request).

Success HTTP200 `{ok:true,selection,paused}`. Errors `{ok:false,error:"unauthorized"|"forbidden"|"invalid"|"conflict"|"internal_error"}` HTTP401/403/400/409/500. Selection mutations require expected revision; on conflict refresh state. App should poll GET every1–2s, show service unavailable on connection errors, never manufacture simulated data. Pause pertains to device USB; source telemetry continues. To start Bridge use installed `~/Library/LaunchAgents/com.agentrobotface.bridge.plist` with launchctl; installer owns its lifecycle.

Source capability keys: `start,tool,success,failure,cancellation,waiting,usage,quota` map to `observed|unverified|unsupported|unavailable`. `validated_coverage` lists actual externally validated channels, separately from runtime configured/delivery state. `gaps` are machine-readable reasons for missing coverage. Device diagnostics include render_updates/window_ms/avg_us/max_us and optional projection (see USB v2 contract). Render metrics measure CPU submission; projection is device-applied metadata readback for diagnosis only. Neither is a source activity input.

## Lifecycle commands for the menu application

The installed source checkout exposes the fixed executable `tools/bridge` (absolute project path is in the LaunchAgent ProgramArguments). Invoke constant argv asynchronously; no shell interpolation from user text.

- `tools/bridge install`: merge only observer entries, create user LaunchAgent, start service. It preserves existing AgentKeyboard/Memmy hooks and does not alter credentials or auto-accept Hermes consent.
- `tools/bridge repair-hooks`: idempotently restore only project observer entries; does not restart source apps. Actual delivery after config reload is still required.
- `tools/bridge doctor`: read service/source installation and sanitized diagnostic state; no mutation.
- `tools/bridge uninstall`: stop own LaunchAgent, remove exactly recorded hook entries, retain ledger/backups.
- `tools/bridge flash -- <executable> <args...>`: record current pause state, await USB release, execute firmware command, restore prior state in finally.

`launchctl bootstrap gui/$(id -u) ~/Library/LaunchAgents/com.agentrobotface.bridge.plist` starts an installed unloaded service. `launchctl kickstart -k gui/$(id -u)/com.agentrobotface.bridge` restarts it. Source config locations are `~/.codex/hooks.json`, `~/.cursor/hooks.json`, `~/.workbuddy/settings.json`, `~/.hermes/config.yaml`. Native UI can open these files or this documentation with NSWorkspace; it must not expose stored tokens. Installer backs up original files privately for transaction recovery; uninstall removes owned entries from current contents instead of restoring stale whole-file backups.

Sources also expose `synchronizing` and `backlog_bytes` for files modified within 120 seconds with more than 64 KiB unread. Identified backfilling sessions are excluded from focus/Auto/catalog candidates; a different caught-up session remains eligible. If no eligible session exists, focus is unknown with detail `syncing_history`. Whole-source API active_sessions is null during partial synchronization; running may still be true for a known eligible active session. Wire counts are observed eligible counts, not full historical coverage. Historical facts remain in the ledger. The panel shows 来源记录同步中. Files are discovered from a bounded two-day window; this is not a claim of complete history. Oversized records are discarded in bounded chunks until newline, and persistent gap `oversized_record_skipped` records potential missing event metadata.


Usage Dashboard has an independent `/v1/usage` read model and authenticated refresh. See [usage-api.md](usage-api.md). Device usage diagnostics contain independent scope/revision/level; paired serial protocol is v3, while legacy focus/stats body semantics remain unchanged.

Device diagnostics (firmware 3.2.3): `heap_diagnostics` contains uptime_ms, internal_free, largest_free, minimum_free, psram_free, screen, as_of_ms. The six device counters must be exact unsigned 32-bit integers. `firmware_errors` retains at most 20 allowlisted, bounded allocation/panic messages; the preceding boot's errors may be retained in `previous_boot_errors`. These are diagnostics, not source content. `tx_pending_bytes`, `reconnect_count`, and `reconnect_reason` expose bounded transport metadata. Missing counters are unavailable, not evidence of zero failures.

`device.protocol_rx` holds cumulative host counts for hello, pong, accepted_pong, link_rejected and seq_rejected. `invalid_protocol_json` counts malformed protocol JSON without saving payloads. `reconnect_evidence` records bounded timing and queue metadata at the most recent retry. Firmware 3.2.7 `link_diagnostics` reports rx_dropped, bad_lines, queue_restarts, timeout_restarts, tx_failed, rx_pending_bytes, rx_chunks, rx_bytes, poll_gap_max_ms and host as_of_ms. Device counters reset on boot; host counters reset with the Bridge process. Neither diagnostic channel changes business state.
