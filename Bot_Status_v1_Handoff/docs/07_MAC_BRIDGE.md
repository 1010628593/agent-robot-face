# 07 · macOS Bridge、配置与本地服务

## 1. 技术栈与边界

Python3.11+独立venv（不改EIM内部Python，不装进Conda base）、asyncio、pyserial、SQLite、Pydantic2或等价严格验证器、aiohttp。使用最少依赖并生成lock；开发工具ruff/pytest/jsonschema按锁文件固定。

选择Python是主机桥接实现，不改变开发板ESP-IDF/C路线。v1不做完整SwiftUI/Tauri，也不要求Docker、数据库服务器或公网端口。

## 2. 数据路径

macOS用户数据目录：`~/Library/Application Support/BotStatus/`，权限0700。数据库、配置、spool、诊断分别子目录；日志按10MB×5轮换。开发板原厂备份位于独立private backup目录，不进入仓库。

```text
BotStatus/
  config.toml
  state.db
  bridge.token             0600，本地服务token，不是厂商凭据
  spool/                   清洗后的待消费事件
  logs/
  evidence/                本机版本与接入结果，无原始聊天
```

密钥若确需持久化使用macOS Keychain；不把API key复制进config或ESP32。数据清理CLI必须显示将删除范围并确认，不触碰厂商目录。

## 3. 服务和API（实施后提供）

默认绑定 `127.0.0.1:17875`，拒绝非loopback访问；不自动绑定0.0.0.0。变更/读状态端点使用本地随机Bearer token；健康端点只返回最小版本信息。禁用CORS，拒绝浏览器跨源Origin，检查Host白名单。请求上限64KiB，单源速率100次/秒突发200。

| HTTP | 路径 | 作用 |
|---|---|---|
| GET | /v1/health | 最小存活/版本，不含账户/会话 |
| GET | /v1/state | 脱敏当前projection |
| GET | /v1/capabilities | 四源本机检测结果 |
| POST | /v1/events/{agent_id} | 已清洗CanonicalEvent，token校验 |
| POST | /v1/actions | 与设备action相同的安全命令 |
| POST | /v1/imports/usage | 已验证导入，不合并不同来源重复账目 |

外部Agent原始Hook输入先在emitter中做白名单清洗，Host入口不接收任意raw prompt。管理API不提供任意执行shell、任意URL代理或系统凭据读取。

## 4. CLI契约（以下命令需要实施者实现）

```text
bot-status doctor
bot-status serve --config /absolute/path/config.toml
bot-status simulate --scenario full-tour --device /dev/cu.usbmodem...
bot-status select codex
bot-status stats --agent cursor --scope today
bot-status probe codex|workbuddy|cursor|hermes
bot-status hooks install <agent> --dry-run
bot-status hooks uninstall <agent> --dry-run
bot-status import usage --agent <agent> --file <authorized-file>
bot-status import quota --agent <agent> --file <authorized-file>
bot-status service install --dry-run
```

simulate必须独立库/独立端口或显式模式，不能向真实账本写假用量。硬件模拟显示SIM，退出后清除模拟state并重新握手。

## 5. Adapter契约

所有Adapter实现四个操作，签名在代码中统一：

```python
class AgentAdapter(Protocol):
    async def probe(self) -> CapabilityReport: ...
    async def start(self, emit: Callable[[CanonicalEvent], Awaitable[None]]) -> None: ...
    async def stop(self) -> None: ...
    async def read_quota(self) -> list[dict]: ...
```

这是接口声明，不是已实现Python库。`start`注册/读取已授权观察路径，不偷偷安装Hook或启动新Agent任务。quota不可读取时返回符合Quota对象契约的明确不可用记录，不抛异常淹没状态通道。

Adapter长轮询/API调用设deadline，不阻塞serial heartbeat；异常局限本Agent。每个source_instance由产品、入口、版本、local profile稳定识别。`probe`不遍历所有用户文件、不读凭据内容。

## 6. Hook执行预算

实现 `hook_emitter` 从stdin接受厂商输入，按源白名单转CanonicalEvent，优先投递loopback；超时200ms写入0600私有spool后退出0。stdout为空；若具体厂商要求中性JSON，由适配器明确只输出允许的中性结构，不能把日志写stdout。

同一个Hook不调用LLM，不查询云额度，不直接开串口。spool每文件最大16KiB，总50MB，最多10000条，满时lost_events计数并标partial。每文件原子写rename；消费成功后删除。事件本身有稳定event_id，重试不二次计数。

安装器先读取既有配置，生成最小合并diff、备份和回滚文件，人工确认后写入；不能覆盖用户已有hooks。卸载只移除本项目的已记录项，不能整个清空hooks.json。

## 7. USB管理

显式端口优先；自动发现按USB descriptor + hello.device_id验证。不能“连接找到的第一个串口”。第一次选择设备需记录确认，端口变更允许重新发现同device_id。

打开时尽量预设DTR/RTS为不触发下载的状态；不同驱动可能短暂重置，验收必须覆盖。串口断开：指数退避0.5/1/2/4/8秒并限制；重连先重新握手，不直接冲发旧缓存action。

Bridge sleep/wake：重建link，统计发当前缓存+age，活跃source重新探测。USB terminal monitor与Bridge互斥，doctor能报告busy port。

## 8. 统计调度

本地ledger投影最多1Hz；quota默认每60秒刷新但服从provider最小间隔与Retry-After。Cursor Admin hourly aggregate不超过每小时一次。手动刷新只提升一次合法刷新请求的优先级，不能突破min interval。

401/403：标needs_auth，不不断重试；429：尊重Retry-After，无此字段则30秒起指数退避至15分钟；5xx/网络错误保留缓存+stale。quota错误不使Agent业务状态变error。

## 9. 多会话与统计持久化

SQLite WAL、单writer、事务短。建议表：source_instances、events_seen、runs、tool_calls、usage_records、quota_snapshots、selection、schema_migrations。

- events_seen键 `(agent_id, source_instance, event_id)`；包含kind与时间，无prompt。
- usage_records键含native_usage_id/account/provider/model；输入类型为delta/cumulative/authoritative_window，不混在一个sum中。
- selection只有一行，revision事务递增。
- 缓存的live state重启时失效；计数/导入仍保留。

原始清洗事件保留7天，统计明细30天，日聚合90天；可配置但默认不无限增长。导入以文件sha和原行ID去重，不删除用户原文件。

## 10. 可选安全动作

`open_agent`仅打开已配置且存在的本地App，使用参数列表调用系统open，不拼接shell。`open_usage`仅打开来源官方域名allowlist页面。默认不开自动启动，不聚焦/输入/批准。

v1设备不需要这些动作即可验收三屏；实现时作为明确可关闭能力，不把全局Accessibility权限作为基础前提。
