"""Export nine touch scenarios through the actual C recognizer/pose/geometry.

Diagnostic rendering only: no assertions, no hardware simulation acceptance.
Requires Pillow and the compiled /tmp/replay_touch exporter (see replay_touch.c).
Run from repository root. Outputs reports/touch-table/.
"""
import json,math,subprocess
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
out=Path('reports/touch-table');out.mkdir(exist_ok=True)
def tap(t,x,y,starts):return [(x,y)] if any(a<=t<a+100 for a in starts) else []
def alternate(t):
 for i,a in enumerate(range(400,2200,300)):
  if a<=t<a+100:return [(170 if i%2==0 else 296,233)]
 return []
def cup(t):
 if not 400<=t<3900:return []
 off=int(30*math.sin((t-400)/800))
 return [(130+off,315),(336+off,315)] if t<2400 else [(336+off,315)]
def pinch(t):
 if not 400<=t<3900:return []
 d=226+80*math.sin((t-400)/500)
 return [(round(233-d/2),315),(round(233+d/2),315)]
scenes={
 '01-contact':lambda t:[(140,320)] if 400<=t<2400 else [],
 '02-eye-tap':lambda t:tap(t,170,233,(500,)),
 '03-repeat':lambda t:tap(t,130,320,(400,720,1040)),
 '04-alternate':alternate,
 '05-follow':lambda t:[(round(233+220*math.sin((t-400)/750)),233)] if 400<=t<4200 else [],
 '06-stroke':lambda t:[(round(233+65*math.sin((t-400)/190)),150)] if 400<=t<4700 else [],
 '07-cup-release':cup,
 '08-pinch-spread':pinch,
 '09-eyes-release':lambda t:[(170,233),(296,233)] if 400<=t<2300 else [(296,233)] if 2300<=t<3700 else []}
clips={};inputs={};summary={}
for name,fn in scenes.items():
 rows=[];inputs[name]=[]
 for t in range(0,6020,20):
  pts=fn(t);coords=[v for p in pts for v in p]+[0]*4
  rows.append(f'{t} {len(pts)} '+' '.join(map(str,coords[:4])))
  inputs[name].append(pts)
 source='\n'.join(rows)+'\n';(out/(name+'.touch')).write_text(source)
 for expression in (1,8,10,11,13,15,18,0):
  raw=subprocess.check_output(['/tmp/replay_touch',str(expression)],input=source,text=True)
  (out/f'{name}-{expression}.jsonl').write_text(raw)
  frames=[json.loads(s) for s in raw.splitlines()]
  summary[f'{name}-{expression}']={'min_left':min(x['pose'][2] for x in frames),'min_right':min(x['pose'][3] for x in frames),'max_roll':max(abs(x['roll']) for x in frames),'peak_streak':max(x['streak'] for x in frames),'peak_alternating':max(x['alternating'] for x in frames)}
  if expression==8:clips[name]=frames
(out/'scenario-summary.json').write_text(json.dumps(summary,indent=2))
font=ImageFont.truetype('/System/Library/Fonts/Supplemental/Arial.ttf',16)
frames=[]
for i in range(0,301,3):
 im=Image.new('RGB',(780,900),'#14191f');d=ImageDraw.Draw(im)
 d.text((14,10),'Nine touch responses / real C geometry / working state',font=font,fill='#c9dcff')
 d.text((14,32),f'{i*20} ms - orange dots: touch input. Offline replay, not a device video.',font=font,fill='#a0adbd')
 for j,(name,clip) in enumerate(clips.items()):
  cell=Image.new('RGB',(466,466),'black');p=ImageDraw.Draw(cell)
  for k,x1,y1,x2,y2,x3,y3,r,w,dark in clip[i]['geometry']:
   color='black' if dark else '#c9dcff'
   if k==0:p.rounded_rectangle((x1,y1,x2,y2),radius=max(0,r),fill=color)
   elif k==1:p.line((x1,y1,x2,y2),fill=color,width=max(1,round(w)))
   elif k==2:p.polygon([(x1,y1),(x2,y2),(x3,y3)],fill=color)
  for x,y in inputs[name][i]:p.ellipse((x-9,y-9,x+9,y+9),outline='#e4a853',width=3)
  mask=Image.new('L',(466,466));ImageDraw.Draw(mask).ellipse((0,0,465,465),fill=255)
  x=10+j%3*260;y=62+j//3*276
  im.paste(cell.resize((246,246)),(x,y),mask.resize((246,246)))
  d.text((x,y+250),name,font=font,fill='#c9dcff')
 frames.append(im)
frames[0].save(out/'nine-interactions.gif',save_all=True,append_images=frames[1:],duration=60,loop=0)
frames[43].save(out/'nine-interactions.png')
print('Exported 9 scenarios x 8 business expressions; working-state animation.')
