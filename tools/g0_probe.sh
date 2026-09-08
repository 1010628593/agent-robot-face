#!/bin/zsh
# Execute, do not source. No build-directory/port defaults and no implicit flash.
# tools/g0_probe.sh --build firmware/build-g0 --port /dev/cu.usbmodem... --validate-only
# tools/g0_probe.sh --build firmware/build-g0 --port /dev/cu.usbmodem... --flash
set -euo pipefail
bot_tools_dir="${0:A:h}"
source "$bot_tools_dir/idf-env.sh"
python - "$@" <<'PY'
import argparse, datetime, hashlib, json, pathlib, re, subprocess, sys, threading, time

p = argparse.ArgumentParser(description="Validate an explicit G0 build, then optionally flash and capture it")
p.add_argument("--build", type=pathlib.Path, required=True)
p.add_argument("--port", required=True)
action = p.add_mutually_exclusive_group(required=True)
action.add_argument("--validate-only", action="store_true")
action.add_argument("--flash", action="store_true")
p.add_argument("--log", type=pathlib.Path)
p.add_argument("--seconds", type=int, default=180)
p.add_argument("--reset", choices=("usb", "uart"), default="usb")
a = p.parse_args()
if not 15 <= a.seconds <= 600:
    p.error("--seconds must be between 15 and 600")
build = a.build.expanduser().resolve(strict=True)
def require(condition, message):
    if not condition:
        raise SystemExit("[g0] Refused: " + message)
def load(name):
    return json.loads((build / name).read_text())
project = load("project_description.json")
require(project.get("project_name") == "bot_status" and project.get("target") == "esp32s3", "not an ESP32-S3 bot_status build")
require(pathlib.Path(project["build_dir"]).resolve() == build, "metadata points to another build directory")
config = load("config/sdkconfig.json")
header_path = build / "config/sdkconfig.h"
header = header_path.read_text()
for option in ("BOT_AUDIO_ENABLE", "BOT_AUDIO_G0_PROBE", "ESP_CONSOLE_USB_SERIAL_JTAG"):
    require(config.get(option) is True, f"{option} is not enabled in build config")
    require(re.search(rf"^#define CONFIG_{option} 1$", header, re.M), f"{option} missing from compiled config header")
require(not config.get("BOT_AUDIO_G0_VERIFIED", False), "development probe must not claim the release verification gate")
flash = load("flasher_args.json")
require(flash.get("extra_esptool_args", {}).get("chip") == "esp32s3", "flasher metadata chip mismatch")
files = []
for address, relative in flash["flash_files"].items():
    image = (build / relative).resolve(strict=True)
    require(image.is_relative_to(build), "flash image escapes explicit build directory")
    require(image.is_file(), "missing flash image")
    int(address, 0)
    files.append((address, image))
app = (build / project["app_bin"]).resolve(strict=True)
require(any(image == app for _, image in files), "app image missing from flasher metadata")
raw = app.read_bytes()
require(b"operator_review_required" in raw and b"active_mics_verified" in raw, "image does not contain the current G0 probe (stale or normal image)")
require(app.stat().st_mtime_ns >= header_path.stat().st_mtime_ns, "image predates configuration; rebuild the G0 target")
manifest = {
    "created_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
    "build": str(build), "port": a.port, "project_version": project.get("project_version"),
    "idf_revision": project.get("git_revision"),
    "config_sha256": hashlib.sha256(header.encode()).hexdigest(),
    "images": [{"offset": offset, "path": str(image), "sha256": hashlib.sha256(image.read_bytes()).hexdigest()} for offset, image in files],
    "g0_verified": False,
}
print(json.dumps(manifest, ensure_ascii=False, indent=2), flush=True)
if a.validate_only:
    print("[g0] Build validation only; device was not opened, reset or flashed.")
    raise SystemExit(0)
require(pathlib.Path(a.port).exists(), "explicit serial port does not exist")
import serial
from esptool.reset import HardReset
stamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
log = (a.log or pathlib.Path("/tmp") / f"g0-{stamp}.log").expanduser().resolve()
require(not log.exists() and not log.with_suffix(log.suffix + ".json").exists(), "log or sidecar already exists; choose a new path")
require(log.parent.is_dir(), "log parent directory does not exist")
# Do not boot the probe after flashing. Capture is opened before the one deliberate reset.
command = [sys.executable, "-m", "esptool", "--chip", "esp32s3", "--port", a.port,
           "--baud", "921600", "--before", "default-reset", "--after", "no-reset", "write-flash"]
command += flash["write_flash_args"]
for address, image in files:
    command += [address, str(image)]
print("[g0] Flashing explicitly selected DEVELOPMENT image; this replaces the normal UI.", flush=True)
subprocess.run(command, cwd=build, check=True)
# Start the receiver and open its output file BEFORE resetting. No post-reset sleep.
ser = serial.Serial(port=None, baudrate=int(project.get("monitor_baud", 115200)), timeout=0.1)
ser.dtr = False
ser.rts = False
ser.port = a.port
ser.open()
ready, stop = threading.Event(), threading.Event()
errors = []
def receive():
    try:
        with log.open("xb") as out:
            ready.set()
            deadline = time.monotonic() + a.seconds
            while not stop.is_set() and time.monotonic() < deadline:
                try:
                    data = ser.read(ser.in_waiting or 1)
                except (serial.SerialException, OSError):
                    # Native USB can re-enumerate on EN reset. Reopen promptly;
                    # missing phases are detected below instead of claiming success.
                    ser.close()
                    while not stop.is_set() and time.monotonic() < deadline:
                        try:
                            ser.open()
                            break
                        except (serial.SerialException, OSError):
                            time.sleep(0.025)
                    continue
                if data:
                    out.write(data)
                    out.flush()
                    sys.stdout.buffer.write(data)
                    sys.stdout.buffer.flush()
    except Exception as exc:
        errors.append(str(exc))
        ready.set()
thread = threading.Thread(target=receive, name="g0-capture", daemon=True)
thread.start()
ready.wait(5)
require(ready.is_set() and not errors, "capture could not become ready: " + str(errors))
print("[g0] Capture ready; resetting. Follow phase A quiet / phase B clap-talk prompts.", flush=True)
try:
    try:
        HardReset(ser, uses_usb=a.reset == "usb").reset()
    except (serial.SerialException, OSError) as exc:
        errors.append("reset transport error: " + str(exc))
    thread.join(a.seconds + 2)
finally:
    stop.set()
    thread.join(2)
    ser.close()
text = re.sub(r"\x1b\[[0-?]*[ -/]*[@-~]", "", log.read_bytes().decode("utf-8", errors="replace"))
summaries = []
for line in text.splitlines():
    if "@g0 " in line:
        try:
            summaries.append(json.loads(line.split("@g0 ", 1)[1]))
        except json.JSONDecodeError:
            pass
complete = not errors and "phase=A " in text and "phase=B " in text and any(
    summary.get("verdict") == "operator_review_required" for summary in summaries)
manifest.update({"capture_complete": complete, "capture_errors": errors, "summaries": summaries,
                 "log": str(log), "log_sha256": hashlib.sha256(log.read_bytes()).hexdigest()})
log.with_suffix(log.suffix + ".json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n")
print(f"[g0] Log: {log}; capture_complete={complete}; G0 still requires operator review.")
raise SystemExit(0 if complete else 1)
PY
