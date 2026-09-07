# Gravity / motion support — corrected hardware reference

The earlier version of this file incorrectly claimed the 1.75-B used QMA7981 and that continuous rotation was already enabled. The board schematic identifies **QMI8658 (0x6B)**. The historical code in `qma7981.disabled/` is retained for history only; it is not built and must not be enabled for this hardware.

The new optional service is `components/bot_imu/`, with a pure-C estimator in `components/bot_motion/`. It reuses `bsp_i2c_get_handle()`, verifies WHO_AM_I=0x05 before configuring the sensor, and delivers bounded latest-only updates to the UI. `BSP_CAPS_IMU=0` does not need to be edited: this application implements the sensor integration separately.

See [QMI8658 interactions and safe calibration](../docs/imu-interactions.md) for the exact profile, ownership, fallback, known limitations and verification commands. Continuous rotation applies to Face only. Actual board identity, mounting angle, compensation sign and motion thresholds still require local verification. No physical validation is implied by a successful host test or cross-build.
