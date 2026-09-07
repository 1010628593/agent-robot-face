# 真实账号数据下的总览复核

使用本机 UsageStore -> 生产 usage_wire.project -> v3 JSON 帧 -> 生产 C 解码器 -> 当前 LVGL stats.c 渲染。输出是当前账号数据的离屏渲染，不是硬件照片；没有向设备注入样例。

发现 Cursor 同时具有 plan=100% 与 on_demand=null。旧总览选择第一个 available 的额度，允许 null 百分比挡住后面的有效套餐额度，结果总览显示 Token 而详情有 100% 圆环。

修复只涉及 Bridge 总览代表额度选择：要求 available、未过期且 used_pct 非 null；0% 仍有效。详情列表继续保留未知按量额度，没有补零或伪造额度。保留已有 Codex 主产品优先和 reset 时间排序。

现有 Bridge 协议/API 集成通过；真实投影读回 Cursor used_pct=100，并经过 C 解码及 LVGL 重绘。before 与 after 是先后两次读取，期间 Codex Token 因真实任务继续运行而增长；不能称为完全相同数值快照。Bridge 已重启加载本次修复，设备固件未修改，仍为 3.2.7。

用户对整体布局的视觉接受仍未确认；本报告只证明总览指标选择错误已修正。
