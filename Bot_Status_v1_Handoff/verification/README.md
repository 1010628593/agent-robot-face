# 文档包校验报告

日期：2026-09-06。范围：文档、JSON Schema、合成样例、测试向量定义与文件完整性。

## 已执行

- `tools/validate_contracts.py`：PASS，结果见 `contract-validation.json`。
- 3个JSON Schema语法检查，18个合法设备消息、3个合法内部事件、4个not_probed能力模板通过。
- 9个非法消息和7个非法原始JSON被拒绝。
- 39个验收向量（状态12、手势12、统计15）的ID/输入定义检查；**未执行产品对应逻辑**。
- 现有文档相对文件链接检查：19项，无缺失；Markdown代码围栏配对通过。
- `config.example.toml` 已解析；校验脚本Python语法已检查。
- 最大样例JSON正文：1732字节；设计上限8192字节。

## 校验环境

本次校验在文档生成容器中运行（Linux / Python 3.13.5 / jsonschema 4.26.0），**不是用户的Mac或ESP32**。本地Agent应在Mac再次运行同一校验。

## 没有执行、不能据此宣称通过

ESP-IDF v6.1编译、BSP兼容性、屏幕/触摸、帧率、实际USB重连、Mac睡眠、四源真实事件、账号usage/quota、8小时实机测试。上述项目状态均为NOT_RUN，任务见实施计划。

## 文件完整性

`MANIFEST.sha256`记录本包除清单自身外的每个文件SHA-256；它验证文件字节，不代表设备功能被测试。
