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

## 4. 待执行（需用户单独确认）

```bash
# 首次烧录（未执行）
cd vendor/waveshare-1.75/examples/esp-idf/02_lvgl_demo_v9
idf.py -B build-idf61 -p /dev/cu.usbmodem2142101 flash

# 等价于
python -m esptool --chip esp32s3 -b 460800 --before default-reset --after hard-reset \
  write-flash --flash-mode dio --flash-size 16MB --flash-freq 80m \
  0x0      build-idf61/bootloader/bootloader.bin \
  0x8000   build-idf61/partition_table/partition-table.bin \
  0x10000  build-idf61/lvgl_demo_v9.bin
```

```bash
# 原厂恢复（未执行；必须三个文件一起写回，且用备份中的分区表，不是示例的）
python -m esptool --chip esp32s3 --port /dev/cu.usbmodem2142101 -b 460800 write-flash \
  0x0      "$BACKUP_DIR/factory-full.bin"   # 整片回写
```

待写固件哈希：`lvgl_demo_v9.bin` SHA-256 `82fdf3c91dab5ecf7659819ee29e23866fb4f68ff135d303b533af57cce34e27`（967,696 B）。

## 5. 未完成

| 项 | 状态 |
|---|---|
| 首次烧录 | **待用户单独确认**（R01 需"官方示例编译及实机显示"） |
| 黑/红/绿/蓝短测、四方向触摸、15 分钟无重启 | 依赖烧录 |
| `evidence/ui/` 实机截图/视频 | 依赖烧录 |
