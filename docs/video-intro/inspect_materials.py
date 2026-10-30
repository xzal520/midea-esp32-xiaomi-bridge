# -*- coding: utf-8 -*-
from pathlib import Path
import subprocess
import imageio_ffmpeg
from PIL import Image, ImageDraw, ImageFont
root=Path(__file__).resolve().parent
ff=imageio_ffmpeg.get_ffmpeg_exe()
font=ImageFont.truetype(r'C:\Windows\Fonts\simhei.ttf',22)
imgs=[]
for name in ['实物照片.jpg','web控制页.png','homekit.png']:
 p=root/name
 im=Image.open(p).convert('RGB'); print(name, im.size)
 imgs.append((name, im))
for name in ['网页.mp4','操作视频.mp4']:
 p=root/name
 r=subprocess.run([ff,'-i',str(p)],capture_output=True)
 print(name, r.stderr.decode('utf-8','replace')[-3000:])
 for sec in [2,12,25]:
  out=root/(name+f'-{sec}.jpg')
  subprocess.run([ff,'-y','-ss',str(sec),'-i',str(p),'-frames:v','1',str(out)],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
  if out.exists(): imgs.append((name+f' {sec}s',Image.open(out).convert('RGB')))
sheet=Image.new('RGB',(1500,((len(imgs)+2)//3)*400),'#182333');d=ImageDraw.Draw(sheet)
for i,(name,im) in enumerate(imgs):
 im.thumbnail((480,350));x=(i%3)*500;y=(i//3)*400
 sheet.paste(im,(x+(480-im.width)//2,y+35));d.text((x+10,y+5),name,font=font,fill='white')
sheet.save(root/'素材总览.jpg')
