# G0 音频契约：源码事实与待验证项

状态：**未通过实机 G0；禁止把构建成功或探针存在当成验收通过。**

本文件替换此前“核验完成”的不准确结论。正常服务仍受
`CONFIG_BOT_AUDIO_G0_VERIFIED=n` 默认门禁约束；G0 开发探针是唯一绕过此门禁的路径。
本文没有新的实机输入、停止、资源或稳定性测量结果。

## 1. 锁定来源与实现选择

依赖基线不变：Waveshare BSP `3.0.1`，Espressif `esp_codec_dev 1.5.11`。
`managed_components` 保持由组件管理器管理，没有直接修改或复制整个上游组件。

采集和生命周期采用仓库内 `firmware/components/bot_audio/bot_audio_port.c` 的窄范围应用适配层。
DMA 释放另有 `firmware/audio_dma_clear.cmake` 的源码派生补丁；两者均不修改上游目录：

| 来源 | 可核验源码事实 | 本地适配 |
|---|---|---|
| BSP `esp32_s3_touch_amoled_1_75.c:bsp_audio_init` | 私有静态双向 I2S 与 data_if；无匹配释放 API | 不调用 BSP audio init，不触及其句柄 |
| BSP 公开头文件 | MCLK=42、BCLK=9、WS=45、DIN=10；公开 `bsp_i2c_get_handle` | 引用 BSP 宏，借用已初始化 I2C 总线 |
| codec `platform/audio_codec_data_i2s.c` | 包装层丢弃 `bytes_read`，使用内部固定超时 | 直接 `i2s_channel_read(..., &bytes_read, timeout_ms)` |
| codec `esp_codec_dev.c` | 设备 close 无法提供可靠 disable 失败传播 | 原生 I2S disable/delete 逐项检查 |
| codec `device/es7210/es7210.c` | 公开 codec_if 能设定采样和增益；默认 MIC1+MIC2 | 明确只配置 MIC1，固定 30dB；物理有效输入数未验证 |
| codec `platform/audio_codec_ctrl_i2c.c` | ctrl close 不传播移除 I2C device 失败 | 静态私有 ctrl_if，直接持有本端 I2C device 并检查移除结果 |
| codec `es7210_stop` | 寄存器停机序列；close/enable 有缓存状态 | 本地保留同一窄停机序列并检查所有写入；覆盖构造器部分失败 |

停机寄存器序列来源版权为 Espressif Systems (Shanghai) CO LTD，Apache-2.0；其来源及差异
在 port 注释中记录。序列仅写 ES7210 的麦克风电源/模拟/时钟寄存器，不写 PMIC、PA、显示或 IMU。

## 2. 所有权与采集契约

- 仅服务 owner 调用 port；UI 只读取服务快照。port 不提供跨线程并发 close/read。
- 固定控制器 `CONFIG_BSP_I2S_NUM`，一次原子申请 TX 与 RX，若 BSP 已持有任一半则失败。
  TX 仅保留 REGISTERED 状态，不初始化、不使能、不驱动 DOUT；RX 为 STD Philips master。
  这一策略保留整控制器排他性，同时避免私自删除 BSP 的共享句柄。
- 本端仅拥有 RX、TX 保留句柄、ES7210 codec 和一个共享总线上的 I2C device。
  永不删除 BSP I2C bus；不创建扬声器 codec，不开启功放，不重新配置 PMIC。
- 配置为 22050Hz、16bit、单通道、MIC1、30dB。其它格式明确返回不支持。
  `configured_mics=1` 不等同于 `active_mics_verified=1`。
- read 输入字节数必须为偶数；真实 `bytes_read` 在成功、超时、其它错误下均保留。
  10ms 为 220/221 帧交替（440/442 字节），20ms 分析窗为 441 帧（882 字节）。
- DMA 每块 220 帧，共 6 块；它与服务 220/221 帧读取边界不同是允许的。
  内存预算必须实测，包含驱动、DMA 描述符和缓冲、codec、任务栈与检测历史，不能再称总计约 7KB。
- 每次成功 open 增加 generation。首次 DMA 完成时间减去该块时长建立采集起点；
  后续按实际读取帧数推导 `capture_end_us`，不会将旧 PCM 的出队时刻冒充采集时刻。
  时间仍含首次 ISR 延迟，需 G0 测量物理时钟误差。
- DMA queue overflow 使用 ISR 回调计数。发生溢出后，消费帧数与采样序号的映射不可证明，
  因而时间置零并置 `clock_uncertain/gap`，直到关闭重开。服务需清历史并有限重试。
  错误/短块以及错误后的首块均标记 gap；首次读取标记 reconfigured。
- close 先尝试关闭 ADC，再无条件尝试 disable RX；发生任何未确认失败保留 owned 状态供重试，
  不把半关闭报告为 DISABLED。全部检查通过后才释放 codec、I2S 和本端 I2C device。
- 音频开启构建使用 `audio_dma_clear.cmake` 派生 IDF I2S 翻译单元：在
  `i2s_free_dma_desc` 清零 `buf_size` 前保存大小，对每个已分配 DMA buffer 执行 volatile
  逐字节清零后才 free，覆盖尚未完成首次 DMA 的缓冲；分配失败回滚也覆盖。
  修改后的清理对该固件内全部 I2S DMA 生效。ESP32-S3 内部 DMA 没有外部缓存同步问题；
  上游带内部 L1 缓存的平台保留 C2M 同步分支。正常 port 先成功 disable 再 delete，
  IDF disable 等待 read 退出并停止 DMA；port 没有并发 reader。
  探针与服务分别清理自己的 PCM。源码保证与实机关闭/残留验收仍分别记录。

补丁锁定本机 ESP-IDF v6.1 的 `components/esp_driver_i2s/i2s_common.c` SHA-256：
`7bfc9633a327eb2b04b9c0b9d8c97bc387215ed32e9c3a19a0ca84f911b0a362`。
配置阶段同时核验整个源文件 hash、替换锚点、恰好一个目标源文件；上游变化使音频构建失败，
需要人工重新审查。衍生文件仅生成于构建目录，保留上游 Apache-2.0 版权头。音频关闭构建不打补丁。

## 3. 开发探针的可信边界

`CONFIG_BOT_AUDIO_G0_PROBE=y` 是开发固件，必须同时开启 `BOT_AUDIO_ENABLE`。
探针必须使用 `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y` 作为主控制台；普通固件默认 UART
且 G0 不启动 bot_link，不能借 USB link 输出 ESP_LOG。此配置仅用于独立探针构建，不能修改正常固件。
探针直接调用 port，独立于检测器、表情和 UI：

1. init/open 实际资源，记录内部内存、DMA-capable 内存、PSRAM。
2. A 阶段安静约 2s，B 阶段操作人员说话/拍手约 4s。
3. 只对真实 `bytes_read/2` 帧统计 RMS/peak；负满幅使用 int32；每块清理 PCM。
4. 记录成功、失败、短块、全零、削顶、gap 和读耗时；不再使用尾部哨兵估算长度。
5. 执行完整 init/open/read/close/deinit 100 次；单次失败计数，停止失败会中止后续循环，
   不创建第二个实例。每次记录返回值、真实长度和关闭后的堆数据。
6. `reopen_ok` 仅在 100 次全流程成功时为真；`responsive_candidate` 只提供人工判断线索。
   最终 verdict 永远为 `operator_review_required`，不自动批准 G0。

抓取工具必须显式提供构建目录与端口；先执行 `--validate-only` 可仅检查元数据，
执行 `--flash` 才烧录。例如：

```sh
tools/g0_probe.sh --build firmware/build-g0 --port /dev/cu.usbmodem123 --validate-only
tools/g0_probe.sh --build firmware/build-g0 --port /dev/cu.usbmodem123 --flash
```

工具验证 sdkconfig JSON/header 的音频/探针/USB Serial JTAG 主控制台开关、项目/芯片/构建目录、镜像内当前探针字符串、
镜像时间和 flasher metadata；不假定分区偏移。烧录后使用 no-reset，在日志接收器就绪后复位，
不再先运行探针再等待两秒抓日志。USB 重枚举时立即重开，若缺少 A/B 或最终摘要则 capture_complete=false。
默认实时输出串口内容并抓取 180 秒，足够容纳 100 次生命周期；可显式调整到 15–600 秒。
每次产生独立日志及 SHA-256/配置/镜像/结果 JSON sidecar，不覆盖旧日志；抓取完整不代表 G0 通过。

探针无法证明服务的 100 次用户切换、并发请求、失败退避、UI 动画延迟或 8 小时运行。
这类验收必须另行以正常服务构建执行。

## 4. 尚未验证的发布门槛

| 项目 | 当前状态 | 需要证据 |
|---|---|---|
| MIC1 实际输入、左右时隙与幅度 | 未验证 | 实机已知信号、安静/近场对照、无全零/交织异常 |
| RX-only 时钟 | 未验证 | 实测 MCLK/BCLK/WS 与有效 PCM；确认 TX 保留不影响接收 |
| read 超时/部分返回 | 源码已透传；实机未验证 | timeout=0/短超时/正常读取返回值和耗时 |
| 溢出、采集时刻 | 源码有标志；实机未验证 | 人工让 worker 延迟、核对 overflow/gap 和复位后的时间 |
| 真停止与隐私 | 未验证 | STOPPING/FAULT 失败分支；成功后时钟/采集停止和 DMA 残留检查 |
| 资源释放 | 源码所有权闭合；实机未验证 | 100 周期日志，预热后内部/DMA/PSRAM 无持续下降 |
| shared I2C 无损 | 未验证 | 开关期间触摸、IMU、显示仍工作，I2C 故障注入和恢复 |
| 运行性能/长期稳定性 | 未验证 | CPU、帧 p95、栈余量、8h；不能借用其它功能报告 |

G0 输入、时钟、读取、关停及资源关键项取得实机证据后，才可在该板卡的运行验收构建中开启 `BOT_AUDIO_G0_VERIFIED`，并通过设备面板显式开启采集。性能和 8 小时运行验收完成前仍不得标为发布通过。
普通构建、探针构建成功以及 G0 源码审查均不改变本文件的“未通过”状态。
