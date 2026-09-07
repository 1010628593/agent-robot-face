#!/usr/bin/env python3
import pathlib, plistlib, re, struct, sys, zlib
root, app = map(pathlib.Path, sys.argv[1:])
resources = app/'Contents/Resources'
def chunk(kind, payload):
    return struct.pack('>I', len(payload))+kind+payload+struct.pack('>I', zlib.crc32(kind+payload)&0xffffffff)
# Use full-quality artwork, not a lossy RGB565 round trip from the device.
for name in ('codex', 'workbuddy', 'cursor', 'hermes'):
    (resources / (name+'.png')).write_bytes((root/'assets/tool-icons/rendered'/(name+'.png')).read_bytes())
info=dict(CFBundleIdentifier='com.agentrobotface.menu', CFBundleName='Agent Robot Face', CFBundleDisplayName='Agent Robot Face', CFBundleExecutable='AgentRobotFace', CFBundlePackageType='APPL', CFBundleVersion='3',CFBundleShortVersionString='3.0.0',LSMinimumSystemVersion='14.0',LSUIElement=True,NSHighResolutionCapable=True,BridgeProjectPath=str(root))
(app/'Contents/Info.plist').write_bytes(plistlib.dumps(info))
# Product face icon: opaque pale eyes and fixed black pupils on black.
size=256
rows=[]
for y in range(size):
    row=bytearray([0])
    for x in range(size):
        eye=any(((x-cx)/41)**2+((y-128)/54)**2 <= 1 for cx in (78, 178))
        pupil=any(((x-cx)/17)**2+((y-128)/28)**2 <= 1 for cx in (78,178))
        row.extend((201,220,255) if eye and not pupil else (0,0,0))
    rows.append(row)
png=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',size,size,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(b''.join(rows)))+chunk(b'IEND',b'')
(resources/'AppIcon.png').write_bytes(png)
info['CFBundleIconFile']='AppIcon.icns'
(app/'Contents/Info.plist').write_bytes(plistlib.dumps(info))
