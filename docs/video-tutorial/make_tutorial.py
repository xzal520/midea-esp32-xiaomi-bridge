# -*- coding: utf-8 -*-
"""Render the local tutorial as an H.264/AAC video with narration and chapters."""
from pathlib import Path
import hashlib, json, math, re, subprocess, sys, wave
import imageio_ffmpeg
from PIL import Image, ImageDraw, ImageFont, ImageOps
from storyboard import SCENES

HERE=Path(__file__).resolve().parent
PROJECT=HERE.parent.parent
WORK=PROJECT/'build'/'video-tutorial'
WORK.mkdir(parents=True,exist_ok=True)
sys.path.insert(0,str(PROJECT/'build'/'video-python'))
FF=imageio_ffmpeg.get_ffmpeg_exe()
W,H,FPS=1920,1080,25
CHAPTERS=list(dict.fromkeys(s['chapter'] for s in SCENES))
OUT=HERE/'美的空调接入米家与HomeKit-详细教程-1080P.mp4'
INK='#163d34'; MUTED='#59716a'; GREEN='#19755b'; GOLD='#d69a36'
FONT='C:/Windows/Fonts/msyh.ttc'; BOLD='C:/Windows/Fonts/msyhbd.ttc'

def run(args):
    r=subprocess.run([FF,'-hide_banner','-loglevel','error','-y']+list(map(str,args)),cwd=str(WORK),capture_output=True)
    if r.returncode: raise RuntimeError(r.stderr.decode('utf-8','replace')[-4000:])

def font(n,bold=False): return ImageFont.truetype(BOLD if bold else FONT,n)
def txt(d,xy,t,n=30,color=INK,bold=False,anchor=None):
    d.text(xy,t,font=font(n,bold),fill=color,anchor=anchor)
def lines(text,width,n):
    f=font(n); result=[]; line=''
    for c in text:
        if c=='\n': result.append(line);line='';continue
        if f.getlength(line+c)>width and line: result.append(line);line=c
        else: line+=c
    if line:result.append(line)
    return result
def paragraph(d,xy,t,width,n=30,color=INK,bold=False,spacing=14):
    x,y=xy
    for line in lines(t,width,n):txt(d,(x,y),line,n,color,bold);y+=n+spacing
    return y
def roundbox(d,box,fill='white',outline='#d8e5dd',radius=20,width=2):
    d.rounded_rectangle(box,radius,fill=fill,outline=outline,width=width)
def fit_photo(im,box,path):
    p=ImageOps.exif_transpose(Image.open(path)).convert('RGB')
    p=ImageOps.contain(p,(box[2]-box[0],box[3]-box[1]),Image.Resampling.LANCZOS)
    x=box[0]+(box[2]-box[0]-p.width)//2;y=box[1]+(box[3]-box[1]-p.height)//2
    im.paste(p,(x,y));return x,y,p.width,p.height

def make_base(i):
    s=SCENES[i];im=Image.new('RGB',(W,H),'#eef3ee');d=ImageDraw.Draw(im)
    d.rectangle((0,0,W,12),fill=GREEN)
    txt(d,(62,38),'MIDEA × MIJIA  /  ESP32-S3 操作教程',25,MUTED,True)
    txt(d,(1858,38),f'{i+1:02d} / {len(SCENES):02d}',25,MUTED,anchor='ra')
    txt(d,(60,91),s['title'],52,INK,True)
    for j,c in enumerate(CHAPTERS):
        x=60+j*258;active=c==s['chapter']
        roundbox(d,(x,183,x+246,229),GREEN if active else '#e0e9e2',outline=None,radius=10)
        txt(d,(x+123,206),c,21,'white' if active else '#60796e',True,anchor='mm')
    roundbox(d,(60,256,1210,899))
    txt(d,(1245,271),'本步操作',26,GREEN,True)
    for j,p in enumerate(s['points']):
        y=328+j*175
        d.ellipse((1247,y,1291,y+44),fill=GREEN)
        txt(d,(1269,y+21),str(j+1),22,'white',True,anchor='mm')
        paragraph(d,(1310,y-2),p,528,31,INK,True,spacing=12)
    d.rectangle((60,918,1860,924),fill='#ceddd2')
    d.rectangle((60,918,60+round(1800*(i+1)/len(SCENES)),924),fill=GREEN)
    d.rectangle((0,941,W,H),fill='#112e27')
    return im

def ui_header(d,label='设置',address='http://midea-s3.local/'):
    roundbox(d,(82,277,1188,873),'#f6f8f6',outline='#dce5df',radius=14)
    d.rounded_rectangle((98,291,1172,335),8,fill='#e4eae5')
    for j,color in enumerate(['#de8f78','#dfc46b','#72b894']):d.ellipse((111+j*21,305,121+j*21,315),fill=color)
    txt(d,(207,300),address,20,MUTED)
    txt(d,(110,352),'Midea Bridge',25,INK,True)
    for j,item in enumerate(['空调控制','智能接入','设备状态','串口调试','设置']):
        x=112+j*210;active=item==label
        roundbox(d,(x,400,x+194,441),'white' if active else '#e8ede8',outline=None,radius=8)
        txt(d,(x+97,421),item,20,GREEN if active else MUTED,True,anchor='mm')
    txt(d,(113,838),'操作步骤示意 · 数值与设备名称以实际页面为准',19,MUTED)
def field(d,x,y,label,value='',w=790,secret=False):
    txt(d,(x,y),label,23,INK,True)
    roundbox(d,(x,y+36,x+w,y+92),'white','#cddcd1',10)
    txt(d,(x+17,y+49),'●●●●●●●●' if secret else value,24,MUTED)
def button(d,box,label,color=GREEN):
    roundbox(d,box,color,None,10);txt(d,((box[0]+box[2])/2,(box[1]+box[3])/2),label,24,'white',True,anchor='mm')

def draw_browser(im,s):
    d=ImageDraw.Draw(im);ui=s['ui']
    label='智能接入' if ui.startswith(('homekit','mijia')) else ('空调控制' if ui in ('control','comfort','timer') else ('设备状态' if ui=='status' else ('串口调试' if ui=='debug' else '设置')))
    address='https://你的教程站点/#provision' if ui.startswith('ble') else ('http://192.168.4.1/' if ui=='ap-form' else 'http://midea-s3.local/')
    ui_header(d,label,address)
    if ui.startswith('ble'):
        title={'ble':'蓝牙配网','ble-connect':'选择你的蓝牙设备','ble-auth':'验证设备身份','ble-wifi':'选择无线网络'}[ui]
        txt(d,(125,470),title,32,INK,True)
        if ui in ('ble','ble-connect'):
            field(d,125,535,'附近的设备','Midea-S3-Bridge-XXXX')
            button(d,(125,680,1085,746),'连接蓝牙设备' if ui=='ble' else '选中目标设备 → 确认连接')
        elif ui=='ble-auth':
            field(d,125,535,'设备网页登录密码',secret=True)
            button(d,(125,680,1085,746),'验证并继续')
        else:
            field(d,125,518,'附近的 2.4 GHz Wi-Fi','Home-WiFi-2.4G')
            field(d,125,637,'Wi-Fi 密码',w=590,secret=True)
            button(d,(740,671,1110,731),'保存 Wi-Fi 并重启')
    elif ui=='ap':
        txt(d,(125,470),'手机 Wi-Fi 设置',32,INK,True)
        for j,t in enumerate(['Home-WiFi-2.4G','Midea-S3-UART-Setup','其他网络']):
            y=535+j*82;roundbox(d,(125,y,1115,y+66),'#e5f3e9' if j==1 else 'white',None,10);txt(d,(150,y+19),t,26,GREEN if j==1 else MUTED,True)
    elif ui=='ap-form':
        txt(d,(125,465),'连接家里的 Wi-Fi',30,INK,True)
        for j,(l,v) in enumerate([('Wi-Fi 名称','Home-WiFi-2.4G'),('Wi-Fi 密码','●●●●●●●●'),('当前设备网页登录密码','●●●●●●●●')]):
            y=520+j*85;txt(d,(130,y),l,23,INK,True);roundbox(d,(490,y-7,1110,y+45),'white','#d6e4d9',10);txt(d,(508,y+3),v,24,MUTED)
        button(d,(128,779,1110,822),'保存设置并重启')
    elif ui=='login':
        txt(d,(220,485),'设备管理网页登录',34,INK,True)
        field(d,220,558,'设备当前网页登录密码',w=700,secret=True)
        button(d,(220,707,920,768),'登录')
    elif ui in ('control','comfort'):
        txt(d,(142,479),'空调控制',32,INK,True);txt(d,(620,508),'27.5 °C',75,GREEN,True,anchor='mm')
        for j,t in enumerate(['开机','关机','查询状态']):button(d,(130+j*335,588,440+j*335,647),t)
        for j,t in enumerate(['切换模式','切换风速','上下摆风','ECO','睡眠']):
            x=130+j*199;button(d,(x,700,x+180,757),t)
    elif ui=='timer':
        txt(d,(130,476),'开关机倒计时',32,INK,True)
        for j,t in enumerate(['倒计时开机','倒计时关机']):
            x=130+j*520;txt(d,(x,544),t,27,GREEN,True);txt(d,(x,602),'01 小时  00 分钟',27,INK)
            button(d,(x,674,x+472,730),'设置'+t);txt(d,(x+120,762),'取消 / 查询倒计时',23,MUTED)
    elif ui in ('fan','fan-save'):
        txt(d,(130,480),'米家七档风速映射',31,INK,True)
        for j,v in enumerate([5,10,30,45,60,80,100]):
            x=130+j*142;txt(d,(x+54,558),f'第 {j+1} 档',22,INK,True,anchor='mm');roundbox(d,(x,594,x+112,660),'white','#c9ddd0',10);txt(d,(x+56,626),str(v),30,GREEN,True,anchor='mm')
        button(d,(130,722,650,782),'保存映射');button(d,(700,722,1116,782),'恢复默认',MUTED)
    elif ui=='wifi':
        txt(d,(130,475),'Wi-Fi 连接设置',31,INK,True)
        field(d,130,533,'Wi-Fi 名称（SSID）','Home-WiFi-2.4G',w=745)
        field(d,130,656,'Wi-Fi 密码',w=745,secret=True)
        button(d,(905,567,1120,625),'扫描网络');button(d,(905,689,1120,751),'保存并重启')
    elif ui=='password':
        txt(d,(130,474),'修改网页登录密码',31,INK,True)
        for j,l in enumerate(['当前密码','新密码（6–64 个字符）','确认新密码']):
            y=539+j*75;txt(d,(130,y),l,24,INK,True);roundbox(d,(565,y-10,1115,y+45),'white','#ccdcd1',10);txt(d,(590,y),'●●●●●●●●',24,MUTED)
        button(d,(130,776,1115,822),'保存密码')
    elif ui=='hostname':
        txt(d,(130,475),'网络名称',31,INK,True)
        field(d,130,540,'主机名（向路由器报告）','主卧空调',w=460)
        field(d,650,540,'访问地址（不填写 .local）','midea-bedroom',w=460)
        txt(d,(135,681),'保存后访问：http://midea-bedroom.local/',26,GREEN,True)
        button(d,(130,740,1110,802),'保存并重启')
    elif ui=='bluetooth':
        txt(d,(130,482),'蓝牙功能',32,INK,True);txt(d,(150,581),'启用 BLE 蓝牙',32,INK,True)
        roundbox(d,(875,565,1080,630),GREEN,None,33);d.ellipse((1019,573,1071,623),fill='white')
        paragraph(d,(150,699),'切换后设备将短暂重启；关闭后 BLE 客户端无法发现设备。',900,28,MUTED)
    elif ui.startswith('homekit'):
        txt(d,(130,475),'HomeKit',34,INK,True)
        roundbox(d,(145,539,387,783),'#e6eee8','#a6c9b4',12)
        paragraph(d,(173,583),'使用自己设备的\n二维码或配对码',196,26,GREEN,True)
        if ui=='homekit':items=['1. 查看配对状态','2. 打开 iPhone 家庭 App','3. 添加配件']
        elif ui=='homekit-add':items=['1. 家庭 App：添加配件','2. 扫码或手动输入配对码','3. 设置名称 / 房间并测试']
        else:items=['1. 网页：清除 HomeKit 配对','2. 家庭 App：移除旧配件','3. 重新添加']
        for j,t in enumerate(items):txt(d,(460,557+j*83),t,27,INK,True)
    elif ui.startswith('mijia'):
        txt(d,(130,478),'米家模块',32,INK,True)
        for j,(a,b) in enumerate([('串口状态','查看是否识别'),('模块网络','查看连接状态'),('协议 / 握手','查看是否完成')]):
            y=548+j*65;txt(d,(150,y),a,26,MUTED);txt(d,(580,y),b,26,GREEN,True)
        button(d,(130,755,600,813),'同步美的状态');button(d,(650,755,1110,813),'进入米家模组配网')
    elif ui=='status':
        txt(d,(130,477),'设备状态',32,INK,True)
        for j,(a,b) in enumerate([('CPU / 内存','运行指标'),('固件版本 / 时间','以实际设备为准'),('当前 IP / Wi-Fi 信号','网络信息'),('BLE / 网络健康','连接与恢复状态')]):
            y=548+j*67;txt(d,(145,y),a,26,MUTED);txt(d,(670,y),b,26,GREEN,True)
    elif ui=='debug':
        txt(d,(130,473),'串口调试',32,INK,True);roundbox(d,(130,532,1120,781),'#112e27',None,12)
        for j,t in enumerate(['时间       来源        方向','RX        美的空调     空调 → ESP32','TX        米家模块     ESP32 → 模块','日志用于核对指令和有效回包']):txt(d,(155,551+j*55),t,25,'#a1e2bc')
    elif ui=='advanced':
        txt(d,(130,478),'高级调试功能',32,INK,True)
        for j,t in enumerate(['IO16 独立 RX：暂停美的发送','脱机模拟：不控制真实空调','测试上报：不是实际传感器读数']):
            y=551+j*85;roundbox(d,(130,y,1120,y+67),'#fff4df',None,10);txt(d,(150,y+18),t,27,'#7b5520',True)
    elif ui=='ota':
        txt(d,(130,477),'固件更新',32,INK,True);field(d,130,552,'选择应用固件','Midea-Bridge-版本-ESP32-S3-MINI-web-OTA.bin',w=985)
        button(d,(130,708,1115,777),'上传并安装 → 等待重启 → 核对版本')
    else:
        txt(d,(130,476),'五个页面，按用途选择',32,INK,True)
        paragraph(d,(140,561),'控制空调 → 配置智能接入 → 查看设备状态\n读取串口日志 → 修改设置',950,35,GREEN,True,spacing=35)

def make_frame(i):
    s=SCENES[i];im=make_base(i);d=ImageDraw.Draw(im)
    visual=s['visual'];box=(80,276,1190,877)
    if visual in ('photo','interface'):
        x,y,pw,ph=fit_photo(im,box,PROJECT/'照片'/s['asset']);d=ImageDraw.Draw(im)
        if visual=='interface':
            rect=(int(x+pw*.285),int(y+ph*.414),int(x+pw*.351),int(y+ph*.595))
            d.rounded_rectangle(rect,10,outline='#ffce47',width=6)
            roundbox(d,(x+int(pw*.39),y+int(ph*.26),x+int(pw*.73),y+int(ph*.34)),GREEN,None,8)
            txt(d,(x+int(pw*.42),y+int(ph*.275)),'Wi-Fi 接口焊线处',23,'white',True)
    elif visual=='wiring':
        rows=[('空调 TX','GPIO17 · RX','9600 / 8N1'),('空调 RX','GPIO16 · TX','ESP32 → 空调'),('米家 MCU_TX','GPIO7 · RX','115200 / 8N1'),('米家 MCU_RX','GPIO8 · TX','ESP32 → 模块')]
        txt(d,(120,294),'对方接口',25,MUTED,True);txt(d,(600,294),'ESP32 引脚',25,MUTED,True)
        for j,(a,b,c) in enumerate(rows):
            y=355+j*100;active=s.get('highlight',-1)==j or (s.get('highlight')==2 and j==3)
            roundbox(d,(105,y,1160,y+83),'#e0f0e5' if active else '#f2f6f2',None,12)
            txt(d,(130,y+17),a,32,INK,True);txt(d,(501,y+17),'→' if j%2==0 else '←',40,GREEN,True);txt(d,(598,y+17),b,32,GREEN,True)
        txt(d,(130,791),'VIN：经测量确认的供电     GND：公共地',31,INK,True)
        txt(d,(130,840),'串口采用 3.3 V 逻辑；不要按线色盲接',25,'#9b6820',True)
    elif visual=='browser':draw_browser(im,s)
    elif visual=='flash':
        txt(d,(125,300),'实机录屏 · 固件刷写.mp4',28,GREEN,True)
    elif visual=='files':
        for j,(a,b) in enumerate([('首次串口刷写','…full-flash-16MB.bin'),('以后网页更新','…web-OTA.bin')]):
            y=370+j*210;roundbox(d,(110,y,1160,y+164),'#e8f3e9',None,15);txt(d,(145,y+24),a,32,GREEN,True);txt(d,(145,y+91),b,34,INK,True)
        txt(d,(115,824),'下载入口：教程页面中的夸克网盘链接',27,MUTED)
    elif visual=='network':
        for j,(a,b) in enumerate([('已连路由器','管理网页 → 设置'),('未连路由器','蓝牙网页 → 验证 → 发送 Wi-Fi'),('持续离线','临时热点 → 192.168.4.1')]):
            y=340+j*158;roundbox(d,(110,y,1160,y+125),'#e5f1e7',None,14);txt(d,(145,y+18),a,31,GREEN,True);txt(d,(145,y+68),b,30,INK)
    else:
        txt(d,(130,330),'按顺序完成本步',38,GREEN,True)
        for j,t in enumerate(s['points']):paragraph(d,(140,440+j*125),f'{j+1:02d}  '+t,970,36,INK,True)
    p=WORK/f'scene-{i:02d}.png';im.save(p)
    return im

def voices():
    subprocess.run([sys.executable, str(HERE/'synthesize_local.py')], check=True)

def timestamp(t,ass=False):
    unit=100 if ass else 1000;value=round(t*unit)
    h,value=divmod(value,3600*unit);m,value=divmod(value,60*unit);sec,sub=divmod(value,unit)
    return f'{h}:{m:02}:{sec:02}.{sub:02}' if ass else f'{h:02}:{m:02}:{sec:02},{sub:03}'

def captions(i,voice_duration):
    bp=WORK/f'voice-{i:02d}.json'; words=json.loads(bp.read_text(encoding='utf-8')) if bp.exists() else []
    result=[];group=[];n=0
    for w in words:
        group.append(w);n+=len(w['text'])
        if n>=25:
            result.append((group[0]['offset']/1e7+.35,(group[-1]['offset']+group[-1]['duration'])/1e7+.35,''.join(x['text'] for x in group)));group=[];n=0
    if group:result.append((group[0]['offset']/1e7+.35,(group[-1]['offset']+group[-1]['duration'])/1e7+.35,''.join(x['text'] for x in group)))
    if not result:
        text=SCENES[i]['narration'];parts=re.findall('.{1,28}',text)
        result=[(.35+j*voice_duration/len(parts),.35+(j+1)*voice_duration/len(parts),t) for j,t in enumerate(parts)]
    result=[(a,min(b,voice_duration+.35),t) for a,b,t in result if a<voice_duration+.35]
    header='''[Script Info]\nScriptType: v4.00+\nPlayResX: 1920\nPlayResY: 1080\nWrapStyle: 2\n[V4+ Styles]\nFormat: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, Encoding\nStyle: Default,Microsoft YaHei,40,&H00FFFFFF,&H00FFFFFF,&H00212A22,&H00000000,0,0,0,0,100,100,0,0,1,1.5,0,2,80,80,46,1\n[Events]\nFormat: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n'''
    for a,b,t in result:
        header+=f'Dialogue: 0,{timestamp(a,True)},{timestamp(b,True)},Default,,0,0,0,,{t}\n'
    (WORK/f'scene-{i:02d}.ass').write_text(header,encoding='utf-8-sig');return result

def build():
    timeline=[];allcaps=[];cursor=0;chapter_starts={}
    for i,s in enumerate(SCENES):
        wav=WORK/f'voice-{i:02d}.wav'
        if not wav.exists():
            run(['-i',WORK/f'voice-{i:02d}.mp3','-ac','1','-ar','48000',wav])
        with wave.open(str(wav),'rb') as f:vd=f.getnframes()/f.getframerate()
        dur=max(vd+1.7,float(s.get('end',0))-float(s.get('start',0)))+float(s.get('hold',0))
        dur=math.ceil(dur*FPS)/FPS
        if s['chapter'] not in chapter_starts:chapter_starts[s['chapter']]=cursor
        local=captions(i,vd);allcaps.extend((cursor+a,cursor+b,t) for a,b,t in local)
        timeline.append(dict(index=i,start=cursor,duration=dur,voice=vd,chapter=s['chapter'],title=s['title']))
        cursor+=dur
    (WORK/'timeline.json').write_text(json.dumps(timeline,ensure_ascii=False,indent=2),encoding='utf-8')
    print(f'TIMELINE {cursor:.2f} seconds ({cursor/60:.1f} minutes)',flush=True)
    for i,(s,t) in enumerate(zip(SCENES,timeline)):
        png=WORK/f'scene-{i:02d}.png';make_frame(i)
        sig=hashlib.sha256((json.dumps([s,{k:t[k] for k in ('duration','voice')}],ensure_ascii=False)+'render-v5').encode()).hexdigest()
        output=WORK/f'clip-{i:02d}.mp4';sf=WORK/f'clip-{i:02d}.sha256'
        if output.exists() and sf.exists() and sf.read_text()==sig:
            print(f'RENDER {i+1}/{len(SCENES)} cached',flush=True);continue
        args=['-loop','1','-framerate',FPS,'-i',png]
        if s['visual']=='flash':
            args+=['-ss',s['start'],'-t',s['end']-s['start'],'-i',PROJECT/'照片/固件刷写.mp4']
            args+=['-i',WORK/f'voice-{i:02d}.wav']
            crop={'top':'crop=780:650:1280:310,','bottom':'crop=780:700:1280:720,'}.get(s.get('crop'),'')
            vf=f'[1:v]{crop}scale=1100:620:force_original_aspect_ratio=decrease,pad=1100:620:(ow-iw)/2:(oh-ih)/2:color=0xf1f5f0,setsar=1,fps={FPS},tpad=stop_mode=clone:stop_duration=90[foot];[0:v][foot]overlay=85:273'
            audio='2:a'
        else:
            args+=['-i',WORK/f'voice-{i:02d}.wav'];vf='[0:v]null';audio='1:a'
        vf+=f",ass=scene-{i:02d}.ass,fade=t=in:st=0:d=0.22,fade=t=out:st={t['duration']-.25:.3f}:d=0.25[v]"
        vf+=f';[{audio}]adelay=350|350,apad,aresample=48000[a]'
        args+=['-filter_complex',vf,'-map','[v]','-map','[a]','-t',f"{t['duration']:.3f}",'-c:v','libx264','-preset','veryfast','-crf','21','-threads','4','-pix_fmt','yuv420p','-r',FPS,'-c:a','aac','-b:a','128k','-ac','2','-ar','48000',output]
        run(args);sf.write_text(sig);print(f'RENDER {i+1}/{len(SCENES)} {s["title"]}',flush=True)
    (WORK/'clips.txt').write_text(''.join(f"file 'clip-{i:02d}.mp4'\n" for i in range(len(SCENES))),encoding='utf-8')
    meta=';FFMETADATA1\ntitle=美的空调接入米家与HomeKit：接线、刷机、配网与设置\ncomment=基于项目安装与刷写教程；中文合成旁白；刷机画面使用固件刷写.mp4\n'
    ch=list(chapter_starts.items())
    for i,(name,start) in enumerate(ch):
        end=ch[i+1][1] if i+1<len(ch) else cursor
        meta+=f'[CHAPTER]\nTIMEBASE=1/1000\nSTART={round(start*1000)}\nEND={round(end*1000)}\ntitle={name}\n'
    (WORK/'chapters.ffmetadata').write_text(meta,encoding='utf-8')
    run(['-f','concat','-safe','0','-i','clips.txt','-f','ffmetadata','-i','chapters.ffmetadata','-map','0:v','-map','0:a','-map_metadata','1','-map_chapters','1','-c:v','copy','-af','aformat=channel_layouts=stereo,loudnorm=I=-16:TP=-1.5:LRA=9,aformat=sample_rates=48000:channel_layouts=stereo','-c:a','aac','-b:a','192k','-ac','2','-ar','48000','-movflags','+faststart',OUT])
    srt='\n'.join(f'{j+1}\n{timestamp(a)} --> {timestamp(b)}\n{t}\n' for j,(a,b,t) in enumerate(allcaps))
    (HERE/'详细教程-中文字幕.srt').write_text(srt,encoding='utf-8-sig')
    md=['# 详细教程视频 · 旁白与章节时间轴','','中文合成旁白：本机 Microsoft Huihui；画面 1920×1080 / 25 FPS。','刷写画面按顺序使用《固件刷写.mp4》，其余包括实物照片、接线图和标明用途的操作界面示意。','','## 章节','']
    for name,start in ch:md.append(f'- {int(start)//60:02d}:{int(start)%60:02d} {name}')
    for s,t in zip(SCENES,timeline):md+=['',f"## {int(t['start'])//60:02d}:{int(t['start'])%60:02d} {s['title']}",'',s['narration'],'']
    (HERE/'详细教程-旁白与章节.md').write_text('\n'.join(md),encoding='utf-8')
    (HERE/'章节定位.txt').write_text('\n'.join(f'{int(start)//60:02d}:{int(start)%60:02d} {name}' for name,start in ch),encoding='utf-8')
    make_frame(0).save(HERE/'详细教程-封面.png')
    print('DONE',str(OUT),flush=True)

def frames():
    for i in range(len(SCENES)):make_frame(i)
    sample=[0,1,6,11,15,21,24,28,32,34,37,42]
    sheet=Image.new('RGB',(1920,4*360),'#dfe9e0')
    for k,i in enumerate(sample):
        im=Image.open(WORK/f'scene-{i:02d}.png').resize((640,360),Image.Resampling.LANCZOS);sheet.paste(im,((k%3)*640,(k//3)*360))
    sheet.save(HERE/'详细教程-画面总览.jpg')

if __name__=='__main__':
    if '--frames' in sys.argv:frames()
    if '--voice' in sys.argv:voices()
    if '--build' in sys.argv:build()
