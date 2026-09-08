# Audio service lifecycle replay

2026-09-08. A standalone scripted diagnostic compiles the actual `firmware/components/bot_audio/bot_audio.c` and pure C core against a pthread-backed FreeRTOS shim and a fake port. The shim replaces task creation, notifications, critical sections and delays. It does not substitute the service state machine. No unit-test framework was added.

Run from repository root:

```sh
sh tools/audio_service_replay/run.sh
```

Strict compilation (`-Wall -Wextra -Werror`) and AddressSanitizer/UndefinedBehaviorSanitizer completed without findings. Actual output:

```text
start_stop cycles=100 workers=1 opens=100 closes=100
duplicate_start stable_epoch=1 opens_delta=0
config_change epoch_advanced=1
off_during_open stop_timed_out=1 eventually_disabled=1
read_timeout attempts=3 fault=1
cleanup_failure rejected_off=1 no_second_open=1 workers=1
final closed=1 balanced=1 lifecycle_violations=0 script_failures=0
```

The fixed sequence performs 100 owner-confirmed start/stop cycles, duplicate start requests, a configuration change, a held-open operation interrupted by OFF, three failed read attempts, and repeated cleanup failure followed by recovery. Fake open is explicitly held during the OFF race: stop first times out in STOPPING, then becomes DISABLED after release and cleanup. While cleanup is failing, a renewed start request cannot allocate a second capture or create a second worker. Script failures and ownership violations produce nonzero exit.

This confirms those replayed service behaviors under host pthread scheduling. It does not establish FreeRTOS timing bounds, allocation-failure behavior, actual ADC/I2S shutdown, DMA memory erasure, acoustic performance, or long-duration hardware resource stability. The persistent worker remains asleep at process exit, matching its intended firmware lifetime. Fake PCM is zeroed synthetic input only. Hardware verification remains a separate acceptance gate.

Source review confirms revision-qualified publication, stop acknowledgement and the atomically returned accepted revision resolve the reported races and duplicate-start restart. No remaining lifecycle correctness issue was identified in that source review; the replay and physical-evidence limits above still apply.
