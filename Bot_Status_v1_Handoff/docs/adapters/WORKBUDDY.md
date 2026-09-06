# Tencent WorkBuddy Adapter

## 关键事实修正

这里指腾讯WorkBuddy桌面工作台。[S11] 同一文档站点有大量CodeBuddy CLI插件/Hook资料；CLI支持不能直接证明WorkBuddy Desktop也暴露同样入口。官方WorkBuddy桌面文档明确提供MCP配置路径。[S12] 积分可能包含多个有效期/赠送池，不能由套餐名硬编码总额。[S13]

本包**没有确认一个能覆盖当前WorkBuddy Desktop所有生命周期和个人积分的稳定公开API**。这不是停止开发的理由，而是必须前置的能力探测任务。

## A. 首选：真实桌面结构化观察

在用户授权后：

1. 获取应用版本、about信息和官方文档入口，不读取凭据。
2. 查是否有Desktop正式插件/Hook/只读会话状态输出。
3. 若用户愿意提供或授权读取其结构化运行日志，仅查看明确路径与最近测试会话，白名单过滤。
4. 用一轮真实测试验证提交、工具、等待、结束字段；固定版本与fixture。
5. 确认读路径可稳定保留且不会改变应用行为，才标state=observed。

不得从CPU使用率、窗口标题颜色、进程存在或日志文件mtime推断working/done。也不默认修改应用包、关闭代码签名或拦截TLS。

## B. 可用降级：MCP自报

用户同意后安装最小本地MCP工具 `bot_status_report`，输入只接受：run_id、state、短label、ttl；工具负责转为quality=reported事件。它只更新状态，不给WorkBuddy新增文件/网络权限，不返回敏感上下文。

由WorkBuddy任务约定在开始/重要阶段/结束调用该工具。这属于自报，不是旁路全量监控，可能漏报。`reported`在设备有明显小标；60秒未续报标stale，绝不无限working。waiting由自报时也保留reported标签。

MCP工具调用自身不计入LLM request或token。自报的token/余额不得直接当exact；默认工具schema不允许提交精确quota。

## C. 使用量与额度

顺序：正式授权API → 用户导出的官方数据 → 手工值 → N/A。针对中国/国际账号分别记录region与account_key（不含原始账户标识）。

积分池分别存：基础、赠送、加量包，附有效期。只有来源提供了可相加的同单位有效余额，才可汇总；最多选择当前屏最有价值的两池，其余详情在Mac。每日奖励、过期和消费优先级不能由设备推算成固定剩余百分比。

“我记录了12次任务”只属于本地observed turns，不等于扣了12积分。

## D. 门禁

- T01先做probe，四源中此项风险最高。
- T18实现A或者B/C，并生成本机capability report。
- A无法实现时，B需要用户批准其reported边界；若用户坚持全自动observed，则此源保留BLOCKED_SOURCE，不改称ready。
- UI、模拟器、其他三个Adapter继续实现，不因一个厂商阻塞全部工作。

## 交付

`adapters/workbuddy.py`、可选`integrations/workbuddy_mcp.py`、官方导入parser、missing/expired/manual测试；报告明确Desktop vs CLI、地区、是否真实观察、额度来源、恢复/卸载方式。
