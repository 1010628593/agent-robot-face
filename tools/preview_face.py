#!/usr/bin/env python3
"""Compile/export the firmware's C geometry into an offline, interactive review.

No npm, network, device, third-party Python modules or independent JS motion model.
Canvas approximates LVGL rasterization; this is a design preview, not a panel capture.
"""
from __future__ import annotations
import argparse
import json
import subprocess
from pathlib import Path

NAMES = ["Disconnected", "Idle", "Blink", "Look left", "Look right", "Look up",
         "Look down", "Curious", "Working", "Thinking", "Tool", "Waiting",
         "Happy", "Done", "Surprised", "Error", "Sleep", "Attention"]
TEMPLATE = r'''<!doctype html><html lang="zh-CN"><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Bot Face · native motion preview</title>
<style>
*{box-sizing:border-box}body{margin:0;background:#101114;color:#d8e1f0;font:15px system-ui}
header,main{max-width:1120px;margin:auto;padding:24px}h1{font-size:27px;margin:0 0 8px}
p{color:#8c98aa;line-height:1.7}section{display:flex;gap:30px;align-items:center;flex-wrap:wrap}
canvas{display:block;background:#000;border-radius:50%;width:100%;height:auto}
#stage{width:min(466px,90vw)}aside{min-width:260px;flex:1}button,select{font:inherit;color:inherit;background:#20242b;border:1px solid #414a57;border-radius:8px;padding:9px 14px;margin:6px 8px 6px 0}
input{width:100%;margin:18px 0}#grid{display:grid;grid-template-columns:repeat(6,1fr);gap:18px;margin:32px 0}
figure{margin:0}figcaption{margin-top:10px;color:#9cacc1;text-align:center;font-size:12px}
@media(max-width:760px){#grid{grid-template-columns:repeat(3,1fr)}}
</style><header><h1>Bot Face / 纯眼睛动画</h1><p>SIM · 从固件 C 引擎导出的逐帧几何。无状态环、常驻标题、问号或嘴巴。Canvas 预览并非实机录像。</p></header>
<main><section><div id="stage"><canvas id="face" width="466" height="466"></canvas></div>
<aside><select id="choice" aria-label="表情"></select><button id="play">暂停</button><button id="restart">重新播放</button>
<p id="time"></p><input id="seek" aria-label="时间轴" type="range" min="0" max="149" value="0">
<p>待机自然眨眼；工作收拢眼睑；工具态左右观察；等待保持关注；完成是一次微笑；错误是一次短震后保持 X 眼。实际屏幕的长按与切页操作由原手势状态机负责。</p>
<p>Done / Error / Disconnected / Sleep 到末帧停住，不用循环伪造新事件。慢速抗烧屏位移在全局时钟上独立运行。</p></aside></section>
<div id="grid"></div></main>
<script>
const names=__NAMES__, clips=__CLIPS__, step=33;
const choice=document.querySelector('#choice'), seek=document.querySelector('#seek');
const ctx=document.querySelector('#face').getContext('2d');
let frame=0, playing=!matchMedia('(prefers-reduced-motion: reduce)').matches, anchor=performance.now();
function paint(c, list){
 c.clearRect(0,0,466,466);c.fillStyle='#000';c.fillRect(0,0,466,466);
 for(const [k,x1,y1,x2,y2,x3,y3,r,w,a,b,o,dark] of list){
  c.globalAlpha=o/255;c.fillStyle=c.strokeStyle=dark?'#000':'#c9dcff';c.beginPath();
  if(k===0){c.roundRect(x1,y1,x2-x1,y2-y1,r);c.fill();}
  else if(k===1){c.lineWidth=w;c.lineCap='round';c.moveTo(x1,y1);c.lineTo(x2,y2);c.stroke();}
  else if(k===2){c.moveTo(x1,y1);c.lineTo(x2,y2);c.lineTo(x3,y3);c.closePath();c.fill();}
  else if(k===3){c.lineWidth=w;c.lineCap='round';c.arc((x1+x2)/2,(y1+y2)/2,Math.max(1,r-w/2),a*Math.PI/180,b*Math.PI/180);c.stroke();}
 }
 c.globalAlpha=1;
}
function render(){let id=Number(choice.value);paint(ctx,clips[id][frame]);seek.value=frame;document.querySelector('#time').textContent=`${names[id]} · ${frame*step} ms · ${frame+1} / ${clips[id].length}`;document.querySelector('#play').textContent=playing?'暂停':'播放';}
names.forEach((name,id)=>{let op=document.createElement('option');op.value=id;op.textContent=name;choice.append(op);
 let f=document.createElement('figure'),c=document.createElement('canvas'),cap=document.createElement('figcaption');c.width=c.height=466;cap.textContent=name;f.append(c,cap);document.querySelector('#grid').append(f);
 paint(c.getContext('2d'),clips[id][id===2?30:20]);f.onclick=()=>{choice.value=id;frame=0;anchor=performance.now();render();};});
choice.value=1;choice.onchange=()=>{frame=0;anchor=performance.now();render();};
seek.oninput=()=>{frame=Number(seek.value);playing=false;render();};
document.querySelector('#play').onclick=()=>{playing=!playing;anchor=performance.now()-frame*step;render();};
document.querySelector('#restart').onclick=()=>{frame=0;anchor=performance.now();playing=true;render();};
function tick(now){if(playing){const id=Number(choice.value),n=clips[id].length;let f=Math.max(0,Math.floor((now-anchor)/step));
 if(f>=n && [0,13,15,16].includes(id)){f=n-1;playing=false;}else f%=n;
 if(frame!==f){frame=f;render();}}requestAnimationFrame(tick);}render();requestAnimationFrame(tick);
</script></html>'''

def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, default=Path(".build/face"))
    parser.add_argument("--output", type=Path, default=Path(".build/face/preview.html"))
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    build = args.build.resolve()
    subprocess.run(["cmake", "-S", str(root / "tests/face"), "-B", str(build)], check=True)
    subprocess.run(["cmake", "--build", str(build)], check=True)
    subprocess.run(["ctest", "--test-dir", str(build), "--output-on-failure"], check=True)
    clips = [json.loads(subprocess.check_output([str(build / "dump_face_frames"), str(i), "150", "33"], text=True)) for i in range(len(NAMES))]
    output = args.output.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(TEMPLATE.replace("__NAMES__", json.dumps(NAMES)).replace("__CLIPS__", json.dumps(clips, separators=(",", ":"))), encoding="utf-8")
    print(f"Offline C-geometry preview: {output}")

if __name__ == "__main__":
    main()
