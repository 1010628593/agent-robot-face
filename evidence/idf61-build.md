# T02 · ESP-IDF v6.1 官方示例构建记录

**实证等级**：`ESP-IDF 编译通过`（**未烧录**，未连接开发板）
**时间**：2026-09-06
**环境**：`. tools/idf-env.sh`（EIM ESP-IDF v6.1 + 补齐的 EIM cmake 4.0.3）

## 1. 源码来源（标准 1.75，非 C 版）

| 项 | 值 |
|---|---|
| 仓库 | https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75.git |
| HEAD | `e4344e70c2fa78a13e8a06566507f1ba8af6672a` |
| 提交日期/说明 | 2026-08-21 · "Refresh ESP-IDF v5.5 compatibility matrix (#21)" |
| 示例 | `examples/esp-idf/02_lvgl_demo_v9`（LVGL 9 官方 demo） |
| 存放位置 | `vendor/waveshare-1.75/`（嵌套克隆，SHIP 由上述 SHA 固定） |

## 2. 构建命令与结果

```bash
. tools/idf-env.sh
cd vendor/waveshare-1.75/examples/esp-idf/02_lvgl_demo_v9
idf.py -B build-idf61 set-target esp32s3
idf.py -B build-idf61 build
```

**结果：`Project build complete.`（2160/2160 目标，0 error）**

产物：

| 文件 | 大小 | SHA-256 |
|---|---|---|
| `build-idf61/lvgl_demo_v9.bin` | 967,696 B (0xec410) | `82fdf3c91dab5ecf7659819ee29e23866fb4f68ff135d303b533af57cce34e27` |
| `build-idf61/bootloader/bootloader.bin` | 22,560 B (0x5820) | `4081e71956b520718727b91433dd299d6dfca57ad0a35ffdc610ba11cb068084` |
| `build-idf61/partition_table/partition-table.bin` | 3,072 B | `d3e6663d9cbd407623c82f215df58a5c9bd1e353fd937ad519018c06fd9298fb` |

App 分区剩余 88%（0x713bf0 free），bootloader 分区剩余 31%。

### 编译期告警（仅 NOTE 级，来自 IDF 自带组件，非本项目代码）

```text
NOTE: components/bt/host/nimble/Kconfig.in:1428: BT_NIMBLE_MESH_PROVISIONER: 'default 0' is not a valid bool value ... treated as 'n'
NOTE: components/fatfs/Kconfig:240: FATFS_PRINT_LLI: ... treated as 'n'
NOTE: components/fatfs/Kconfig:245: FATFS_PRINT_FLOAT: ... treated as 'n'
```

无 error、无 warning 需要处置；**v6.1 未出现 API 不兼容**。

## 3. 依赖锁定（已归档）

归档：`evidence/locks/02_lvgl_demo_v9.idf61.dependencies.lock`
（SHA-256 `73639dc8bb77a6f313158e600ee8607a3fb52dc73eb929275a96cd14fee68a60`，370 行；lock version 3.0.0，manifest_hash `ea1cddcfc86fa8d45a2eccdfa041b544d1d1c46a1f4abf6e36ffc9eca968a898`）

关键解析版本：

| 组件 | 版本 | 备注 |
|---|---|---|
| `idf` | 6.1.0 | 目标 v6.1，未降级 |
| `waveshare/esp32_s3_touch_amoled_1_75` | **3.0.1** | BSP（manifest 声明 `*`，现锁定） |
| `lvgl/lvgl` | **9.4.0** | LVGL 9，符合规范 |
| `espressif/esp_lcd_co5300` | 2.1.0 | CO5300 AMOLED 驱动（与规范一致） |
| `waveshare/esp_lcd_touch_cst9217` | 2.0.0 | CST9217 触摸（与规范一致） |
| `espressif/esp_lvgl_adapter` | 0.6.4 | |
| `espressif/usb` | 1.5.0 | BSP 对 `idf_version >=6.0` 的条件依赖，说明 BSP 3.0.1 已考虑 IDF 6.x |
| `espressif/esp_io_expander_tca9554` | 2.0.3 | IO 扩展（电源/复位相关，不手动改） |
| `espressif/esp_codec_dev` | 1.5.11 | BSP 自带（v1 不使用音频，不初始化） |

**可复现性**：仓库 SHA + 本 lock 文件 + IDF v6.1 即可重建同一构建。

## 4. 首次构建踩到的两个坑（已修，不影响 EIM）

1. `idf.py` 报 `"cmake" must be available on the PATH`：EIM 把 cmake 装成 `tools/cmake/4.0.3/CMake.app/Contents/bin/cmake`，其激活脚本的静态 PATH 不含该布局；`idf_tools.py export` 又因 `esp-rom-elfs` 已装版本与 tools.json 不匹配而整体报错退出。**处置**：新增 `tools/idf-env.sh`，激活后补入 EIM 自带 cmake 4.0.3，不安装/升级/降级任何 EIM 工具。
2. 沙箱环境 `os.mkdir` 对已存在的 `build-idf61/log` 抛 `PermissionError`，使 idf.py 在启动阶段中断。清除该残留目录后重跑通过（与代码无关）。

## 5. 未完成项

| 项 | 状态 | 阻塞原因 |
|---|---|---|
| 实机显示/触摸验证 | 未开始 | 开发板未接入（`BLOCKED_HARDWARE`，见 `evidence/baseline.md` 第 3 节） |
| 首次烧录 | 未执行 | **需用户单独确认**（`idf.py -B build-idf61 -p <PORT> flash`） |
| 15 分钟稳定性 | 未开始 | 依赖烧录 |

构建成功 ≠ 上板成功。烧录命令待确认端口后给出。
