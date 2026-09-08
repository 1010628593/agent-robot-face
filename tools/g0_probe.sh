#!/bin/zsh
# G0 音频契约探针 —— 烧录 + 抓日志（开发验证专用）
#
# 用法：  . tools/g0_probe.sh [PORT]
#   例： . tools/g0_probe.sh /dev/cu.usbmodem101
#
# 做两件事：
#   1) 烧录 G0 探针固件（bootloader + 分区表 + app）
#   2) 抓 45 秒串口日志到 /tmp/g0_probe.log，并高亮 @g0 汇总行
#
# 关键：必须用 `.` / `source` 执行，否则 idf 环境不会留在当前 shell。
# 探针运行期间：前 ~2 秒请保持安静，之后 ~4 秒请对着板子拍手或说话。

PORT=${1:-/dev/cu.usbmodem101}
BAUD=921600
LOG=/tmp/g0_probe.log
CAPTURE_SECONDS=45

cd "$(dirname "$0")/../firmware" || return 1

echo "[g0] 端口         : $PORT"
echo "[g0] 固件         : build/bot_status.bin"
echo "[g0] 日志         : $LOG"

# 1) 激活 IDF 环境（EIM v6.1 + 补齐 cmake）
source ../tools/idf-env.sh || return 1

# 2) 烧录。注意：探针固件会取代正常 UI，跑完需重新烧正常固件。
echo "[g0] 开始烧录 ..."
python -m esptool --chip esp32s3 -p "$PORT" -b "$BAUD" \
    --before default_reset --after hard_reset write_flash \
    --flash-mode dio --flash-size 16MB --flash-freq 80m \
    0x0 build/bootloader/bootloader.bin \
    0x8000 build/partition_table/partition-table.bin \
    0x10000 build/bot_status.bin || return 1

echo "[g0] 烧录完成，等待 2 秒后开始抓日志 ..."
sleep 2

# 3) 抓日志（原生 USB CDC，波特率忽略）
echo "[g0] 抓日志 ${CAPTURE_SECONDS}s —— 请：前 2 秒安静，之后拍手/说话"
python - "$PORT" "$LOG" "$CAPTURE_SECONDS" <<'PY'
import sys, time, serial
port, out, dur = sys.argv[1], sys.argv[2], float(sys.argv[3])
try:
    ser = serial.Serial(port, 115200, timeout=0.5)
except Exception as e:
    print("[g0] 打开串口失败:", e); sys.exit(1)
# 复位后重新开始的日志更好读；这里直接接收探针输出
start = time.time()
with open(out, "wb") as f:
    while time.time() - start < dur:
        n = ser.in_waiting or 1
        data = ser.read(n)
        if data:
            f.write(data)
            f.flush()
ser.close()
print("[g0] 抓日志结束")
PY

echo "================= @g0 汇总行 ================="
grep -a "@g0" "$LOG" || echo "(未找到 @g0 汇总行，请看完整日志)"
echo "================= g0 探针过程 ================="
grep -a -E "\(g0\)|G0" "$LOG" | tail -60
echo "=============================================="
echo "[g0] 完整日志: $LOG"
