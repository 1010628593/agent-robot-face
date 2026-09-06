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

## 待开发功能
1. USB桥接通信（T10）
2. 真实数据适配器（T15-T17）
3. 触摸手势优化（T14）
4. 性能优化和稳定性测试

## 重要注意事项
- 烧录必须使用esptool直调，idf.py有build/log目录权限问题
- 原厂备份已保存：$HOME/Workspace/bot-status-private-backups/20260906-202608/factory-full.bin
- 恢复必须整片回写备份，不能只写app
- 日志抓取需要在复位瞬间，平时端口安静是正常的

## 开发规则
- 所有UI坐标来自design/ui_tokens.json
- 颜色来自设计规范
- SIM模式必须明确标注"SIM"
- 触摸坐标通过bot_gesture处理，LVGL手势识别关闭