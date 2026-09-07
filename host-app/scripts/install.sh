#!/bin/zsh
set -euo pipefail
APP_ROOT="${0:A:h:h}"
"${APP_ROOT}/scripts/build.sh"
DEST="${HOME}/Applications/Agent Robot Face.app"
mkdir -p "${HOME}/Applications"
if [[ -e "${DEST}" ]]; then
  IDENT=$(/usr/libexec/PlistBuddy -c 'Print CFBundleIdentifier' "${DEST}/Contents/Info.plist")
  [[ "${IDENT}" == com.agentrobotface.menu ]] || { print -u2 'Refusing to replace an unrelated application'; exit 1; }
fi
/usr/bin/ditto "${APP_ROOT}/build/Agent Robot Face.app" "${DEST}"
/usr/bin/codesign --verify --strict "${DEST}"
print -r -- "Installed: ${DEST} (launch manually with open)"
