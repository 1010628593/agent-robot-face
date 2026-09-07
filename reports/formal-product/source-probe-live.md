# Live source acceptance — 2026-09-07

## WorkBuddy Desktop 5.5.3
Controller submitted a dedicated harmless terminal printf validation task through the actual WorkBuddy Desktop UI at14:55 CST. Read-only probe correlated the new desktop-created session with the WorkBuddy-owned SQLite and structured event records. This is not CodeBuddy CLI substitution and not MCP self-report.

- UI: final `已完成28s`, `验证完成`.
- Session d9de0e01-eb50-480b-a780-71dc5c9fd357, DB source_mode=craft, mode=craft, use_sandbox_cli=1.
- User event timestamp1788764104825, Bash function_call timestamp1788764119948, paired function_call_result completed1788764121204, assistant terminal completed1788764128676.
- Database observed working→completed, last update1788764133785.
- Source: `~/.workbuddy/workbuddy.db` and matching `~/.workbuddy/projects/*/<session-id>.jsonl`.
- Collected only schema/lifecycle metadata, not conversation content/tool arguments/results.
- Proven: Desktop running/tool/success. Not yet proven: permission/input waiting, cancellation, failure; these must not be inferred from a tool start or process existence.

## Hermes CLI 0.21.0
Actual oneshot CLI validation, isolated /tmp/bot-source-validation cwd. First session 20260907_150415_550f62 failed API after three connection retries. Per-process removal of inherited dead proxy succeeded without changing system/source settings.
- Successful session: 20260907_150520_ae7805, source=cli.
- user metadata timestamp1788764720.4213529; assistant terminal call1788764732.801763; paired tool1788764732.911169; final assistant finish_reason=stop1788764735.119248.
- Tool call call_94bd142867304772a7028000, category terminal. Arguments and output excluded.
- DB agent_close alone is not success: both failed and successful invocations used it. Native hook completed/interrupted and terminal message evidence required.

## Cursor IDE 3.19.16
Actual native Cursor Agents desktop UI task, composer e998b995-0a3d-4579-9e96-a8bd5d4c18d0, generation d430d32e-3dc6-42ed-8d45-525b9cebb39a.
UI showed Worked for15s / Ran1command / 验证完成. SQLite composerData ultimately completed. An earlier live query returned aborted while UI still showed active run; do not classify cancellations from that transient field. Fresh official hook delivery remains required for reliable terminal semantics.

## Codex standalone CLI 0.152.0
Actual exec invocation session01a07aae-f6c0-7630-a54c-36fbb51e3f13, turn01a07aae-f761-7952-929a-e145166f71b7. Session metadata source=exec, originator=Codex Desktop (inherited host identity), so source entrypoint is necessary to distinguish CLI/Desktop. Start observed, connection refusal retries prevented tools so far. No success claim.

### Cursor delivered Hook evidence
Actual Cursor hook log under Library/Application Support/Cursor/logs/20260907T130312/window1_wb1/output_20260907T130314/cursor.hooks.workspaceId-137ffe041e1c10b571881e48d512e15b.log delivered beforeSubmitPrompt, preToolUse Shell, afterAgentResponse, stop status=completed for the same composer/generation. Tool ID9f1c83af-5863-4938-9c52-eebbe24db52b. DB bubble3e4adef8-97fd-436e-ba7b-7cf5fb43d768 toolFormerData completed; start1788764801453/end1788764805950. afterAgentThought may suffix generation IDs; never count these as new runs.

### Hermes native finalization schema
Installed turn_finalizer.py839–855 supplies on_session_end completed,failed,interrupted,turn_exit_reason. Current preexisting hooks only on_session_start/pre_llm_call/post_llm_call; delivery of newly installed finalization hook still needs validation. Missing final assistant record alone does not classify failure.

### Codex CLI connection boundary
Second actual exec attempt with inherited proxy removed ended exit1 after stream reconnect attempts to chatgpt.com. No tool execution completed. This was an initial symptom; the final causal diagnosis and successful compatible CLI validation below supersede that provisional connection-only attribution.

### Codex CLI terminal proof
Root sent SIGINT only to its own original validation process after repeated connection refusal; rollout emitted turn_aborted reason=interrupted. Direct retry session01a07ab2-a7c4-7b61-8f3c-37e4a9fd5903 emitted task_complete with non-null error object (keys message,codex_error_info; code other) and exit1. Therefore task_complete is a terminal event, NOT success by itself. Error messages are excluded from telemetry; retain only non-null error flag and bounded code. Proven live start/user-interrupt/nativefailure; tool/success remains blocked by network.

### Cursor actual cancellation
Root clicked Stop generation during its own harmless sleep20 tool task. Conversation unchanged; generation33818f5d-2095-41a8-88d3-0002cd62c1b1, toolcf2f8953-bc94-49cd-a738-23388daa1cab. Actual hook log emitted stop status=aborted multiple times, then late postToolUseFailure, then repeated stopaborted. Deduplicate terminal; terminal cancellation must not be overwritten by tool failure arriving after stop. UI returned ContinueWorking. This proves native stopaborted as cancellation for this controlled action; generic composerDataaborted remains insufficient.

### WorkBuddy cancellation classification boundary and meta turns
Second foreground cancellation probe: user0ba17145-99d6-4cf1-ba9c-407f983a89f3 timestamp1788765347790; root clickedStop; real UI showed 已取消4s用户已取消. Assistant3dc2680c-6b34-40bc-8197-38f204ebde99 timestamp1788765352386 statusincomplete,parentId points user, providerData.skipRun=true; errorflags isNetworkError/isStreamTimeout/isRetryable allfalse. No structured enum/code independently identifies cancellation. Do not automatically equate incomplete+these flags to cancelled; report capability gap despite UI acceptance.
Next userb83827ea-eb9d-4bff-8e86-9a1869a54eac timestamp1788765355494 has providerData.isMeta=true,parentIdpoints cancelledreply. This is explicit meta trigger, not new humanprompt; subsequentTaskOutput/finalassistant belongs metaturn. Follow parentId correlation so it cannot overwrite previous cancelledrun. Allowlist these booleans only, no providerData payload/error messages. First sleep30 was returned inbackgroundandcompleted beforeStop, so didnotprove cancellation.

### Corrected Codex CLI causal diagnosis
The final direct-retry error was model-version incompatibility: installed standalone0.152.0 cannot use configured gpt-6-astra and requires newerCodex. Reconnection lines alone were misleading; do not attribute finalfailure onlytonetwork. Actualinstalled Desktopbundle /Applications/ChatGPT.app/Contents/Resources/codex reports0.153.4. Root now validates that executable via actual exec entrypoint withoutreplacingglobalCLI orchangingmodel/accountsettings. Doctor should showdefaultstandaloneversionincompatibility honestly.

### Codex compatible CLI success
Installed0.153.4 executable invoked through actual exec entrypoint completedexit0. Session01a07ac3-b014-75d0-ae5c-cfedd4321c07; turn01a07ac3-b082-71e1-8bdd-da18f7b40dc9. task_started1788766040, custom_tool_callexec call_3AU2MAgDC7dLOioLIRzlmlZt, pairedcustom_tool_call_output, task_complete1788766134 witherrornull. No arguments/outputcollected. CLI andDesktop actualentrypoints validatedseparately; defaultstandalone0.152 compatibilitywarning remains.
