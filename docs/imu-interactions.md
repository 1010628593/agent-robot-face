# QMI8658 face interactions

## Scope and integration

The device-level motion layer adds automatic Face leveling, shake/dizzy, movement attention and settling bounce. It does not modify Agent states, trigger tools, change selection, approve requests, read credentials or alter the wire schema. Face remains two eyes on black; the SIM badge remains because the existing Agent model is simulated.

This change is based on PR #2 (`2af70b7`), not a replacement for it. Picker/Stats remain in their existing fixed orientation; their four-direction rotation and face-down sleep are **not implemented in this increment**. No fourth screen, permanent rings, captions or motion commands are added.

## Hardware contract and sources

The standard/B Waveshare 1.75 schematic and hardware reference identify **QMI8658 at 7-bit I2C 0x6B**, sharing GPIO14 SCL / GPIO15 SDA. The sensor is board-mounted; ESP32-S3 itself does not contain it. The old QMA7981 README/component are not an applicable hardware implementation.

Sources inspected:
- Waveshare `HARDWARE_REFERENCE.md`: https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75/blob/main/HARDWARE_REFERENCE.md
- Vendor schematic: https://files.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-1.75/ESP32-S3-Touch-AMOLED-1.75.pdf
- BSP public `bsp_i2c_get_handle()` declaration: https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75/blob/main/firmware/brookesia/components/waveshare__esp32_s3_touch_amoled_1_75/include/bsp/esp32_s3_touch_amoled_1_75.h
- Register/scale reference: Lewis He's MIT-licensed SensorLib `src/SensorQMI8658.hpp`, inspected blob `2a75b6d6cc9ab7eba2a9134a4805e1978ccd77e3`: https://github.com/lewisxhe/SensorLib/blob/master/src/SensorQMI8658.hpp

The QMI register implementation here is small independent C code, not an Arduino/SensorLib dependency. WHO_AM_I register 0x00 must return 0x05 **before any writes**. Polling uses little-endian auto-increment (CTRL1=0x40), +/-4g, +/-1024dps, ODR code 6 (nominal accel 125Hz / gyro 112.1Hz; six-axis clock follows gyro). No FIFO, hardware gesture, calibration command, chip reset or interrupt configuration is used. Async burst data is rejected when a second read of the 24-bit sensor timestamp differs; duplicate samples are not integrated twice. This is not a claim of cycle-perfect hardware synchronization.

## Software ownership and fault behavior

`bot_imu` owns only a sensor device handle on the BSP-owned bus. It starts after display initialization. It never creates/deletes the shared bus, changes GPIO or PMIC rails, or calls LVGL. The single worker has a 4096-byte task stack, priority 3, about 10ms polling and 10ms per-transaction timeout. Sample timing uses elapsed time, not an assumed fixed rate.

A one-slot FreeRTOS queue carries the latest filtered view, not an unbounded sample backlog. The UI peeks without waiting and converts sensor timestamps into the existing LVGL clock. Data older than 250ms disables new motion reactions/rotation while the last visual angle and ordinary Face/touch continue. Five I2C failures or one second without a usable sample ends an attempt; at most three lifetime attempts with 1s/2s backoff occur. There is no reset loop or bus recovery that interrupts touch. After final failure, reboot is required to retry.

## Estimation and interaction contract

- Rodrigues gyro prediction maintains a gravity direction in sensor coordinates; accelerometer correction is gated by norm/residual/motion, preventing a shake from being interpreted directly as tilt.
- Gyro bias is estimated only after a quiet one-second window with at least 50 samples. Keep the device still at startup. Gravity is retained, not subtracted as accelerometer bias. Six-axis data cannot distinguish every extremely slow rotation from bias.
- Screen-plane gravity enters the flat lock below 0.22g and exits above 0.35g. Near-flat devices hold their last reliable angle. This cannot infer absolute yaw or where the user is when the screen lies horizontally.
- Angular interpolation takes the shortest wrapped path, with 0.4-degree estimator deadband and 240deg/s rate limit. No 358-degree spin across +/-180.
- Three alternating linear-acceleration peaks above 0.60g inside 850ms trigger dizzy; a single bump or same-direction impulses do not. The detector also uses a 0.18g release threshold, 70ms peak refractory period and gyro-speed gate. These are **initial engineering thresholds, not measured on this board**.
- Dizzy lasts 2400ms from trigger, then returns to the current Agent pose. A 7400ms trigger-to-rearm cooldown plus 500ms quiet requirement prevents endless replay during continuous shaking. No event queue accumulates.
- Movement after >1500ms quiet produces 600ms attention. Settling for >350ms produces a 450ms damped eye bounce. These are motion heuristics, not guaranteed pickup/placement or person detection.
- WAITING, ERROR, DONE, UNKNOWN and CANCELLED suppress playful reactions. Their arrival consumes the active reaction so it cannot reappear when the Agent returns to WORKING. Events received while Picker/Stats are visible are consumed, not played late on return.

## Rendering and touch

`face_reaction.c` only composes a temporary pose. Dizzy uses moving pupils, uneven eyelids and a recovery blink, not X eyes (reserved for errors). The existing per-Agent expression clocks are not reset by a physical action.

LVGL rotates the bounded 322x244 Face surface around display center (233,233). Eye primitives, masks, smile arcs and crosses rotate together. This uses an intermediate LVGL transform layer, **not an allocation-free graphics path**; its buffer may be roughly 0.15–0.30MiB depending on the color format plus LVGL overhead. Check actual heap/PSRAM and frame rate on-device; host screenshots do not measure ESP32 throughput. Old/new transformed extents are invalidated by LVGL. The root and SIM label are not rotated.

A DOWN latches the visible Face angle until UP/cancel/page departure. Raw touch positions are inverse-rotated using that same matrix, including the final release point. Face swipe directions are interpreted in the leveled Face coordinates. Text pages retain their existing coordinates and central-confirm hit area. Motion never produces a navigation/approval command.

## macOS build and first-board calibration

Use the existing EIM v6.1 environment; do not install another Python/IDF or modify managed components.

```bash
idf.py --version
idf.py -C firmware -B build-imu build
idf.py -C firmware -B build-imu menuconfig
```

Menu: **Component config -> Bot motion interactions**.

| Option | Default | Purpose |
|---|---|---|
| BOT_IMU_ENABLE | y | Disable to retain the previous non-motion UI |
| BOT_IMU_MOUNT_DEG | 0 | Sensor gravity angle when the intended face is upright |
| BOT_IMU_REVERSE_ROTATION | n | Correct opposite PCB/sensor normal if required |
| BOT_IMU_DIAGNOSTICS | n | At most one six-axis/orientation log per second |

**Zero mount offset is a reference convention, not a verified physical axis mapping.** Before judging automatic leveling:

1. Build, retain the original Flash backup, and obtain explicit user approval before flashing. No command in this document automatically flashes.
2. Enable diagnostics for the initial check. Confirm the log reports WHO_AM_I=05 at 6B and sensible data. If identity fails, do not bypass the check or re-enable QMA7981.
3. Hold the screen in the intended upright position, rest it for at least two seconds. The accelerometer norm should be close to 1g and gyro close to zero; `bias=1` indicates the quiet calibration completed.
4. With mount=0/reverse=n, record the stable `angle` and set BOT_IMU_MOUNT_DEG to its rounded value. Rebuild only after recording the initial orientation. Set the reversal option only if rotating the enclosure makes the face turn in the same world direction rather than compensating.
5. Check upright, left/right 45 and 90 degrees, upside-down, flat and back upright. Flat should lock rather than hunt. In-plane mount offset/reversal assume the IMU is on the board plane; unexpected Z/XY behavior requires checking board revision/axis mapping, not random sign edits.
6. Test a normal tap/swipe, gentle pickup/placement and gentle repeated shake without pulling the USB cable. Do not demand violent shaking. WAITING/ERROR must remain recognizable and navigation must not trigger from motion.
7. Check low FPS, blank transform buffers, resets, sensor/touch bus contention and eight-hour stability before considering the feature hardware-validated.

The Agent simulator still advances states every seven seconds. Test playful reactions while the selected Agent is IDLE/WORKING/TOOL; no dizzy response in WAITING/ERROR is intentional.

## Reproduce host verification / actual-code preview

```bash
for suite in native motion face lvgl; do
  cmake -S tests/$suite -B .build/$suite -DCMAKE_BUILD_TYPE=Debug
  cmake --build .build/$suite --parallel
  ctest --test-dir .build/$suite --output-on-failure
done
mkdir -p .build/motion-frames
BOT_SNAPSHOT_DIR="$PWD/.build/motion-frames" .build/lvgl/test_lvgl_ui motion_preview
```

The preview emits PPM frames from actual LVGL software rendering with synthetic movement. It is not a camera recording, sensor replay, macOS test or board-FPS measurement. Clang real-LVGL sanitizer testing retains the narrow, documented upstream callback exception from PR #2; strict unmodified upstream UBSan is not claimed green.
