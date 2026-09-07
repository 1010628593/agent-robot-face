#!/bin/zsh
set -euo pipefail
DEST="${HOME}/Applications/Agent Robot Face.app"
if [[ -e "${DEST}" ]]; then
  IDENT=$(/usr/libexec/PlistBuddy -c 'Print CFBundleIdentifier' "${DEST}/Contents/Info.plist")
  [[ "${IDENT}" == com.agentrobotface.menu ]] || { print -u2 'Refusing to remove an unrelated application'; exit 1; }
  print 'Disable menu login startup and quit the app before uninstalling.'
  /bin/rm -rf -- "${DEST}"
fi
print 'Menu application removed. Bridge, source hooks, ledger and credentials retained.'
