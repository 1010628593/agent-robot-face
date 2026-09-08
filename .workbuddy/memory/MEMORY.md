# Bot Status v1 项目长期记忆

## 硬件平台
- ESP32-S3-Touch-AMOLED-1.75 开发板（Waveshare）
- 466x466圆形AMOLED显示屏
- QMA7981加速度计（I2C总线，GPIO14/15）
- CST9217电容触摸屏
- 16MB Flash + 8MB PSRAM

## 开发环境
- ESP-IDF v6.1（路径：/Users/hanxin/.espressif/v6.1/esp-idf）
- macOS系统，原生USB CDC端口：/dev/cu.usbmodem2142101
- 构建工具：idf.py（需要激活脚本）
- 固件分区：factory@0x110000（原厂），示例使用factory@0x10000(8M)+storage@0x810000(7M)

## 关键配置
- BSP版本：waveshare/esp32_s3_touch_amoled_1_75 3.0.1
- LVGL版本：9.4.*
- 显示亮度：28%（设计规范）
- UI定时器：10ms轮询
- 重力感应检测周期：20ms（50Hz）

## 已完成功能
1. **基础UI系统**：三屏界面（FACE、PICKER、STATS）
2. **手势识别**：12种手势状态机
3. **帧协议**：JSON编解码器
4. **SIM模拟器**：状态循环器
5. **重力感应**：QMA7981驱动，连续跟随旋转，保持表情水平

## 麦克风硬件（ES7210）
- ES7210四通道ADC，I2C地址0x40
- MIC1/MIC2=板载前置双麦，MIC3=ES8311播放参考，MIC4=未连接
- BSP时隙顺序：MIC1→MIC3(参考)→MIC2→MIC4（不等于物理编号顺序！）
- 首版配置：22050Hz单声道STD模式，`active_mics=1`
- 默认增益30dB，改增益需重置检测器
- BSP无deinit函数，需手动`i2s_del_channel()`释放

## 待开发功能
1. **麦克风环境音响应**（进行中）：docs/microphone-response-plan.md + v1.1修订方案
2. USB桥接通信（T10）
3. 真实数据适配器（T15-T17）
4. 触摸手势优化（T14）
5. 性能优化和稳定性测试

## 重要注意事项
- 烧录必须使用esptool直调，idf.py有build/log目录权限问题
- 原厂备份已保存：$HOME/Workspace/bot-status-private-backups/20260906-202608/factory-full.bin
- 恢复必须整片回写备份，不能只写app
- 日志抓取需要在复位瞬间，平时端口安静是正常的

## 构建环境（踩坑记录）
- **必须用 `source tools/idf-env.sh`**（EIM venv），不能用 `esp-idf/export.sh`
  后者激活 python_env 与项目配置 venv 不一致 → 报 "Run idf.py fullclean"
- **`source` 不能接管道**：`source x.sh | tail` 会让环境变量丢在子shell，idf.py 变 command not found
  正确：`source ../tools/idf-env.sh > /tmp/log 2>&1 && idf.py build`
- **IDF v6.1 组件名**：heap 组件叫 `heap`（不是 `esp_heap`）
- macOS 无 `timeout` 命令；`ps` 在沙箱下 operation not permitted
- 原生USB CDC端口会变（曾为 /dev/cu.usbmodem2142101，现为 /dev/cu.usbmodem101），烧录前用 esptool chip-id 探测
- 探测端口若报 "Could not exclusively lock port" = 被其他进程占用（常见：正在跑的日志抓取脚本）

## 开发规则
- 所有UI坐标来自design/ui_tokens.json
- 颜色来自设计规范
- SIM模式必须明确标注"SIM"
- 触摸坐标通过bot_gesture处理，LVGL手势识别关闭