# T02 · 硬件识别与原厂备份（只读）

**实证等级**：`文档校验通过`（识别与备份均为只读；**未烧录**）
**时间**：2026-09-06 · **端口**：`/dev/cu.usbmodem2142101`

## 1. 端口确认

`ls /dev/cu.*` 在接入前后对比：新增 `/dev/cu.usbmodem2142101`（原生 USB CDC，与 `START_HERE_macOS.md` 对标准 1.75 的描述一致）。
另一个 `/dev/cu.usbserial-214220` 属于 ASUS 外设，**不是**开发板。

```bash
python -m esptool --chip esp32s3 --port /dev/cu.usbmodem2142101 flash-id
```

```text
Chip type:          ESP32-S3 (QFN56) (revision v0.2)
Features:           Wi-Fi, BT 5 (LE), Dual Core + LP Core, 240MHz, Embedded PSRAM 8MB (AP_3v3)
Crystal frequency:  40MHz
USB mode:           USB-Serial/JTAG
MAC:                28:84:85:8d:71:e0
Manufacturer: 20 / Device: 4018
Detected flash size: 16MB
Flash type set in eFuse: quad (4 data lines)
Flash voltage set by eFuse: 3.3V
```

核对结论：**ESP32-S3 ✅、16MiB ✅**（与规格要求一致，未出现"不同容量/不同板型"，可继续）。

## 2. 安全功能只读检查（`espefuse summary`）

| eFuse | 值 | 判定 |
|---|---|---|
| `SECURE_BOOT_EN` | False | 未启用 Secure Boot |
| `SPI_BOOT_CRYPT_CNT` | Disable (0b000) | **未启用 Flash 加密**（备份为明文，可安全烧录） |
| `WR_DIS` / `RD_DIS` | 0 / 0 | 未锁定，无读保护 |
| `DIS_PAD_JTAG` / `SOFT_DIS_JTAG` | False / 0 | JTAG 可用 |
| `ENABLE_SECURITY_DOWNLOAD` | False | 未开安全下载模式 |

**无异常安全功能 → 继续进行备份。**（未写任何 eFuse，未启用 Secure Boot/Flash Encryption。）

## 3. 原厂完整备份

```bash
export BACKUP_DIR="$HOME/Workspace/bot-status-private-backups/$(date +%Y%m%d-%H%M%S)"
umask 077
python -m esptool --chip esp32s3 --port /dev/cu.usbmodem2142101 --baud 460800 \
  read-flash 0 ALL "$BACKUP_DIR/factory-full.bin"
shasum -a 256 "$BACKUP_DIR/factory-full.bin" > "$BACKUP_DIR/factory-full.bin.sha256"
```

| 项 | 值 |
|---|---|
| 路径 | `$HOME/Workspace/bot-status-private-backups/20260906-202608/factory-full.bin`（**仓库外**，`umask 077`，不入库） |
| 长度 | **16,777,216 字节（= 16MiB ✅）** |
| SHA-256 | `4c020428c2fe5c7610a4694682ddc473427707ad553a21afe168c090d0b33c38` |
| 读取耗时 | 106.9 s（约 1256 kbit/s） |

### 备份完整性抽查

- 全 0xFF 占比 25.4% → 文件是真实固件内容，不是空片或读取失败。
- 分区表 magic `0xAA50` ✅，可解析出 8 个分区。
- **原厂分区表**（与官方示例不同，恢复时必须连带分区表一起恢复）：

| 分区 | 类型 | 偏移 | 大小 |
|---|---|---|---|
| nvsfactory | data/nvs | 0x9000 | 0x32000 |
| nvs | data/nvs | 0x3b000 | 0xd2000 |
| otadata | data | 0x10d000 | 0x2000 |
| phy_init | data/phy | 0x10f000 | 0x1000 |
| **factory** | app | **0x110000** | 0x580000 |
| ota_0 | app/ota0 | 0x690000 | 0x300000 |
| assets | data (0x82) | 0x990000 | 0x300000 |
| storage | data (0x82) | 0xc90000 | 0x370000 |

> 对比：官方 `02_lvgl_demo_v9` 的 `partitions.csv` 为 `nvs 0x9000 / phy_init 0xf000 / factory 0x10000 (8M) / storage 0x810000 (7M)`。
> 两者**布局不同**：烧录官方示例会覆盖分区表，使原厂 `factory@0x110000` 与 `assets` 成为孤儿分区。因为已有完整备份，可整体恢复；**禁止只恢复 app 而不恢复分区表**。

## 4. 首次烧录（**已执行**，2026-09-06 20:5x，用户已单独确认）

```bash
cd vendor/waveshare-1.75/examples/esp-idf/02_lvgl_demo_v9/build-idf61
python -m esptool --chip esp32s3 -p /dev/cu.usbmodem2142101 -b 460800 \
  --before default-reset --after hard-reset write-flash \
  --flash-mode dio --flash-freq 80m --flash-size 16MB \
  0x0 bootloader/bootloader.bin 0x8000 partition_table/partition-table.bin 0x10000 lvgl_demo_v9.bin
```

结果：`Wrote 967696 bytes (544888 compressed) at 0x00010000 in 4.6 seconds` → `Hash of data verified.` → `Hard resetting via RTS pin...`
**只写 3 个分区，未使用 `--erase-all`，未改 eFuse。**

> 注：`idf.py flash` 在本机受管环境会撞上 `os.mkdir('build/log')` 已存在的 shim 报错，故改用构建产物里的规范化 `flash_args`（`--flash-mode dio --flash-freq 80m --flash-size 16MB`）直接 esptool 写入，命令与 idf.py 最终执行的一致。

### 启动日志（复位后完整抓取，115200）

```text
I (273) boot: Loaded app from partition at offset 0x10000
I (283) octal_psram: vendor id : 0x0d (AP)   density : 0x03 (64 Mbit)
I (327) esp_psram: Found 8MB PSRAM device / Speed: 80MHz
I (642) esp_psram: SPI SRAM memory test OK
I (650) cpu_start: GPIO 44 and 43 are used as console UART I/O pins
I (661) app_init: App version:      e4344e7
I (674) app_init: ESP-IDF:          v6.1
I (686) efuse_init: Chip rev:         v0.2
I (733) spi_flash: flash io: qio
I (763) esp_lvgl:adapter: LVGL adapter initialized successfully
I (774) co5300: version: 2.1.0
I (776) co5300_spi: LCD panel create success, version: 2.1.0
I (2219) CST9217: Checkcode: 0x204ECACA
I (2221) CST9217: Resolution X: 466, Y: 466
I (2223) CST9217: Chip Type: 0x9217, ProjectID: 0x5734
I (2230) esp_lvgl:touch: Touch input device registered successfully (IRQ mode: enabled)
I (2262) esp_lvgl:adapter: LVGL task started successfully
I (2269) main_task: Returned from app_main()
```

**关键结论**

| 项 | 证据 |
|---|---|
| ESP-IDF v6.1 实机运行 | `app_init: ESP-IDF: v6.1` |
| CO5300 AMOLED 初始化 | `co5300_spi: LCD panel create success`（驱动 2.1.0） |
| CST9217 触摸初始化 | `Checkcode: 0x204ECACA`、Chip `0x9217`、IRQ 模式注册成功 |
| **屏幕分辨率** | **466 × 466**（与 `docs/02_UI_UX.md` 画布一致 ✅） |
| 8MB PSRAM | `Found 8MB PSRAM device`、`SRAM memory test OK` |
| 无 Panic / 无反复复位 | 日志直到 `Returned from app_main()`，无 `Guru Meditation`、无 `rst:` 复位行 |

仅 2 条 warning（非错误）：`co5300_spi: The 3Ah command ... overwritten by external initialization sequence`（BSP 自身行为）；`LV_USE_GESTURE_RECOGNITION is disabled`（官方 demo 未开手势，v1 会自己实现手势层，T04）。

> 注意：本示例主 console 是 **UART0 (GPIO43/44)**，USB Serial/JTAG 为 secondary；所以日志要在复位瞬间抓取，平时端口安静属正常。

## 5. 待执行 / 待确认

```bash
# 原厂恢复（未执行；必须整片回写，不能只写 app）
python -m esptool --chip esp32s3 --port /dev/cu.usbmodem2142101 -b 460800 write-flash 0x0 "$BACKUP_DIR/factory-full.bin"
```

| 项 | 状态 |
|---|---|
| 15 分钟连续运行观察 | **进行中**（后台抓取 900 s 串口日志 → `evidence/ui/official-demo/boot-and-15min.log`） |
| 屏幕实拍（黑/红/绿/蓝短测需后续自定义固件） | **待用户目测确认**：屏幕上应显示官方 LVGL 9 demo 界面 |
| 四方向触摸 | 待用户触摸验证（官方 demo 有控件可点） |

## 6. 已写固件哈希（写入后校验通过）

| 文件 | 大小 | SHA-256 |
|---|---|---|
| `lvgl_demo_v9.bin` | 967,696 B | `82fdf3c91dab5ecf7659819ee29e23866fb4f68ff135d303b533af57cce34e27` |
| `bootloader.bin` | 22,560 B | `4081e71956b520718727b91433dd299d6dfca57ad0a35ffdc610ba11cb068084` |
| `partition-table.bin` | 3,072 B | `d3e6663d9cbd407623c82f215df58a5c9bd1e353fd937ad519018c06fd9298fb` |

实证等级：**已烧录**（R01 的一半完成；实机显示与 15 分钟稳定性仍需目测/长跑确认）。
