# -*- coding: utf-8 -*-
from pathlib import Path
import subprocess,zipfile,hashlib
import imageio_ffmpeg
from PIL import Image,ImageDraw,ImageFont
R=Path(__file__).resolve().parent;B=R.parent.parent/'build'/'video-1080p';FF=imageio_ffmpeg.get_ffmpeg_exe()
video=R/'ESP32-S3-主卧-120秒-1080P.mp4'
def run(args):
 p=subprocess.run([FF,'-hide_banner','-y']+list(map(str,args)),capture_output=True)
 if p.returncode:raise RuntimeError(p.stderr.decode('utf8','replace')[-2000:])
 return p
meta=subprocess.run([FF,'-hide_banner','-i',str(video)],capture_output=True).stderr.decode('utf8','replace')
for l in meta.splitlines():
 if 'Duration:' in l or 'Stream #0:' in l: print(l)
run(['-v','error','-i',video,'-f','null','-'])
for t in [3,24,46,56,66,78,95,107,116]:run(['-v','error','-ss',t,'-i',video,'-frames:v','1',B/f'qa-{t}.jpg'])
Image.open(B/'qa-3.jpg').save(R/'1080P-封面.png')
sheet=Image.new('RGB',(1440,810));d=ImageDraw.Draw(sheet);f=ImageFont.truetype(r'C:\Windows\Fonts\msyh.ttc',18)
for i,t in enumerate([3,24,46,56,66,78,95,107,116]):
 im=Image.open(B/f'qa-{t}.jpg').resize((480,270));sheet.paste(im,((i%3)*480,(i//3)*270))
sheet.save(R/'1080P-画面总览.jpg')
run(['-v','error','-i',R/'ESP32-S3-主卧-1080P-人声.wav','-c:a','libmp3lame','-b:a','192k',R/'1080P-独立旁白.mp3'])
run(['-v','error','-i',B/'music.wav','-c:a','libmp3lame','-b:a','192k',R/'1080P-原创配乐.mp3'])
names=[video.name,'ESP32-S3-主卧-1080P-字幕.srt','1080P-分镜与旁白稿.md','1080P-分镜与旁白稿-生成时间轴.md','1080P-制作说明.md','1080P-独立旁白.mp3','1080P-原创配乐.mp3','1080P-封面.png','1080P-画面总览.jpg']
with zipfile.ZipFile(R/'1080P-发布素材包.zip','w',compression=zipfile.ZIP_STORED) as z:
 for name in names:z.write(R/name,name)
print('bytes',video.stat().st_size)
print('sha256',hashlib.sha256(video.read_bytes()).hexdigest())
print('Full decode passed. Package complete.')
