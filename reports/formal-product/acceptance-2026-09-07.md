# Agent Robot Face 正式产品实施与验收 — 2026-09-07

## 交付状态

生产固件、真实状态 Bridge、中文设备完整界面和原生 Mac 菜单栏已实现、安装并接上实机。用户在最新烧录后明确反馈：**“显示与交互正常”**。

四源都有真实任务证据与设备投影，但 **四源全部生命周期验收尚未完成**。WorkBuddy Desktop 的真实 Stop 不携带终态分类；Hermes CLI 强制中断路径没有结束 Hook。取消、失败及等待按来源逐项保持未验证，不能用已配置、进程存在、工具开始或通用 Stop 冒充。

## 已实现

- 眼睛固定 #C9DCFF、黑色背景、正常睁眼固定瞳孔；移除按 Agent 配色、明暗呼吸和半透明眼形叠加。
- 业务状态基底、短暂互动、自然眨眼分层；重要状态收束嬉闹，完成／取消／错误有各自持久语义。
- 中央半径190px接触用途锁定；边缘横滑导航；轻点与往返抚摸洗牌动作、路径／反转过滤、650ms长按、13px移动取消、冷却与无积压。
- 保留98.7°安装参考、自动扶正、触摸朝向锁定及局部重绘。没有恢复常态整幅软件旋转。
- 生产默认关闭SIM；中文选择页、自动关注入口、主机确认、任务／今日统计／额度页签、真实缺失和过期状态，真实中文子集字体和单色产品图标。
- Python独立虚拟环境、SQLite账本、会话／任务／工具去重与迟到保护、稳定自动关注与固定Agent、私有Hook合并备份、用户级LaunchAgent。
- USB v2、32×256有界接收队列、UI线程应用模型及统一发送者；版本不匹配明确报告；烧录包装器暂停串口并自动恢复。
- 原生380×560菜单栏面板，连接、焦点、四源能力、统计、安装／修复、启动、暂停／恢复、中文诊断与恢复入口。

## 实际验收证据

| 项目 | 结果与证据 |
|---|---|
| 真实LVGL | 现有14项场景全部通过；开发关闭模式另行编译和实际渲染。见 `../formal-device-implementation.md` |
| 固件构建／烧录 | 最终bot_status.bin 848912bytes，esptool写入并校验哈希成功。见 `../formal-device/flash-readback.log` |
| 8192字节边界 | 实机接受满长welcome及ping，22个pong、9个诊断窗口，无错误重握手。见 `../formal-device/hardware-v2.json` |
| 四源设备回读 | 逐个固定四源，实机Agent/mode/selection_rev/focus_selection_rev/run_id/state与Bridge一致；最后恢复自动。见 `hardware-projection.json` |
| 真USB释放／恢复 | API暂停后connected=false且串口释放，恢复后新last_seen及重新握手成功。见 `live-pause-resume.json` |
| Bridge集成 | 12组现有元数据／协议／PTY／安装集成及8项实际loopback HTTP检查通过；未新增前后端单元测试 |
| 原生菜单栏 | Swift6.4 arm64构建、plist及严格签名校验通过；已安装并启动，CUA曾读取实际在线面板。中文错误对象解析与缺口文案经独立复核 |
| 用户实机体验 | 最新固件后中央触摸、边缘导航、长按及显示问题询问，用户回复“显示与交互正常” |

回读采样在真实脸页／信息页切换期间，均值约8.7–28.9ms、最大20.98–32.15ms；较早稳定脸页窗口约6.5–7.0ms。它们是CPU渲染提交测量，包含页面切换和首次重绘，**不是屏幕实测FPS**。构建成功、20ms调度或局部更新次数不等同物理帧率。

## 来源能力

- Codex Desktop与CLI分别取真实rollout入口；Desktop source=vscode且originator=Codex Desktop，CLI source=exec。实际开始、工具、成功、失败与明确中断已有证据；等待未实测。当前root及其他任务真实在运行，未启动AppServer伪装观察。
- Cursor IDE实际完成与停止任务已有证据。安装后的新任务再次投递本项目start→tool_start→tool_end→done；重复stop与迟到工具结果不覆盖取消。失败及等待仍按未验证显示。
- Hermes CLI实际开始、工具和完成，以及安装后的native Hook投递已证明，Hook与DB按canonical任务去重。一次60秒任务正常完成，未误标取消；后续300秒任务双Ctrl-C退出130，但没有on_session_end。不能从该退出码给现有观察器补造取消事件。
- WorkBuddy Desktop新任务实际投递SessionStart→UserPromptSubmit→PreToolUse→PostToolUse→Stop，匹配本地transcript工具／完成链。Stop无status/error/end_reason；先前UI取消对应incomplete结构仍有歧义。原生Hook投递已证实，完整终态分类仍阻塞。
- 所有官方额度缺失均显示暂无来源。真实零与未知分开；统计注明部分覆盖。未收集原始提示词、工具参数、结果、聊天正文或凭据到Bridge账本／设备。

## 真实环境发现与修正

本机两个仍更新的历史Codex文件超过1GB，且有超过2MiB的单行。修复读取器停在超大行不前进的问题：分块丢弃到换行，保存仅游标／标志，并暴露oversized_record_skipped覆盖缺口。最近数据优先，历史轮询保留有界预算；按会话排除未追上的历史，避免抢Auto或遮住其他已追上的当前任务。此处不声称全部历史同步完成，也不声称跳过的大行语义已恢复。

原生菜单面板收起后没有普通窗口，CUA会报告noWindowsAvailable；用户从菜单栏双眼图标展开。未为了自动化增加工作台或使用私有系统接口。初次CUA读取和用户反馈不代替所有登录项控制逐项测试。

## 保留验收边界

未进行真实USB拔插、Mac睡眠唤醒、退出登录／重新登录验收；已做的是实际串口close/reopen、Bridge重启及烧录恢复。未做物理屏幕帧率采集。失败／等待／取消缺失逐源保留，四源全生命周期验收门槛不降级。

本批未提交Git或推送；保留原有未提交IMU修改。私有Hook备份位于用户数据目录，不纳入公开报告。Bridge虚拟环境使用本机已有Python3.13.12基解释器，包隔离；迁移机器需按安装文档准备运行时。

最终复核：14/14真实LVGL再次通过；12组Bridge集成与8项HTTP检查再次通过（bridge-final-integration.log）；已安装应用签名与plist通过。Bridge为auto/Codex、demo=false、USBconnected、无服务错误。最后脸页窗口117次提交/5259ms，均值6933µs、最大7631µs；该提交计数不代表面板帧率。采样在活动工具切换中，诊断5秒窗口与实时API并非同一采样时刻。
