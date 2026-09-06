# Nous Research Hermes Agent Adapter

## 公开路径

Hermes官方将Gateway hooks与支持CLI的plugin/shell hooks区分。CLI路径应使用实际版本支持的plugin观察Hook；不能把 `~/.hermes/hooks/HOOK.yaml` 的Gateway事件当作CLI普遍事件。[S14]

当前文档列出pre/post_tool_call、pre_llm_call、on_session_end、pre_approval_request、post_api_request等；版本变动应以本机回调签名和fixture为准。尤其on_session_end可能是每轮finalization，不应仅按名字理解成应用退出。[S14]

## 首选实现

创建一个最小只读plugin，在 `register(ctx)` 注册观察回调。所有回调返回None，不返回block/transform/directive。只提取IDs、工具名、结果类别和usage；不保留messages、history、assistant_response。

| 回调（以本机存在为前提） | 使用 |
|---|---|
| pre_llm_call | 每轮开始证据；若实际每迭代回调则按turn_id去重 |
| pre_tool_call / post_tool_call | 工具集合增删 |
| pre_approval_request | waiting_started(approval) |
| post_approval_response | waiting_cleared |
| on_session_end | 按completed/failed/interrupted/turn_exit_reason终结 |
| post_api_request | 成功请求的usage；不是整个turn结束 |
| api_request_error | 记录请求错误/重试，非terminal不能把脸变error |

没有turn_id时本机捕获序列形成稳定local run_id，报告该映射限制，不能每个tool创建新turn。

## 使用量

以provider返回的usage为依据，记录输入、输出、缓存/推理字段的语义。不同provider“input是否包含cache”不统一，parser需保留该事实，不把缓存重复加进total。

仅存在进程内累计计数时保存baseline，不把启动前未知历史当今日0。成本只有官方reported cost或有明确模型价格版本的估计；估计显示~，不推断钱包余额。

## 额度

Hermes是Agent框架，不天然拥有一个统一Hermes套餐余额。按当前provider/account查询官方余额/限额；没有公开read接口时用N/A或LOCAL BUDGET。

若Hermes实际使用Codex账户后端，quota必须共用account_key，显示SHARED。不要把Codex quota与Hermes quota相加成双倍资源。

## 发布证据

真实CLI或Gateway入口各自单独验收。最少覆盖多工具、显式审批、取消、成功、provider重试、usage重复、源进程退出。插件安装/卸载须最小diff，出错不阻塞原Agent。
