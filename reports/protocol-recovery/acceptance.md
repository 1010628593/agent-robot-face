# 3.2.7 USB 接收丢块修复

## 根因证据

3.2.6 增加设备 @link 和主机协议计数后，device-counters.json 捕获 rx_dropped、bad_lines、queue_restarts 增长；主机收到的 pong 均正常解析，超时前仍有新 @heap 数据。故不能把本问题归为 UI 死锁或先前的 SPI DMA 内存分配失败。

原接收队列有 32 个槽，每槽最多 256 字节，但每次 USB read（可能很短）都占一个槽。ESP-IDF usb_serial_jtag_read_bytes 实际调用 xRingbufferReceiveUpTo，只返回当前可用片段，不保证填满 256 字节。新诊断实测 rx_bytes / rx_chunks 约 62 字节；原队列常态有效容量约 2KB，UI 绘制期间最长 197ms 不处理队列，足以触发丢块与帧截断。丢块使设备重新握手，后续心跳超时的串口重开又可能重置 USB 设备。

仅将主机每轮发送从 2048 改为 512 字节仍出现丢块（paced.json），不能单独视为修复成功。

## 修复

- 保留主机 512 字节非阻塞发送节流和 32KiB 有界发送队列。
- 设备接收改为 8192 字节 FreeRTOS stream buffer，容量不再受小分片占槽影响；一个 RX task 写入、UI 线程读取，仍不从串口线程操作 LVGL。
- 每个 UI poll 最多处理 2048 字节。实际溢出仍丢弃当前残帧并重新握手，不假在线。
- 保留脱敏协议、队列与超时计数；不收集提示词、工具参数和消息正文。
- 3.2.5 用量布局及前面的 DMA 固定内部缓冲继续保留。

## 验证

现有 native 编译、Bridge 协议/API 集成通过，没有新增单元测试。ESP-IDF 编译、诊断固件烧录哈希校验通过。

stream-buffer.json 是与失败组相同设备、真实 Bridge 数据、自动用量回放的修复后记录。设备启动 120315ms 时累计 rx_chunks=6476、rx_bytes=404847；rx_dropped=0、bad_lines=0、queue_restarts=0、timeout_restarts=0、tx_failed=0，最大 UI poll 间隔 197ms。没有把 CPU 提交耗时当作屏幕 FPS。

测试是有时限的压力验证，不能代表无限期稳定。最终生产构建关闭 BOT_DEVICE_SOAK；用户对最新用量布局的实机视觉验收仍待反馈。整体四源正式产品不能据本项通过标记全部完成。

最终统计：115 个采样、1 个 boot_id、0 个断线采样、0 个固件错误采样。启动 170315ms 时累计收到 577398 字节、9231 个读取片段，设备丢块／坏帧／队列重握手／超时重握手／发送失败均为 0。生产 3.2.7 已烧录并通过写入哈希校验，BOT_DEVICE_SOAK=OFF；production.json 确认正式版本在线且链路错误计数为 0。
