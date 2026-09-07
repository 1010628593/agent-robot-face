# Agent Robot Face 菜单应用

原生 SwiftUI 菜单栏面板，macOS 14+、Apple Silicon。380 × 560 面板可滚动展开。Bridge 独立拥有 USB；退出菜单应用不会停止观测或设备连接。

## 构建与安装

在项目目录执行 `host-app/scripts/build.sh`，产物位于 `host-app/build/Agent Robot Face.app`。脚本使用当前 `xcrun` Swift SDK，生成 Info.plist、四个设备同源单色产品图标，并执行本地 ad-hoc 签名与验证。不需要开发者证书。`host-app/scripts/install.sh` 将应用安装到 `~/Applications/Agent Robot Face.app`；安装不会自动打开应用。随后使用 Finder 打开或执行 `open "$HOME/Applications/Agent Robot Face.app"`。

构建时将当前项目绝对路径写入 bundle 的 `BridgeProjectPath`。运行时优先从已安装 Bridge LaunchAgent 的 ProgramArguments 发现 `tools/bridge`，否则使用 bundle 路径。源码不固定用户名。移动项目后重新安装 Bridge 和重新打包应用。

`host-app/scripts/uninstall.sh` 只删除标识为 `com.agentrobotface.menu` 的用户应用包。先在面板关闭「菜单应用登录时启动」并退出应用。Bridge、来源 hooks、账本与授权文件均保留。卸载 Bridge 需另行运行项目的 `tools/bridge uninstall`。

## 操作

- 当前焦点：自动选择，或固定 Codex / Cursor / Hermes / WorkBuddy。请求携带 API 当前版本号，等待 Bridge 确认后更新；其他端同时改动时重新读取并提示重试。
- 来源接入：安装、配置、实际观测三者独立。展开显示活动会话、每种生命周期/用量能力、已验证覆盖和缺口；尚未观测的计数显示「未知」。同步积压时显示同步中与待处理字节数。
- 今日统计：来自同一 Bridge 投影，显示数值、精度、覆盖、数据来源与最近更新时间。缺失数值显示未知、空配额显示暂无官方数据。
- 接入与启动：菜单应用登录项使用 macOS ServiceManagement；Bridge 登录项由 Bridge 控制。两者独立。暂停 USB 释放串口，来源观测继续。
- 安装接入、修复 Hooks：异步调用固定 `tools/bridge install` / `repair-hooks`，保留已有其他集成；修复后仍需来源重载与真实事件验证。
- 启动服务：先 kickstart 已安装服务，未加载则 bootstrap 用户 plist。重启恢复调用 kickstart -k。Doctor 执行只读 CLI 检查；其退出结果显示在面板，详细元数据由诊断区读取。维护命令超过 30 秒终止并报告失败。
- 配置按钮打开对应来源配置文件。应用不会执行、批准或取消 Agent 任务，不编辑账户配额。

## 状态与安全

仅访问 `http://127.0.0.1:17940`，使用独立 ephemeral URLSession 且禁用该会话代理，不修改系统代理。单个轮询任务：展开时约 1.5 秒，关闭时约 15 秒；刷新请求互斥。服务错误保留上次快照，同时明确标为缓存、设备未连接，禁用依赖在线服务的控制。

控制授权只在请求时读取 `~/.local/share/agent-robot-face/control.token`；不复制、展示或记录 token。CLI 输出不进入日志或界面，诊断仅使用白名单元数据 API。调试时不要打印授权头或 token 文件。

无模拟回退。构建通过不等于真实菜单、登录项、来源接入、USB 操作验收通过；验收结果见实施报告。

## 收起后重新打开

点击 macOS 菜单栏中的眼睛图标重新打开面板。点击面板外部会收起浮层，应用继续在菜单栏运行；这时没有普通窗口，自动化工具报告 `noWindowsAvailable` 并不等于应用退出或故障。Finder 再次打开应用也不保证展开菜单浮层。

本机 SDK 的 MenuBarExtra 公共接口提供 `isInserted`（是否显示菜单栏项目），没有控制浮层展开的 `isPresented` 接口。本应用保持标准菜单栏点击行为，不遍历系统私有状态项目，也不为自动化创建额外工作台窗口。

## 用量 Dashboard（v3.0）

菜单弹出面板默认进入「用量」，「设备与接入」保留原有设备状态、自动/固定焦点、来源配置、登录启动、USB 暂停和诊断恢复功能。

用量浏览与设备业务焦点分离：默认查看当前 Agent，也可选全部工具或指定工具；切换范围不会固定或切换脸页。今日、7 天和 30 天含今日，日期/时区遵循 Bridge。维度四宫格显示 Token、实际费用、额度和缓存；工具四宫格同时展示四个 Agent 的 Token 和代表额度窗口。额度卡注明来源工具，全工具额度不会相加。Codex 代表窗口优先主账户池，Spark 独立池在额度列表单列。

点击进入三行分页列表，再进入单项详情。左右拖动或列表箭头翻页；详情左右拖动切换同类指标；向下拖动返回上一层，也可用返回/总览按钮。Token 详情显示真实每日柱形记录；缺失日期留空，已知零值显示基线，不补零。模型列表按 API 排序分三行，最多显示 API 提供的前 100 个模型。

仪表是单色圆环，按已用百分比分段取蓝（<25%）、绿（<50%）、黄（<75%）、橙（<90%）、红；弧长严格对应比例，无渐变。无可靠额度、未知上限、超过 15 分钟的额度以短横线显示，不画已用弧。Token 总量不虚构上限。实际费用按来源币种分开显示，仅接受 Bridge 的实际费用和覆盖范围，未知金额不补零；额度美元金额不并入费用。

只读请求 `/v1/usage` 每两秒轮询（仅用量面板显示时）。快速切换筛选和当前业务 Agent 时取消旧任务并用代际号拒绝旧响应。离线保留当前筛选的缓存并明确标记；切换筛选先清空旧范围，防止错配。刷新通过已有本地授权文件调用 `/v1/usage/refresh`，202 只显示排队，待修订变化且采集结束后显示收到新批次；超时或失败不宣称更新成功。不读取来源登录凭据，也不直接访问官方服务。
