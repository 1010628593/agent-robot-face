# 从已安装的 ESP-IDF v6.1 开始

用户已经通过EIM安装并激活v6.1，不再安排重复安装。先完成官方示例与安全备份；本页不启动四源集成。

## 1. 验证当前shell

```bash
idf.py --version
printf '%s\n' "$IDF_PATH"
python -c 'import sys; print(sys.executable)'
```

预期版本v6.1，IDF路径通常为 `$HOME/.espressif/v6.1/esp-idf`。在EIM“打开终端”生成的shell使用即可。其他终端需要激活时，先确认脚本存在：

```bash
test -f "$HOME/.espressif/tools/activate_idf_v6.1.sh" &&   . "$HOME/.espressif/tools/activate_idf_v6.1.sh"
```

脚本不存在就回EIM打开，不拼接其他版本路径。固件用EIM自己的Python；Bridge用另外的venv，不往EIM环境塞应用依赖。

## 2. 工作区

推荐真实代码在 `$HOME/Workspace/bot-status`。先检查该目录是否存在/是否有Git变更；本包可放其根目录或 `handoff/`。已有项目不能覆盖。

```bash
mkdir -p "$HOME/Workspace/bot-status/vendor"
cd "$HOME/Workspace/bot-status/vendor"
# 仅当 waveshare-1.75 目录不存在时执行clone
git clone https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75.git waveshare-1.75
cd waveshare-1.75
git rev-parse HEAD
```

使用标准1.75仓库，B版不是C版。[S01][S02]

## 3. 编译（此步不写开发板）

```bash
cd "$HOME/Workspace/bot-status/vendor/waveshare-1.75/examples/esp-idf/02_lvgl_demo_v9" &&   idf.py -B build-idf61 set-target esp32s3 &&   idf.py -B build-idf61 build
```

第一次会解析BSP/LVGL组件；保存dependencies.lock。官方指南目前明确验证5.5.5/6.0.2，用户目标6.1应先本机编译验证，不未经同意换版本。[S02][S03][S04]

构建失败记录第一条真实编译/API错误及上下文；不要只发最后的build failed。不用全盘升级依赖“碰运气”。

## 4. 接板与只读备份

使用可传数据USB-C线直连Mac。列出端口并通过拔插确认：

```bash
ls /dev/cu.*
python -m esptool version
```

在已有EIM工具中使用对应esptool；下面是v5连字符语法，先确认help支持。若版本不同，在独立tools venV装 `esptool>=5,<6`，不要升级EIM内部工具。

```bash
export PORT='/dev/cu.usbmodem1101' # 改为真实确认的端口
python -m esptool --chip esp32s3 --port "$PORT" flash-id
```

核对ESP32-S3与16MB。不同容量/不同板型先停止，不能按标题强刷。

```bash
export BACKUP_DIR="$HOME/Workspace/bot-status-private-backups/$(date +%Y%m%d-%H%M%S)"
umask 077
mkdir -p "$BACKUP_DIR"
python -m esptool --chip esp32s3 --port "$PORT" --baud 460800   read-flash 0 ALL "$BACKUP_DIR/factory-full.bin"
stat -f '%z bytes' "$BACKUP_DIR/factory-full.bin"
shasum -a 256 "$BACKUP_DIR/factory-full.bin" > "$BACKUP_DIR/factory-full.bin.sha256"
```

16MiB应为16777216字节。备份可能含配网数据，不提交仓库。[S15] 记录大小+hash仅证明备份文件记录，不等于恢复测试已通过。

## 5. 人工确认后烧录官方示例

关闭Bridge与monitor，核对固件、端口、备份与schema，再执行：

```bash
cd "$HOME/Workspace/bot-status/vendor/waveshare-1.75/examples/esp-idf/02_lvgl_demo_v9"
idf.py -B build-idf61 -p "$PORT" flash monitor
```

`Ctrl+]`退出monitor。不先erase整片、不将1.75C/bin或其他build的partition混在一起。[S02]

验收：自己编译的画面显示、触摸读数正确、15分钟无Panic/白屏/反复复位。之后才复制为firmware工程开发Face。

## 6. 卡住时

下载失败：先排线/端口占用，再按板子官方BOOT/复位说明进入下载模式；PWR不是可随意当RESET使用的按钮。不要拆壳短接引脚。无串口先看USB数据线，标准板原生USB无需默认装CH340驱动。[S01][S05]

白屏：先恢复同一构建的官方示例，检查电源初始化、panel初始化、flush回调和依赖锁，不猜PMIC寄存器。触摸反向只在board_port修正一次。

## 下一步

按 `docs/10_IMPLEMENTATION_PLAN.md` 执行T03起的合同/状态/交互任务。本页命令是开发环境操作；完整Bridge命令在实现之前并不存在，不应照着执行后误以为安装失败。
