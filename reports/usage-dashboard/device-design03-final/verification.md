# 设备 UI 3.0.2 原生验收

生产修改已冻结；ESP build、flash、硬件 FPS 验收由 root 执行。本报告未将原生 CPU 时间称作硬件帧率。

## 界面

`overview.png` / `multi-quota.png` / `quota.png` 是生产 stats.c + ui.c 的真实 LVGL 466×466 渲染，使用明确样例数据。`comparison.html` 放置批准设计03与当前同视口输出。保留03总览、层级和直接页签；同工具多额度按用户新增选择使用最多3条360°同心环。输入顺序固定，未知/过期额度不画彩环且入口保留。点击中心周期行或环均进入对应额度详情。

数字字体64/36来自 Noto Sans SC，包含破折号。Stats 单独从共享96px RGB565图标生成32px单色A8图标，不改变 Picker 彩色素材。

返回与底部页签在550、800、1200、1800ms的四张详情帧完全一致，像素 SHA256为 `6373219422ec74e0b07ee2c5f86a63c6de5cd9dd4d971cb5587169845bd36589`。返回区域44亮像素，页签区域1041亮像素。完整证据 `../device-design03-steady/pixel-proof.txt`。静态 chrome 的漏绘通过仅结构变化时 root 一次 invalidation 和限制局部 slide 区域处理；心跳不触发结构重绘。

## 交互

`center0..2` 与 `ring0..2` 六条原生触摸回放全部进入depth2；center2图明确显示月度86%。`contacts/trace.txt` 验证350ms、约15px小移动的同目标点按可用；两指→一指和700ms长按均不激活info。取消后的五张静止图 hash完全一致。连续下滑只逐级 depth2→1→0，根层下滑回Face。输入识别仍由既有 arbiter 管理；没有新增hold动作。

## 性能

最终 `performance.txt`：300个空闲tick 总览0.164ms/详情0.156ms，均0 flush。300个host clock+data_rev更新，总览0.853ms、0 flush、0 label/gauge/layout变更；详情2.269ms、10 flush、5个分钟重置文字更新、0 gauge/layout变更。以上为Mac原生累计CPU时间。旧基线分别600个空闲flush；300个host更新总览1500/详情3000 flush，详见 `../device-polish/performance.txt`。

## 编译

真实 LVGL replay_os_ui native target 构建成功；build-touch-debug/compile_commands.json 中实际Xtensa flags针对 stats.c/ui.c 的语法与警告检查通过。没有新增单元测试。未经本子任务flash；完整ESP链接、实机通信和帧率不在本原生证据范围。
