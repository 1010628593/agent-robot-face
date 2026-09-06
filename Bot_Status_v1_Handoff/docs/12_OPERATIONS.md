# 12 · 开发、诊断、恢复与日常运行

## 1. 开发环

host逻辑单测 → contract验证 → 固件编译 → 停Bridge/monitor → 确认目标设备 → 烧录 → 观察日志 → 启Bridge → 实机交互。每个硬件写操作保留构建SHA和端口记录。

v6.1与BSP不兼容时：先定位API变更/组件manifest约束，固定供应商版本，最小patch写在vendor-patches并保留依据。v6.0.2只作为供应商参考，不能自动切换目标。

## 2. 首次备份与发布工件

私有备份包含factory-full.bin、sha256、flash-id、USB描述、板型照片、读回日期。发布包只能包含自建固件、分区/引导、manifest、各文件offset和hash、精确版本与flash命令；不要分发含用户NVS的原厂读回镜像。

不要手填猜测offset。使用当前构建产生的flasher_args.json/生成工具，所有bin必须来自同一build。恢复factory-full.bin写0x0只有在确认它是完整同板镜像且用户同意后执行。

## 3. 恢复条件与命令边界

ROM下载仍可枚举时按官方BOOT流程进入，先只读识别芯片。不要改eFuse/禁用安全保护，不force写不匹配镜像。

恢复命令示意（只在用户确认后）：

```bash
python -m esptool --chip esp32s3 --port "$PORT"   --baud 460800 write-flash 0x0 "$BACKUP_DIR/factory-full.bin"
```

写入前验证sha、大小、板型、供电和端口；不同时接电池外部电源进行未经验证操作。不先erase整个Flash，除非明确知道需要且另行确认。[S02][S15]

## 4. 故障矩阵

| 现象 | 优先查 | 不要做 |
|---|---|---|
| 找不到端口 | 数据线、原生USB、BOOT模式、占用 | 盲装驱动/换框架 |
| build报错 | 第一错误、idf版本、lock、BSP约束 | 全部升级或自动降级 |
| 白屏但有hello | panel电源/flush/初始化顺序 | 猜AXP寄存器 |
| 触摸90°错位 | rotation/mirror单层变换 | UI与driver各补一次 |
| 能看日志不能联动 | USB两个reader、monitor占用、帧前缀 | 多开串口重试 |
| Agent一直working | source已退出/事件漏采/terminal未归约 | 超时直接done |
| 切Agent额度错位 | selection_rev和缓存过滤 | 仅按当前页面名字换标签 |
| 额度频繁回0 | 缺失被填0/刷新清空cache | 隐藏错误 |
| 用量翻倍 | delta/cumulative混淆、双Hook、导入重放 | 手动减掉一半 |
| 暗屏长按自动选中 | wake触摸序列未消费 | 增大长按时间掩盖 |
| 睡醒丢串口 | USB重枚举、旧link_id | 发旧action队列 |

## 5. Doctor输出

显示：固件/协议/bridge版本、IDF vendor记录、设备id截断、端口、last_rx_age、queue_depth、dropped_frames、heap概况（若启用诊断）、四源health、usage coverage、quota age。

不显示：凭据、邮箱、绝对项目路径、完整Prompt、工具参数。diagnostic export需预览redaction结果。

## 6. 后台服务

v1后期才生成launchd plist：绝对venv Python、绝对config路径、用户权限、KeepAlive适度、日志轮换。不得在plist明文放provider API key。不root运行，不修改系统launch daemon。

先dry-run安装路径和plist内容，用户确认后load；卸载仅移除本项目label。UI未稳定时先前台serve便于看日志。

## 7. 升级策略

一次只升级IDF、BSP、LVGL之一，独立分支，重复protocol/gesture/UI/8h测试。Agent版本变化可能只影响Adapter，保留原fixture并新增新版本fixture，不覆盖旧样本掩盖回归。

## 8. 一天使用流程

插USB → Bridge启动 → hello/welcome → catalog → focus/stats → 查看/切换；工作结束Mac睡眠 → source状态待重确认 → 设备断连后关屏。重新连上不要补播历史几十条通知。
