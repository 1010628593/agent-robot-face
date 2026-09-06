# 06 · ESP32 固件技术设计

## 1. 基线

设备是标准1.75-B，不是1.75C。官方资料列出CO5300/QSPI显示和CST9217/I²C触控，16MB Flash、8MB PSRAM；源码与示例从标准1.75仓库取得。[S01][S02][S03]

第一步编译官方 `examples/esp-idf/02_lvgl_demo_v9`；实机通过后复制为自己的工程，保存vendor commit和依赖lock。目标使用用户已有ESP-IDF v6.1；v6.1不是供应商当前指南明确列出的验证线，编译/上板是硬门禁，不能凭源码下载完成就称兼容。

`main.c` 原示例通过 `bsp_display_start()`、display lock启动LVGL示例。沿用该初始化，先替换演示UI，不同步更换总线频率、屏幕驱动、PSRAM模式或PMIC设置。[S03]

## 2. 初始化次序

1. 初始化最小日志/错误记录与NVS（按BSP实际需要，不自行擦全NVS）。
2. 校验板型与编译target，输出固件版本与build hash。
3. 使用官方BSP完成供电、显示、触控初始化。
4. 在display lock内创建UI根对象、Face、Picker、Stats缓存视图。
5. 初始化device_store为offline/unknown；启动UI定时器与手势层。
6. 初始化USB协议并发hello。
7. 根据收到的welcome/catalog/focus/stats更新mailbox。

无Agent连接时UI仍自然工作并清楚显示离线。不同子系统失败需可区分：屏幕初始化失败不能假装只是Bridge离线。

## 3. 单一显示所有者

LVGL默认不保证线程安全。[S06] 本项目由BSP的LVGL任务/锁管理UI；定义一个33ms的LVGL timer读取mailbox并推进参数。其他任务只能写定长数据。不要另起第二个 `lv_timer_handler()` 循环。

`device_store`应用数据时只交换受控结构；网络解析不持有display lock。动画timer中禁止串口读写、SQLite、等待ACK或同步文件I/O。

## 4. USB Serial/JTAG

原生USB是默认通道，不写TinyUSB HID，不模拟键盘，不开启USB文件系统。[S05]

工程选择IDF USB Serial/JTAG驱动的一套read/write接口，**不要同时使用stdin和direct driver作为两个接收者**。实施者应对照v6.1 API选择 `usb_serial_jtag_*` 驱动。

运行态协议统一由TX task写入；ESP日志若重定向到USB，也必须经过同一串行化队列，前缀 `# `。推荐协议占用USB driver，默认console重定向UART0，应用日志通过自定义有界sink送TX；ROM启动噪声由Host忽略。不要让默认VFS日志与direct write抢同一端点。

日志策略：WARN默认，info每秒上限20行，每行最多256B，丢弃计数可诊断。日志不能影响ack/focus。开发时可以只运行monitor；联动时Bridge独占端口。

## 5. 内存与渲染预算

一张RGB565整帧：466×466×2 = **434,312字节**；双整帧约848KiB。8MB PSRAM并不意味着全部可当DMA内存或任意分配，保持官方buffer策略，先量测再优化。

起步以局部重绘为主；若需要自选partial buffer，可试2×48行RGB565，约89,472B，但必须满足当前BSP/DMA能力。严禁未经测量把所有图层搬入PSRAM或打开最大缓存。

设计目标（不是既有实测）：

| 指标 | 目标 |
|---|---|
| Face正常动画 | 30fps目标，帧时间p95≤50ms |
| UI对象 | Face≤80；Picker≤140；Stats≤180 |
| 状态输入到可见变化 | 收到合法focus后≤200ms |
| 总纹理/图标资源 | 初版≤1MB（可再基于map优化） |
| RX正文 | ≤8192B |
| active source缓存 | 4项catalog + 单focus + 单stats |
| 内存稳定 | 1000次切屏后无持续下降；24h趋势记录 |

不要在每帧new/delete对象；预建对象，更新坐标/宽高/opacity。glow使用少量预烘焙透明层或2–3层低透明弧线，不实时高斯模糊、不全屏alpha混合十层。

## 6. 状态与动画分离

`face_set_state()`只在 `(agent,run,state)`变化时触发过渡；每帧tick使用单调dt。相同状态快照更新进度/时间，不重置blink随机种子。

程序化面孔参数：eye_height/width、corner_radius、spacing、gaze_x/y、eyelid、mouth_curve、accent_phase。使用固定小型关键帧，不存几十秒原始图片序列。

done和error的入场动作带唯一transition_key；中断后取消旧动画回调，避免旧done timer把新working切回idle。旧页面销毁时取消相关timer；更推荐保留三屏根对象并切换visibility。

## 7. Touch

使用官方touch driver提供坐标；统一校正rotation/mirror。真机记录中心/上下左右五点。gesture recognizer使用完整down/move/up流，不混用LVGL自动long事件和另一套自定义识别，避免一按触发两次。

见 `design/interaction_tokens.json`。BOOt/PWR是硬件按钮，不把PWR长按复用为Agent Picker；用户要求的长按是触摸屏长按。

## 8. Stats渲染

由Host发送已经归约的metric/quota；设备只做格式化、倒计时显示和stale判断。value使用整数/受限浮点；金额值为USD微单位整数。quota.used_pct可超过100表示超额，环长度clamp到100，但文字显示实际超额标记。

N/A、0、unlimited、pending、stale须有不同显示分支。刷新时保留旧值与loading小标，不把数字清0。右滑返回Face立刻用最新cache。

## 9. 亮度与电源

通过官方BSP可用接口控制亮度/显示on-off；若BSP没有统一wrapper，在board_port内包装当前panel API，留硬件测试。初版不直接改PMIC供电rail、不用deep sleep，因为它可能断开USB。[S05]

亮度来自design token；idle dim计时只由真实触摸/业务状态变化重置，heartbeat/ping不算活动。首次唤醒触摸序列吞掉。启动默认较低亮度，禁止全白长时间burn-in。

## 10. 故障隔离

- JSON/协议错误：丢单帧并计数，不重启。
- 未知Agent ID/枚举：拒绝，不回退Codex假装成功。
- 内存不足：保留最简离线/错误UI，关复杂图层并诊断；不要无限重试分配。
- 触摸I²C错误：有限重试，显示可读状态；不重置PMIC或其他共享I²C设备。
- watchdog：不要通过关闭watchdog掩盖阻塞；所有长操作分批/转任务。

## 11. 原生模拟器

用同版LVGL9 SDL/host工程重用 `bot_core` 和 `bot_ui`，只替换board/transport/time实现。可先交纯逻辑C测试，再做SDL。浏览器画一张相似脸不能证明LVGL UI可运行。

Simulator必须支持固定时间、模拟pointer轨迹、加载contracts/examples，截图三屏与stale/N/A/SIM态。没有SDL环境不阻塞T02实机起步，但不得标为已测。
