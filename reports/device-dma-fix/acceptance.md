# 设备卡死与重启修复：3.2.3

## 直接证据与修复

3.2.0 卡住时，设备反复回传 `spicommon_dma_setup_priv_buffer: Failed to allocate priv TX buffer`（before.json）。BSP 默认 50 行 PSRAM 绘制缓冲，每块 46,600 字节；当前 SPI 配置必须为它临时分配内部 DMA 副本。实测最大连续内部空闲块只有 31,744 字节，不能满足该申请。

构建时派生 BSP 编译单元，改用两块 16 行内部缓冲，每块 14,912 字节；不改 managed_components，不改变界面分辨率和抗锯齿。避免每次 SPI 传输临时申请大块内部内存。

旧 Bridge 的 250ms 同步写超时会关闭、重开串口。采集到 Write timeout、heartbeat_timeout，以及 USB_UART_CHIP_RESET。改为单一发送线程中的 32KiB 有界队列，每轮最多非阻塞写 2048 字节，保留部分写剩余数据；用量请求只回 ACK 和用量投影，减少重复目录、关注、统计帧。

增加有界 @heap 诊断和设备错误记录。LVGL 断言改为 abort 以保留可诊断崩溃，未发现断言是本次根因。

## 已完成验证

- ESP-IDF 编译、实机烧录、写入哈希校验通过。
- 现有 Bridge 协议和 API 集成通过；PTY 驱动实际非阻塞发送泵。
- 部分写及暂时零字节写验证：三帧顺序与完整字节保持一致。
- 无调试器连接的 3.2.3 实机回放：用量总览、Cursor、额度详情、Token、费用、返回脸页；8 轮。
- soak-runtime.json：62 个采样，uptime 25041 至 155213ms，1 个 boot_id，0 次断线采样，0 条设备错误；内部空闲 50,739–50,855 字节，最大连续块始终 31,744 字节，未逐轮下降。
- soak-overview-runtime.json 早期回放未真正进入详情，不计入详情验收。soak-details-before-transport.json 保留修复传输前重连证据。
- 正式生产构建 BOT_DEVICE_SOAK=OFF，3.2.3 已烧录，Bridge 恢复在线。production-runtime.json：启动后 80 秒内部空闲 71,727 字节、错误为空、TX 队列为空。
- 生产固件 SHA256：dd76e4bde0ef49de6ea4ffe0d1e7d2949f3168631b8ce76a96a188049fc8b2bf。

## 验收边界

本轮证明短时重复真实页面渲染没有复现原 DMA 错误及重启，不代表长时间运行已完全验收。CPU 绘制提交记录不是显示 FPS；切页峰值仍约 102–114ms。实际连续触摸、滑动流畅度等待用户反馈。未将构建通过当作视觉验收，也未将整体正式产品标记完成。

## 用户实机反馈

正式 3.2.3 恢复后，用户按“进入用量 → 点环／左右切页 → 返回脸页”体验并反馈：**不再卡死，操作明显改善**。本轮卡死修复取得用户实机确认；长期运行和进一步切页性能优化仍保留边界。

正式运行补充：production-followup.json 的 9 个连续采样保持同一 boot_id、无固件错误；但它与 production-runtime.json 的 boot_id 不同，Bridge 累计记录一次 `TimeoutError:heartbeat_timeout`（随后恢复）。因此不可据用户改善反馈宣称生产运行零重启或连接完全稳定。原 SPI 分配错误未复现，剩余单次心跳重连根因尚未确定。
