#!/bin/zsh
# Bot Status v1 - ESP-IDF v6.1 (EIM) 构建环境助手
#
# 用法：  . tools/idf-env.sh
#
# 只做三件事：
#   1. 复用 EIM 已安装的 ESP-IDF v6.1（不重装、不降级、不改 EIM 目录）
#   2. 补齐 EIM 激活脚本遗漏的 cmake（EIM 把 cmake 装成 CMake.app，
#      其静态 PATH 指向 *.app 之外的路径，导致 idf.py 报 "cmake must be available"）
#   3. 打印版本，供证据记录
#
# 不修改 EIM 的任何文件，不安装/升级任何工具链。

EIM_ACTIVATE="$HOME/.espressif/tools/activate_idf_v6.1.sh"

if [ ! -f "$EIM_ACTIVATE" ]; then
  echo "[idf-env] 缺少 EIM 激活脚本: $EIM_ACTIVATE" >&2
  echo "[idf-env] 请回 EIM 打开 v6.1 终端；不要拼接其他版本路径。" >&2
  return 1 2>/dev/null || exit 1
fi

# shellcheck disable=SC1090
. "$EIM_ACTIVATE"

# 补齐 cmake：优先使用 EIM 已安装版本，找不到就报错而不是静默降级
if ! command -v cmake >/dev/null 2>&1; then
  # EIM 布局: <tools>/cmake/<ver>/CMake.app/Contents/bin/cmake （深度 5，maxdepth 必须 >= 5）
  EIM_CMAKE="$(find "$HOME/.espressif/tools/cmake" -maxdepth 6 -type f -name cmake 2>/dev/null | sort -V | tail -1)"
  if [ -n "$EIM_CMAKE" ]; then
    export PATH="$(dirname "$EIM_CMAKE"):$PATH"
    echo "[idf-env] 已补入 EIM cmake: $(dirname "$EIM_CMAKE")" >&2
  else
    echo "[idf-env] 未找到 EIM 安装的 cmake，且 PATH 中也没有。" >&2
    return 1 2>/dev/null || exit 1
  fi
fi

echo "[idf-env] IDF        : $(idf.py --version 2>/dev/null | tail -1)"
echo "[idf-env] IDF_PATH   : $IDF_PATH"
echo "[idf-env] cmake      : $(cmake --version 2>/dev/null | head -1)"
echo "[idf-env] ninja      : $(ninja --version 2>/dev/null)"
echo "[idf-env] python     : $(python -c 'import sys; print(sys.executable)' 2>/dev/null)"
