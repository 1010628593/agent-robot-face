# 合同目录

这些是本项目拟定的接口，不是Codex/WorkBuddy/Cursor/Hermes或微雪的厂商API。

- `device-message.schema.json`：双向USB消息；v1严格字段，message.body按type验证。
- `agent-event.schema.json`：Mac内部CanonicalEvent与usage记录。
- `capability-report.schema.json`：本机能力与证据报告。
- `examples/`：18个独立合法消息样例，全部为合成数据；其中manual/N/A示例用于真实性边界。
- `event-examples/`：三个独立CanonicalEvent样例。
- `invalid/`：9个反例必须拒绝；06错误单位、07重复agent依赖语义校验，不止Schema。

**独立样例不是同一连接的完整时间线。**不能简单按文件名次序写到设备；SIM发布器需根据场景分配link/seq/revision并保持因果关系。例08的selection_rev=4是为了单独测试WorkBuddy，不表示它能在例11的ack之前应用。

接受边界：JSON正文8192字节，UTF-8，拒绝NaN/Infinity/重复key/超过12层。所有接口都先schema后semantic。不要设置Pydantic extra=ignore来静默吞掉敏感字段。

统计的null是已知缺失；schema里字段仍然必须出现。版本变化通过v2或显式协商，不私自追加任意字段。数字单位和重复catalog等见 `tools/validate_contracts.py`。

`acceptance/*.template.json` 故意为not_probed，不是填好的本机事实。实施Agent必须生成自己的evidence报告，不能把模板改成ready而不补实际证据。
