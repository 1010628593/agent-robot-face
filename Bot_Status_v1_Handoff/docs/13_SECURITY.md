# 13 · 安全、隐私与可观测性

## 1. 威胁边界

设备不是可信的账号保管器，不能凭设备发来的JSON执行任意命令。USB本地不等于无需验证；未来网络尤其需要认证。任何Hook/Plugin都是以用户权限运行的代码，必须可审计、可卸载。

## 2. 数据最小化

从源到Bridge仅留：产品/实例/匿名session/run、状态、白名单工具名、结果类别、计数/usage数值、时间。对于包含prompt/history/arguments的Hook input，必须先取白名单再记录，禁止“先落全量日志以后再脱敏”。

向设备不发送账户邮箱、token、Cookie、Prompt、文件名、路径、工具输出；account_key用安装级salt hash。data detail默认为Terminal/File/Browser等类别，可配置privacy=true完全隐藏detail。

## 3. API限制

本地服务loopback + random token + Host/Origin校验 + 限流/限body；不开放公网，不提供任意路径读取、任意URL抓取。导入必须用户指定文件，不扫描整个home。

设备action whitelist只有select/refresh/opt-in open_agent/open_usage。没有approve、send、shell、delete、redeem-credit或purchase。quota服务只读，禁止自动购买加量包/消耗reset额度。

## 4. 认证

优先复用官方客户端自身已认证的只读接口，让客户端处理刷新。需要用户提供的Admin/API凭据只存Mac Keychain，明确用途和可撤销方式。不读取浏览器Cookie数据库、不破解keychain、不把凭据烙入固件。

WorkBuddy/某provider没有接口时，返回N/A或支持授权导入，不将凭据抓取列为“普通适配”。

## 5. Hook保障

所有旁路Hook正常或错误都不改变授权结果；stdout不输出额外指令/模型上下文。超时、spool满或Bridge关闭只影响显示，不能让原Agent被block或继续额外轮次。安装变更前diff，备份/回滚；尊重原工具的信任确认，不使用bypass信任参数。

## 6. 开发与硬件

烧录前备份；禁止默认erase-flash与eFuse。固件内无真实Wi-Fi密码或密钥。为后续Wi-Fi预留认证接口，但v1不启动开放AP和未鉴权websocket。

软件包中的示例数据全部为合成，并标SIM。真实脱敏fixture需经人工检查后入库；raw输入留在私有路径或直接不存。

## 7. 日志与指标

可记录：时间、事件类型、匿名ID、状态转移、延迟、错误码、丢包数量、frame_size、heap、队列深度。不可记录: HTTP Authorization、credential文件内容、完整usage原始响应中的个人身份。

报告区分source错误（某产品读取不到）与设备错误（断连/显示）。quota错误不等于Agent任务失败。

## 8. 必测

恶意action传URL/shell拒绝；body额外api_key字段拒绝；未授权HTTP/Origin拒绝；厂商返回密钥形字符串不会进入日志；关闭Bridge后Hook仍快速退出；UI不可直接批准命令；超过帧限制只丢该帧。
