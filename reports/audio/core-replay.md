# Pure C environment audio replay

2026-09-08. This is deterministic synthetic-input diagnostics, not physical microphone, codec lifecycle, detection precision/recall or display acceptance. No unit-test suite was added. The executable prints measurements; review the output rather than treating exit zero as a semantic assertion.

## Reproduce

Run from repository root:

```sh
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -I firmware/components/bot_audio_core/include firmware/components/bot_audio_core/audio_features.c firmware/components/bot_audio_core/audio_detector.c tools/audio_core_replay.c -lm -o /tmp/audio-core-replay
/tmp/audio-core-replay
```

Compiler completed without warnings. Replay completed without ASan/UBSan findings. Output:

```text
calibration state=3 rms=0.000167235 floor=-75.5295
onset 0 event=1 id=1 beat=1 locked=0 period=0
onset 1 event=1 id=2 beat=2 locked=0 period=0
onset 2 event=1 id=3 beat=3 locked=0 period=0
onset 3 event=1 id=4 beat=4 locked=0 period=0
onset 4 event=1 id=4 beat=5 locked=1 period=500
onset 5 event=0 id=4 beat=6 locked=1 period=500
lost lock=0 retained=0
sustained=1 event=4
quiet sustained=0 event=2
negative rail peak=1.00751 quality=1
short available=0 quality=2 state=2
bpm=60 locked=1 period=1000 beat=6
bpm=90 locked=1 period=670 beat=6
bpm=120 locked=1 period=500 beat=6
exceptional locked=0 event=1
bpm=150 locked=1 period=400 beat=6
bpm=180 locked=1 period=330 beat=6
unstable state=2 low_confidence=1
restart epoch=21 available=0 event_id=0 beat=0
```

States 2/3 mean CALIBRATING/RUNNING. Events 1/2/4 mean LOUD/QUIET/SUSTAINED. Quality bits 1/2 mean CLIPPING/GAP. First stream starts near UINT32_MAX and crosses wrap during calibration. Input has a +1000-code DC offset and a small 800 Hz baseline. Filtered peak can briefly exceed 1 during a full negative step; the output remains finite and clipping is flagged from safe int32 raw magnitude.

The fifth onset locks rhythm; retained higher-priority LOUD may mask its RHYTHM edge, while beat_seq and rhythm_locked remain independent. Rhythm timing allows one 10 ms analysis hop at 333/1000 ms boundaries: the 180 BPM replay estimates a 330 ms median. Exceptional-onset contrast uses >12 dB above established onsets and >-24 dBFS to break the lock; it is not acoustic loudness classification.

Core stores mono PCM only in a bounded 441-frame high-pass window and clears it on gap/restart. Service must supply trusted capture timestamps, independently verify active_mics, clear core on shutdown, and set hardware supported/verified/error fields. Stable input calibrates after at least 2 seconds; persistently unstable input remains CALIBRATING with LOW_CONFIDENCE after 8 seconds. Suppression clears interactive evidence without emitting false QUIET, and freezes background aggregation. Hardware timing, overflow handling, shutdown and sustained-memory budgets require separate device evidence.
