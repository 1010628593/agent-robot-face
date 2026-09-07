#!/bin/zsh
set -euo pipefail
APP_ROOT="${0:A:h:h}"
PROJECT_ROOT="${APP_ROOT:h}"
OUTPUT="${APP_ROOT}/build/Agent Robot Face.app"
mkdir -p "${OUTPUT}/Contents/MacOS" "${OUTPUT}/Contents/Resources"
python3 "${APP_ROOT}/scripts/package-resources.py" "${PROJECT_ROOT}" "${OUTPUT}"
/usr/bin/sips -s format icns "${OUTPUT}/Contents/Resources/AppIcon.png" --out "${OUTPUT}/Contents/Resources/AppIcon.icns" >/dev/null
xcrun swiftc -swift-version 6 -parse-as-library -O -target arm64-apple-macosx14.0 -sdk "$(xcrun --show-sdk-path)" -module-cache-path /tmp/agent-robot-face-swift-cache "${APP_ROOT}"/Sources/*.swift -o "${OUTPUT}/Contents/MacOS/AgentRobotFace"
codesign --force --sign - "${OUTPUT}"
codesign --verify --strict "${OUTPUT}"
print -r -- "${OUTPUT}"
