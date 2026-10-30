# -*- coding: utf-8 -*-
from pathlib import Path
import sys, json, subprocess, asyncio, wave, math, re, hashlib
import numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageOps
import imageio_ffmpeg

ROOT=Path(__file__).resolve().parent
PROJECT=ROOT.parent.parent
WORK=PROJECT/'build'/'video-1080p'
WORK.mkdir(parents=True,exist_ok=True)
sys.path.insert(0,str(PROJECT/'build'/'video-python'))
FF=imageio_ffmpeg.get_ffmpeg_exe()
W,H=1920,1080
SCENES=[
('01','让美的空调，接入米家与 HomeKit','原显示板 Wi-Fi 接口 · 本地串口桥接','operation',0,
'美的空调，也能原生接入米家和 HomeKit。这套主卧项目利用原显示板自带的 Wi-Fi 模块接口，通过 ESP32-S3 转换协议，保留空调真实的控制与反馈。'),
('02','已验证的实机组合','美的 i青春Ⅱ × 小米巨省电 1.5 匹拆机模块','photo',0,
'目前稳定使用的组合，是美的 i青春二代，搭配小米巨省电一点五匹空调的拆机模块。模块负责接入米家，ESP32 扮演它认识的空调主控。'),
('03','接线，从两路串口开始','ESP32-S3 · 共地 · 3.3V 逻辑','wiring',0,
'美的串口接收用 GPIO 十七，发送用十六。米家业务串口接收用七，发送用八。收发交叉、信号共地，并确认三点三伏逻辑电平。'),
('04','两种语言，一个状态模型','MIoT 文本指令 ⇄ 美的二进制帧','protocol',0,
'米家一侧是十一万五千二百波特的文本协议，美的一侧是九千六百波特的二进制帧。ESP32 解析指令、映射属性，再组帧和计算校验。'),
('05','先抓包，再确定每一个功能','get_down → set_properties → Midea 0x40','capture',0,
'控制命令通过 get down 获取。逐项操作米家和美居，对比串口抓包，才能确定属性编号、数据单位、方向和标志位，形成可靠的功能映射。'),
('06','0.5℃设温，不丢失半度位','27.0℃ → 27.5℃ → 28.0℃','temperature',8,
'温控支持零点五度步进。代码把设温拆成整数温度和半度标志位，再写进美的控制帧；米家和网页显示随真实反馈更新。'),
('07','七档风速，自由映射百级调速','米家 7 档 → 美的 1–100 协议风速值','fan',0,
'米家的七档风速，对应美的百级调速。每档数值都能在网页自由设置，写入和回读共用一张映射表，保存后重启仍然保留。'),
('08','开机、关机倒计时交给空调执行','分钟制 · 原生定时槽 · 回包确认','timer',0,
'开机和关机倒计时，分别写入美的原生定时槽。分钟数和开关方向统一转换，并等待空调回包确认，关闭网页后仍由空调执行。'),
('09','保留美的的常用控制体验','上下扫风 · 左右扫风 · ECO · 屏显','operation',17,
'上下扫风、左右扫风、模式、ECO 和屏显等常用控制，都通过美的协议映射。辅热和自清洁也接入对应功能，实际支持以空调为准。'),
('10','控制有回应，状态能同步','美的 C0 回包 → 米家属性 / HomeKit','homekit',0,
'串口收到美的有效回包后，程序更新统一状态，再反馈给米家和 HomeKit。控制目标与设备回报分开处理，避免界面显示成功、空调却没有执行。'),
('11','把适配中的问题逐一解决','单位与方向 · 引号解析 · 关机风速保持','issues',0,
'适配中解决了五分钟变五小时、倒计时方向颠倒，以及字符串解析造成的错误。关机回包中的自动风速不再覆盖原设定，重新开机沿用有效档位。'),
('12','达成任一条件，发布全部源代码','ESP32-S3 主卧 · 项目已投入日常使用','outro',0,
'项目已在主卧稳定使用，不同拆机模块仍需抓包适配。评论超过一百条，或者点赞超过一千次，届时发布全部源代码。'),
]
BASE_SCENES=list(SCENES)
STORY=ROOT/'1080P-分镜与旁白稿.md'
if STORY.exists():
 content=STORY.read_text(encoding='utf-8-sig')
 sections=re.findall(r'^##\s+[^\n]*?秒：([^\n]+)\n(.*?)(?=^##\s|\Z)',content,re.M|re.S)
 if len(sections)!=len(SCENES):raise ValueError('旁白稿场景数与视频不一致，请保留 12 个分镜标题。')
 for i,(title,body) in enumerate(sections):
  narrative=body.split('画面：',1)[0].strip().replace('👉','通过')
  if not narrative:raise ValueError('分镜旁白不能为空')
  visual=re.search(r'画面：([^；\n]+)；辅助标题：([^\n]+)',body)
  old=SCENES[i]
  kind=visual.group(1).strip() if visual else old[3]
  subtitle=visual.group(2).strip().replace('\\_','_') if visual else old[2]
  SCENES[i]=(old[0],title.strip(),subtitle,kind,old[4],narrative)

def run(args):
 p=subprocess.run([FF,'-hide_banner','-loglevel','error','-y']+list(map(str,args)),capture_output=True,cwd=str(WORK))
 if p.returncode: raise RuntimeError(p.stderr.decode('utf-8','replace')[-3000:])

async def voices():
 import edge_tts
 for i,s in enumerate(SCENES):
  out=WORK/f'voice-{i:02}.mp3'
  sig=hashlib.sha256(s[5].encode('utf8')).hexdigest();sigfile=WORK/f'voice-{i:02}.sha256'
  oldsig=sigfile.read_text().strip() if sigfile.exists() else hashlib.sha256(BASE_SCENES[i][5].encode('utf8')).hexdigest()
  if not out.exists() or out.stat().st_size<1000 or sig!=oldsig:
   for attempt in range(3):
    try:
     await edge_tts.Communicate(s[5], 'zh-CN-XiaoxiaoNeural',rate='+4%',volume='+0%').save(str(out))
     break
    except Exception:
     if attempt==2:raise
     await asyncio.sleep(2)
  sigfile.write_text(sig)
  print('voice',i+1,flush=True)

def F(size,bold=False,mono=False):
 p=r'C:\Windows\Fonts\consola.ttf' if mono else (r'C:\Windows\Fonts\msyhbd.ttc' if bold else r'C:\Windows\Fonts\msyh.ttc')
 return ImageFont.truetype(p,size)
def text(d,xy,t,size=30,color='#eaf4ff',bold=False,mono=False,anchor=None):
 d.text(xy,t,font=F(size,bold,mono),fill=color,anchor=anchor)
def wrap(t,n=32):return '\n'.join(t[i:i+n] for i in range(0,len(t),n))
def card(d,box,fill='#102c42',line='#285476',radius=22):d.rounded_rectangle(box,radius,fill=fill,outline=line,width=2)
def photo_fit(im,box):
 return ImageOps.contain(im,(box[2]-box[0],box[3]-box[1]),Image.Resampling.LANCZOS)

def base(i):
 a=np.zeros((H,W,3),dtype=np.uint8); y=np.arange(H)/H
 for c,(b,k) in enumerate([(7,9),(20,13),(32,17)]):a[:,:,c]=(b+k*y).astype('uint8')[:,None]
 im=Image.fromarray(a);d=ImageDraw.Draw(im)
 for x in range(0,W,96):d.line((x,0,x,H),fill='#102b40')
 for y in range(0,H,96):d.line((0,y,W,y),fill='#102b40')
 d.ellipse((1350,-360,2260,550),fill='#102c47')
 d.rectangle((80,65,90,100),fill='#44d5c8')
 text(d,(110,64),'ESP32-S3  /  MIDEA × MI HOME',25,'#99b3c9',True)
 text(d,(1840,73),f'{i+1:02} / 12',24,'#8ca8be',anchor='ra')
 text(d,(80,140),SCENES[i][1],52,'#ffffff',True)
 text(d,(82,220),SCENES[i][2],29,'#55d7d0')
 return im

def code_card(im,title,lines,explain):
 d=ImageDraw.Draw(im);card(d,(700,325,1840,875),'#091725')
 text(d,(740,350),title,25,'#5bdccd',True)
 for j,line in enumerate(lines):text(d,(738,412+j*45),line,23,'#d8e7f4',mono=True)
 for j,(a,b) in enumerate(explain):
  text(d,(90,355+j*150),a,32,'#ffffff',True)
  text(d,(90,408+j*150),b,25,'#a9c2d8')

def make_scene(i):
 s=SCENES[i];kind=s[3];im=base(i);d=ImageDraw.Draw(im)
 if kind=='photo':
  pic=Image.open(ROOT/'实物照片.jpg').convert('RGB');pic=ImageOps.exif_transpose(pic).crop((250,400,2150,3400))
  box=(1150,300,1740,905);p=photo_fit(pic,box);im.paste(p,(box[0]+(box[2]-box[0]-p.width)//2,box[1]))
  for j,(a,b) in enumerate([('美的 i青春Ⅱ','利用原显示板的 Wi-Fi 模块接口'),('小米巨省电 · 1.5 匹','米家空调拆机模块，保留原生 App 接入'),('ESP32-S3 MINI','双向协议转换 + HomeKit + 网页管理')]):
   text(d,(100,340+j*170),a,38,'#ffffff',True);text(d,(100,405+j*170),b,28,'#aac7dc')
 elif kind=='wiring':
  for x,title,sub in [(85,'美的显示板接口','9600 · 8N1'),(735,'ESP32-S3','串口收发交叉连接'),(1385,'米家拆机模块','115200 · 8N1')]:
   card(d,(x,330,x+450,700));text(d,(x+225,380),title,32,'#ffffff',True,anchor='mm');text(d,(x+225,430),sub,24,'#8db6d4',anchor='mm')
  text(d,(310,510),'TX → GPIO17 RX',25,'#58ddd2',mono=True,anchor='mm');text(d,(310,580),'RX ← GPIO16 TX',25,'#ffcf8a',mono=True,anchor='mm')
  text(d,(960,520),'UART2  <-->  UART1',26,'#ffffff',mono=True,anchor='mm');text(d,(960,590),'统一空调状态模型',27,'#b2ccde',anchor='mm')
  text(d,(1610,510),'TX → GPIO7 RX',25,'#58ddd2',mono=True,anchor='mm');text(d,(1610,580),'RX ← GPIO8 TX',25,'#ffcf8a',mono=True,anchor='mm')
  d.line((120,770,1800,770),fill='#55d8ca',width=4);text(d,(960,815),'GND 共地 · 3.3V 逻辑 · 电源按模块规格接入',30,'#dceaf3',anchor='mm')
  text(d,(960,875),'米家模块连接 MCU 业务口，不是启动日志口',25,'#a1bfd5',anchor='mm')
 elif kind=='protocol':
  for j,(title,body) in enumerate([('① 获取米家控制','get_down\r\ndown set_properties 2 4 27.5'),('② 映射并发送美的帧','SIID / PIID → 统一状态\n0x40 · 温度 / 风速 / 扫风 / 校验'),('③ 等待实际反馈','C0 状态回包 → 校验与解析\nproperties_changed + HomeKit 通知')]):
   x=85+j*620;card(d,(x,360,x+560,790));text(d,(x+30,405),title,30,'#57ddd2',True)
   d.multiline_text((x+30,510),body,font=F(25),fill='#d8e8f6',spacing=35)
  text(d,(960,865),'命令目标 ≠ 实际状态；回包确认后反馈结果',31,'#f6cd8c',True,anchor='mm')
 elif kind=='capture':
  code_card(im,'miot_bridge.h · 指令解析入口',[
   'if (!strcmp(tokens[1], "set_properties")) {',
   '    set(tokens, count);',
   '    return;',
   '}',
   '',
   '// text protocol -> target state -> Midea UART',
   '// validate -> send -> match real response'],[
   ('操作一次，记录一组','分别抓取米家写入值与美居原始控制帧'),
   ('逐字节对照','确认温度、扫风、风速、定时等字段'),
   ('资料与实机互证','目录协议网页 + 固件代码 + 原始回包')])
 elif kind=='temperature':
  code_card(im,'midea_s3_bridge.ino · 0.5℃ 编码',[
   'int halfDegrees = int(temperature * 2.0f + 0.5f);',
   'const int whole = halfDegrees / 2;',
   'const bool half = (halfDegrees & 1) != 0;',
   '',
   'frame[12] = uint8_t((mode << 5)',
   '    | (half ? 0x10 : 0x00)',
   '    | (whole - 16));'],[
   ('半度设温','整数温度 + 半度标志位'),
   ('27.5℃ 正确表达','保持米家 / 网页 / 美的状态一致'),
   ('控制与回读闭环','收到有效状态后更新显示')])
 elif kind=='fan':
  text(d,(100,335),'默认七档映射 · 可在网页修改',33,'#ffffff',True)
  levels=[5,10,30,45,60,80,100]
  for j,v in enumerate(levels):
   x=105+j*245;top=770-v*3.2
   d.rounded_rectangle((x,top,x+170,775),15,fill='#167e99',outline='#55d8cd',width=2)
   text(d,(x+85,top-32),str(v),35,'#ffffff',True,anchor='mm');text(d,(x+85,820),f'第 {j+1} 档',27,'#aac8df',anchor='mm')
  text(d,(960,890),'1–100 严格递增 · 写入 / 回读共用映射 · NVS 持久保存',28,'#67dfd0',anchor='mm')
 elif kind=='timer':
  pic=Image.open(ROOT/'web控制页.png').convert('RGB');pic=pic.crop((0,1160,pic.width,1780))
  p=photo_fit(pic,(80,355,1210,865));im.paste(p,(80,355))
  text(d,(1300,390),'分钟，不是小时',35,'#ffffff',True);text(d,(1300,480),'开机 / 关机槽独立',32,'#58d9d0',True)
  text(d,(1300,575),'等待真实回包确认',30,'#aec8dc');text(d,(1300,650),'取消定时写入 0',30,'#aec8dc')
  text(d,(1300,730),'美的 / 米家 / 网页同步',28,'#58d9d0')
  text(d,(960,880),'美的原生倒计时 · 与美居云端预约分开',29,'#f5cf93',anchor='mm')
 elif kind=='homekit':
  pic=Image.open(ROOT/'homekit.png').convert('RGB');pd=ImageDraw.Draw(pic)
  # The whole pairing card is covered, including the supplied QR / pairing PIN.
  pd.rectangle((0,135,pic.width,660),fill='#edf3f7');pd.text((80,290),'HomeKit 配对信息已隐藏',font=F(48,True),fill='#43617a')
  p=photo_fit(pic,(850,315,1810,905));im.paste(p,(1100,315))
  for j,(a,b) in enumerate([('接收有效 C0 状态','帧长度、校验、状态新鲜度'),('更新统一状态','实际温度、模式、扫风与定时'),('通知两侧界面','米家属性上报 + HomeKit 特征通知')]):
   text(d,(100,350+j*170),a,34,'#ffffff',True);text(d,(100,410+j*170),b,26,'#a7c3d8')
 elif kind=='issues':
  for j,(a,b,c) in enumerate([('5 分钟 → 5 小时','单位 / 定时槽方向不一致','统一分钟数，开关定时分别映射'),('写入后返回 -4003','带引号的字符串未正确识别','完整解析参数，再按实际类型处理'),('关机后风速变自动','关机回包覆盖了有效风速','保留最后开机档位，主动设置优先')]):
   y=330+j*180;card(d,(90,y,1830,y+145));text(d,(125,y+30),a,33,'#f7c98c',True);text(d,(725,y+28),b,28,'#9ebbd1');text(d,(725,y+83),c,30,'#5ad9cc',True)
 elif kind=='outro':
  for x,label,value,unit in [(140,'评论超过','100','条'),(1060,'点赞超过','1000','次')]:
   card(d,(x,345,x+720,755),'#102e45','#35768a')
   text(d,(x+360,405),label,40,'#acd0df',True,anchor='mm')
   text(d,(x+340,550),value,134,'#57e0cd',True,anchor='mm')
   text(d,(x+580,590),unit,40,'#e4f3fa',True,anchor='mm')
  text(d,(960,552),'或',46,'#f5cf93',True,anchor='mm')
  text(d,(960,835),s[1].split('，',1)[-1],52,'#ffffff',True,anchor='mm')
 elif kind in ('operation','web'):
  # A moving video window is composited into this area by ffmpeg.
  card(d,(78,304,1842,914),'#0c1d2b')
 return im

def srt_stamp(sec):
 n=round(sec*1000);h,n=divmod(n,3600000);m,n=divmod(n,60000);s,n=divmod(n,1000)
 return f'{h:02}:{m:02}:{s:02},{n:03}'

def make_subtitles(durations):
 cursor=0;blocks=[];idx=1;sub_images=[]
 for i,(s,dur) in enumerate(zip(SCENES,durations)):
  # Short phrases follow the narration in order; timings use their character lengths.
  pieces=re.split(r'(?<=[，。；])',s[5]);parts=[];buf=''
  for p in pieces:
   if len(buf)+len(p)>32 and buf:parts.append(buf);buf=''
   buf+=p
  if buf:parts.append(buf)
  weight=sum(len(p) for p in parts);local=0
  for part in parts:
   span=(dur-0.30)*len(part)/weight
   blocks.append(f'{idx}\n{srt_stamp(cursor+local)} --> {srt_stamp(cursor+local+span)}\n{part}\n')
   idx+=1;local+=span
  cursor+=dur
 (ROOT/'ESP32-S3-主卧-1080P-字幕.srt').write_text('\n'.join(blocks),encoding='utf-8-sig')
 return blocks

def music(seconds=120):
 sr=44100;n=int(seconds*sr);out=np.zeros(n,dtype=np.float32)
 bpm=92;beat=60/bpm; chords=[(130.81,164.81,196),(110,130.81,164.81),(87.31,110,130.81),(98,123.47,146.83)]
 for b in range(int(seconds/beat)+1):
  start=int(b*beat*sr)
  if start>=n:break
  length=min(int(beat*1.6*sr),n-start);t=np.arange(length,dtype=np.float32)/sr
  c=chords[(b//8)%4];freq=c[b%3]*2
  tone=(np.sin(2*np.pi*freq*t)+.22*np.sin(4*np.pi*freq*t))*np.exp(-t*3.3)*.055
  tone*=np.minimum(t/.008,1);out[start:start+length]+=tone
  if b%2==0:
   ln=min(int(.18*sr),n-start);tt=np.arange(ln)/sr
   out[start:start+ln]+=(np.sin(2*np.pi*(52*tt+10*(1-np.exp(-tt*30))/30))*np.exp(-tt*24)*.08).astype(np.float32)
 for block in range(int(seconds/(beat*8))+1):
  start=int(block*beat*8*sr)
  if start>=n:break
  ln=min(int(beat*8*sr),n-start);t=np.arange(ln,dtype=np.float32)/sr;c=chords[block%4]
  env=np.minimum(t/.6,1)*np.minimum((ln/sr-t)/.7,1)
  out[start:start+ln]+=sum(np.sin(2*np.pi*f*t) for f in c)*env*.018
 fade=int(sr*2);out[:fade]*=np.linspace(0,1,fade);out[-fade:]*=np.linspace(1,0,fade)
 # Derive music ducking from the generated narration without depending on
 # FFmpeg's older sidechain format negotiation.
 with wave.open(str(ROOT/'ESP32-S3-主卧-1080P-人声.wav'),'rb') as wf:
  voice=np.frombuffer(wf.readframes(wf.getnframes()),dtype='<i2').astype(np.float32)/32768
  voice=voice.reshape(-1,wf.getnchannels()).mean(axis=1)
 step=2205;levels=[]
 for j in range(0,n,step):
  block=voice[j:min(j+step,len(voice))]
  levels.append(.28 if len(block) and np.sqrt(np.mean(block*block))>.014 else .68)
 gains=np.interp(np.arange(n),np.arange(len(levels))*step,levels).astype(np.float32)
 out*=gains
 stereo=np.column_stack((out,out*.96));pcm=(np.clip(stereo,-1,1)*32767).astype('<i2')
 with wave.open(str(WORK/'music.wav'),'wb') as w:w.setparams((2,2,sr,0,'NONE','not compressed'));w.writeframes(pcm.tobytes())

def build():
 raw=[]
 for i in range(len(SCENES)):
  run(['-i',WORK/f'voice-{i:02}.mp3','-ac','1','-ar','44100',WORK/f'raw-{i:02}.wav'])
  with wave.open(str(WORK/f'raw-{i:02}.wav'),'rb') as w:raw.append(w.getnframes()/w.getframerate())
 timeline=WORK/'timeline.json'
 tempo=sum(raw)/(120-.35*len(SCENES))
 durations=[rd/tempo+.35 for rd in raw]
 durations[-1]=120-sum(durations[:-1])
 timeline.write_text(json.dumps(durations),encoding='utf8')
 print('timeline fitted to current narration: 120 seconds',flush=True)
 for i,rd in enumerate(raw):
  dur=durations[i];factor=rd/(dur-.35)
  run(['-i',WORK/f'raw-{i:02}.wav','-af',f'atempo={factor:.6f},apad=pad_dur=0.35','-t',dur,'-ac','2',WORK/f'audio-{i:02}.wav'])
 # Concatenated narration track.
 (WORK/'audio.txt').write_text(''.join(f"file 'audio-{i:02}.wav'\n" for i in range(len(SCENES))),encoding='utf8')
 run(['-f','concat','-safe','0','-i','audio.txt','-c:a','pcm_s16le',ROOT/'ESP32-S3-主卧-1080P-人声.wav'])
 make_subtitles(durations);music()
 run(['-ss','12','-i',ROOT/'网页.mp4','-t','10','-an','-c:v','libx264','-crf','18','-preset','fast',WORK/'safe-web.mp4'])
 story=['# ESP32-S3 主卧 · 120 秒视频分镜与旁白','','中文人声：晓晓神经网络合成；配乐：原创合成轻电子音乐。','']
 cursor=0
 for i,(s,dur) in enumerate(zip(SCENES,durations)):
  story += [f'## {cursor:.1f}–{cursor+dur:.1f} 秒：{s[1]}','',s[5],'',f'画面：{s[3]}；辅助标题：{s[2]}',''];cursor+=dur
 (ROOT/'1080P-分镜与旁白稿-生成时间轴.md').write_text('\n'.join(story),encoding='utf8')
 for i,(s,dur) in enumerate(zip(SCENES,durations)):
  im=make_scene(i);png=WORK/f'scene-{i:02}.png';im.save(png)
  if i==0:im.save(ROOT/'1080P-封面.png')
  clip_sig=hashlib.sha256(json.dumps([s,round(dur,5),3],ensure_ascii=False).encode('utf8')).hexdigest()
  clip_sigfile=WORK/f'clip-{i:02}.sha256'
  if (WORK/f'clip-{i:02}.mp4').exists() and (WORK/f'clip-{i:02}.mp4').stat().st_size>1000 and clip_sigfile.exists() and clip_sigfile.read_text()==clip_sig:
   print('reuse scene',i+1,flush=True);continue
  kind=s[3];args=['-loop','1','-framerate','30','-i',png]
  if kind in ('operation','web'):
   src=(WORK/'safe-web.mp4') if kind=='web' else (ROOT/'操作视频.mp4')
   args+=['-stream_loop','-1','-ss',0 if kind=='web' else s[4],'-i',src]
   # Crop private pairing area from web footage; original source stays untouched.
   vf=('[1:v]crop=1100:1080:400:0,scale=1760:600:force_original_aspect_ratio=decrease,pad=1760:600:(ow-iw)/2:(oh-ih)/2:color=0x0c1d2b[foot];' if kind=='web' else '[1:v]crop=1920:950:0:100,scale=1760:600:force_original_aspect_ratio=decrease,pad=1760:600:(ow-iw)/2:(oh-ih)/2:color=0x0c1d2b[foot];')
   args+=['-filter_complex',vf+'[0:v][foot]overlay=80:310[v]','-map','[v]']
  else:args+=['-vf','scale=1920:1080']
  args+=['-t',f'{dur:.6f}','-an','-c:v','libx264','-preset','fast','-crf','19','-pix_fmt','yuv420p','-r','30',WORK/f'clip-{i:02}.mp4']
  run(args);clip_sigfile.write_text(clip_sig);print('render scene',i+1,flush=True)
 (WORK/'clips.txt').write_text(''.join(f"file 'clip-{i:02}.mp4'\n" for i in range(len(SCENES))),encoding='utf8')
 run(['-f','concat','-safe','0','-i','clips.txt','-c','copy',WORK/'picture.mp4'])
 # Burn Chinese subtitles using libass; the subtitle path is local and ASCII.
 import shutil
 shutil.copyfile(ROOT/'ESP32-S3-主卧-1080P-字幕.srt',WORK/'captions.srt')
 out=ROOT/'ESP32-S3-主卧-120秒-1080P.mp4'
 filters="[1:a]loudnorm=I=-16:TP=-1.5:LRA=9,aresample=44100[vo];[2:a]volume=0.8[bg];[vo][bg]amix=inputs=2:duration=first:dropout_transition=0,volume=2[a];[0:v]subtitles=captions.srt:force_style='FontName=Microsoft YaHei,FontSize=14,PrimaryColour=&H00FFFFFF,OutlineColour=&H00121B27,BorderStyle=1,Outline=1.5,Shadow=0,MarginV=20'[v]"
 p=subprocess.run([FF,'-hide_banner','-loglevel','error','-y','-i','picture.mp4','-i',str(ROOT/'ESP32-S3-主卧-1080P-人声.wav'),'-i','music.wav','-filter_complex',filters,'-map','[v]','-map','[a]','-t','120','-c:v','libx264','-crf','19','-preset','fast','-c:a','aac','-b:a','192k','-movflags','+faststart',str(out)],cwd=str(WORK),capture_output=True)
 if p.returncode:raise RuntimeError(p.stderr.decode('utf8','replace')[-3000:])
 print('DONE',str(out),flush=True)

if __name__=='__main__':
 if '--voice' in sys.argv:asyncio.run(voices())
 elif '--build' in sys.argv:build()
