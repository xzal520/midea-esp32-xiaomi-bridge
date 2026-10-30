# -*- coding: utf-8 -*-
"""Website links and an illustrated Bluetooth walkthrough, without website QR codes."""
from pathlib import Path
import hashlib, json, math, re, shutil, sys, wave
from PIL import Image, ImageDraw, ImageOps
import numpy as np
import make_natural_audio as media
import remix_clear_air as remix
import add_follow_intro as intro
from website_segments import SITE, BLE, WEB_SEGMENTS

R = media.render
WORK, DEST, run = media.WORK, media.DEST, media.run
BASE_SCENES = R.SCENES[:49]
R.SCENES = BASE_SCENES + WEB_SEGMENTS
OUT = DEST / '美的空调接入米家与HomeKit-详细教程-含网页与蓝牙操作版-1080P.mp4'

def duration(path):
    with wave.open(str(path), 'rb') as f: return f.getnframes() / f.getframerate()

def center(d, xy, text, size=32, color=R.INK, bold=False):
    d.text(xy, text, font=R.font(size, bold), fill=color, anchor='mm')

def base(title, label='线上图文教程 · 蓝牙配网'):
    im = Image.new('RGB', (1920, 1080), '#eef3ee'); d = ImageDraw.Draw(im)
    d.rectangle((0, 0, 1920, 12), fill=R.GREEN)
    R.txt(d, (60, 38), 'MIDEA × MIJIA  /  ESP32-S3 操作教程', 25, R.MUTED, True)
    R.txt(d, (1860, 38), label, 24, R.GREEN, True, anchor='ra')
    R.txt(d, (60, 110), title, 52, R.INK, True)
    d.rectangle((0, 941, 1920, 1080), fill='#112e27')
    return im

def address(d, url=SITE):
    R.roundbox(d, (60, 190, 1860, 278), 'white', '#bdd5c7', 14)
    R.txt(d, (90, 218), '浏览器地址栏', 27, R.MUTED, True)
    R.txt(d, (320, 204), url, 48, R.GREEN, True)

def phone(im, image, x, y=302, h=583):
    shot = Image.open(WORK / image).convert('RGB')
    shot = ImageOps.contain(shot, (310, h), Image.Resampling.LANCZOS)
    d = ImageDraw.Draw(im)
    R.roundbox(d, (x-8, y-8, x+shot.width+8, y+shot.height+8), 'white', '#bdd5c7', 20)
    im.paste(shot, (x, y))
    return x, y, shot.width, shot.height

def pointer(d, x, y):
    d.ellipse((x-21, y-21, x+21, y+21), outline='#dda827', width=5)
    d.polygon([(x, y), (x+8, y+43), (x+17, y+31), (x+32, y+44), (x+40, y+35), (x+24, y+23), (x+38, y+16)], fill='white', outline=R.INK)

def step(d, x, y, number, title, detail, width=840, active=False):
    R.roundbox(d, (x, y, x+width, y+130), '#e1f0e5' if active else 'white', '#c8ddcf', 16)
    d.ellipse((x+20, y+23, x+76, y+79), fill=R.GREEN)
    center(d, (x+48, y+51), str(number), 28, 'white', True)
    R.txt(d, (x+99, y+18), title, 34, R.INK, True)
    R.paragraph(d, (x+99, y+73), detail, width-120, 25, R.MUTED, spacing=8)

def website_frame(segment, phase=0):
    kind = segment['kind']; im = base(segment['title']); d = ImageDraw.Draw(im)
    if kind == 'outro':
        R.txt(d, (65, 215), '把网址保留下来，跟着图文步骤继续操作', 35, R.MUTED)
        for y, title, url, detail in [(332, '安装与刷写教程', SITE, '接线、固件下载、刷机、配网、HomeKit 和各项设置'),
                                     (602, '直接打开蓝牙配网', BLE, '连接设备 → 验证密码 → 选择 Wi-Fi → 保存并重启')]:
            R.roundbox(d, (100, y, 1820, y+210), 'white', '#c7dccf', 22)
            R.txt(d, (135, y+22), title, 34, R.GREEN, True)
            R.txt(d, (135, y+80), url, 58 if url == SITE else 52, R.INK, True)
            R.txt(d, (135, y+155), detail, 28, R.MUTED)
        return im
    address(d, BLE if kind == 'ble' else SITE)
    left = phone(im, 'online-guide.png' if kind == 'overview' else 'online-bluetooth.png', 115)
    right = phone(im, 'online-bluetooth.png' if kind == 'overview' else 'online-bluetooth-connect.png', 455)
    d = ImageDraw.Draw(im)
    if kind == 'overview':
        step(d, 840, 330, 1, '安装与刷写教程', '按目录查看材料、接口、接线、刷机与设置', active=phase==0)
        step(d, 840, 495, 2, '蓝牙配网页面', '无需云端账号，在网页中连接设备并发送 Wi-Fi 设置', active=phase==1)
        step(d, 840, 660, 3, '顶部导航随时切换', '同一个网站，两个入口；直接输入上方网址即可', active=phase>=2)
        if phase >= 1:
            x,y,w,h = right; sy=h/722; sx=w/355
            d.rounded_rectangle((x+138*sx,y+67*sy,x+210*sx,y+110*sy), 5, outline='#dda827', width=4)
            pointer(d, x+190*sx, y+91*sy)
    else:
        step(d, 840, 330, 1, '点击顶部「蓝牙配网」', '或直接打开地址末尾带 #provision 的链接', active=phase==0)
        step(d, 840, 495, 2, '点击「连接蓝牙设备」', '设备已通电、蓝牙已开启，手机或电脑靠近设备', active=phase==1)
        step(d, 840, 660, 3, '按后续画面继续操作', '选择设备 → 验证设备密码 → 选择 Wi-Fi → 保存重启', active=phase>=2)
        if phase == 0:
            x,y,w,h=left; sx=w/355; sy=h/722
            d.rounded_rectangle((x+138*sx,y+67*sy,x+210*sx,y+110*sy),5,outline='#dda827',width=4)
            pointer(d,x+185*sx,y+88*sy)
        else:
            x,y,w,h=right; sx=w/355; sy=h/722
            d.rounded_rectangle((x+35*sx,y+267*sy,x+308*sx,y+320*sy),6,outline='#dda827',width=4)
            pointer(d,x+218*sx,y+295*sy)
    R.txt(d, (840, 850), '网页实景截图 · 下方继续逐步演示', 26, R.MUTED)
    return im

def panel(d, title, active=1):
    R.roundbox(d,(65,286,1215,900),'#fbfcfa','#c9dbce',18)
    R.roundbox(d,(85,304,1195,352),'#e7eee8',None,8)
    R.txt(d,(110,315),BLE,25,R.MUTED)
    for j,t in enumerate(['连接设备','选择网络','保存设置']):
        x=112+j*360
        d.ellipse((x,377,x+38,415),fill=R.GREEN if j+1<=active else '#d9e4dc')
        center(d,(x+19,396),str(j+1),22,'white' if j+1<=active else R.MUTED,True)
        R.txt(d,(x+52,379),t,26,R.GREEN if j+1<=active else R.MUTED,True)
    R.txt(d,(112,443),title,37,R.INK,True)
    R.txt(d,(110,859),'操作步骤示意 · 请以浏览器和实际设备返回为准',22,R.MUTED)

def demo_frame(index, phase):
    scene=BASE_SCENES[index]
    im=base(scene['title'],'蓝牙配网 · 操作步骤示意'); d=ImageDraw.Draw(im)
    notes={
        22:[('开启蓝牙，靠近设备','手机或电脑蓝牙保持开启'),
            ('选择对应的桥接设备','Midea-S3-Bridge- 开头'),
            ('确认系统配对提示','如果弹出请求，再点击确认')],
        23:[('输入设备当前网页密码','不是家里的 Wi-Fi 密码'),
            ('点击「验证并继续」','等待身份验证通过'),
            ('区分三个不同密码','设备网页 / 路由器 / 临时热点')],
        24:[('选择 2.4 GHz Wi-Fi','从扫描列表选择；隐藏网络可手填'),
            ('输入路由器 Wi-Fi 密码','开放网络可留空'),
            ('保存并确认重启','等待 ESP32 连接新网络')]
    }
    for j,(title,detail) in enumerate(notes[index]):
        step(d,1250,330+j*173,j+1,title,detail,width=600,active=j==min(phase,2))
    if index==22:
        panel(d,'在浏览器设备列表中选择目标')
        R.roundbox(d,(130,514,1140,652),'white','#b8cebf',12)
        if phase>=1:d.rounded_rectangle((132,516,1138,650),11,fill='#e2f0e7',outline=R.GREEN,width=4)
        R.txt(d,(160,536),'Midea-S3-Bridge-XXXX',37,R.INK,True)
        R.txt(d,(160,600),'选择与你实际设备对应的名称',26,R.MUTED)
        R.button(d,(760,710,1138,778),'确认连接')
        if phase==0:pointer(d,400,554)
        elif phase==1:pointer(d,640,563)
        else:pointer(d,940,746)
        if phase>=3:
            R.roundbox(d,(130,791,1138,840),'#fff0d7',None,9)
            R.txt(d,(150,800),'如系统弹出配对请求，请确认。',26,'#806221',True)
    elif index==23:
        panel(d,'验证设备身份')
        R.field(d,130,528,'设备网页登录密码','',w=1005)
        if phase>=1:R.txt(d,(151,579),'●●●●●●●●',33,R.INK)
        R.button(d,(130,692,1135,764),'验证并继续')
        R.txt(d,(135,799),'使用设备当前的网页登录密码，不是路由器密码。',26,R.MUTED)
        pointer(d,520,594) if phase<2 else pointer(d,705,723)
        if phase>=2:d.rounded_rectangle((128,690,1137,766),12,outline='#dda827',width=5)
    else:
        panel(d,'选择家里的 2.4 GHz Wi-Fi',2 if phase<4 else 3)
        R.field(d,130,518,'Wi-Fi 名称（SSID）','Home-WiFi-2.4G' if phase>=1 else '等待扫描，选择目标网络',w=1005)
        R.field(d,130,638,'Wi-Fi 密码','',w=690)
        if phase>=2:R.txt(d,(151,687),'●●●●●●●●',30,R.INK)
        R.button(d,(130,775,1135,837),'保存 Wi-Fi 并重启设备')
        pointer(d,775,578) if phase<2 else pointer(d,720,702) if phase<3 else pointer(d,800,800)
        if phase>=4:
            d.rectangle((75,365,1204,849),fill='#dfe9e1')
            R.roundbox(d,(180,476,1094,767),'white','#94b7a2',18)
            R.txt(d,(220,513),'保存 Wi-Fi 并重启设备？',37,R.INK,True)
            R.txt(d,(220,591),'确认后等待设备重启并连接路由器。',29,R.MUTED)
            R.button(d,(720,671,1049,734),'确定')
            R.button(d,(220,671,548,734),'取消',R.MUTED)
            pointer(d,880,701)
    return im

def render_clip(key, seconds, wav, caps_index, frames):
    caption_list=media.captions(caps_index,duration(wav))
    lines=[]
    for j,(fraction,image) in enumerate(frames):
        path=WORK/f'{key}-stage-{j}.png'; image.save(path)
        end=frames[j+1][0] if j+1<len(frames) else 1
        lines.append(f"file '{path.name}'\nduration {(end-fraction)*seconds:.6f}\n")
    lines.append(f"file '{key}-stage-{len(frames)-1}.png'\n")
    listing=WORK/f'{key}-frames.txt'; listing.write_text(''.join(lines),encoding='utf-8')
    clip=WORK/f'{key}.mp4'
    vf=(f'[0:v]fps=25,ass=scene-{caps_index:02d}.ass,fade=t=in:st=0:d=0.2,'
        f'fade=t=out:st={seconds-.25:.3f}:d=0.25[v];'
        '[1:a]adelay=350|350,apad,aresample=48000[a]')
    run(['-f','concat','-safe','0','-i',listing,'-i',wav,'-filter_complex',vf,'-map','[v]','-map','[a]',
         '-t',seconds,'-c:v','libx264','-preset','veryfast','-crf','21','-threads','4','-pix_fmt','yuv420p',
         '-r','25','-c:a','aac','-b:a','128k','-ac','2','-ar','48000',clip])
    print('RENDERED '+key,flush=True)
    return clip,caption_list

def build_entries():
    old=json.loads((WORK/'timeline.json').read_text(encoding='utf-8'))
    fresh={}
    for scene in WEB_SEGMENTS:
        slot=scene['slot']; wav=WORK/f'voice-{slot:02d}.wav'
        seconds=math.ceil((duration(wav)+2.1)*25)/25
        fractions=[0,.32,.67] if scene['kind']!='outro' else [0]
        clip,caps=render_clip('website-'+scene['kind'],seconds,wav,slot,
            [(f,website_frame(scene,min(j,2))) for j,f in enumerate(fractions)])
        fresh[slot]=dict(title=scene['title'],chapter=scene['chapter'],duration=seconds,
                        voice=wav,clip=clip,caps=caps,narration=scene['narration'])
    entries=[]
    follow_caps=intro.draw(music_credit='Music: “Clear Air” — Kevin MacLeod (incompetech.com)')
    entries.append(dict(title='帮忙点个关注',chapter='片头 · 帮忙点个关注',duration=8.8,
        voice=intro.VOICE,clip=WORK/'follow-intro-repaired-picture.mp4',caps=follow_caps,narration=intro.TEXT))
    entries.append(fresh[49])
    for i,(scene,t) in enumerate(zip(BASE_SCENES,old)):
        if i==21:
            entries.append(fresh[50]);continue
        wav=WORK/f'voice-{i:02d}.wav'; clip=WORK/f'clip-{i:02d}.mp4'
        if i in (22,23,24):
            fractions={22:[0,.28,.56,.82],23:[0,.18,.37,.65],24:[0,.18,.36,.54,.73]}[i]
            clip,caps=render_clip(f'ble-operation-{i}',t['duration'],wav,i,
                [(f,demo_frame(i,j)) for j,f in enumerate(fractions)])
        else:caps=media.captions(i,t['voice'])
        entries.append(dict(title=scene['title'],chapter=scene['chapter'],duration=t['duration'],
            voice=wav,clip=clip,caps=caps,narration=scene['narration']))
    entries.append(fresh[51])
    cursor=0
    for entry in entries:entry['start']=cursor;cursor+=entry['duration']
    return entries,round(cursor,6)

def join_voice(entries,total):
    output=WORK/'website-raw-narration.wav';sr=48000
    with wave.open(str(output),'wb') as sink:
        sink.setparams((1,2,sr,0,'NONE','not compressed'))
        delay=bytes(round(.35*sr)*2)
        for e in entries:
            with wave.open(str(e['voice']),'rb') as source:
                assert (source.getnchannels(),source.getsampwidth(),source.getframerate())==(1,2,sr)
                audio=source.readframes(source.getnframes())
            segment=delay+audio
            n=round(e['duration']*sr)*2
            if len(segment)>n:raise ValueError('Narration exceeds scene duration: '+e['title'])
            sink.writeframes(segment+bytes(n-len(segment)))
    normalized=WORK/'website-clean-narration.wav'
    run(['-i',output,'-af','aformat=channel_layouts=stereo,loudnorm=I=-16:TP=-1.5:LRA=9,'
         'aformat=sample_rates=48000:channel_layouts=stereo','-ac','2','-ar','48000',normalized])
    return normalized

def write_text(entries,total):
    allcaps=[];chapters=[];seen=set()
    for e in entries:
        if e['chapter'] not in seen:chapters.append((e['chapter'],e['start']));seen.add(e['chapter'])
        allcaps += [(e['start']+a,e['start']+b,t) for a,b,t in e['caps']]
    (DEST/'网页与蓝牙操作版-中文字幕.srt').write_text('\n'.join(
        f'{j+1}\n{R.timestamp(a)} --> {R.timestamp(b)}\n{t}\n' for j,(a,b,t) in enumerate(allcaps)),encoding='utf-8-sig')
    meta=(';FFMETADATA1\ntitle=美的空调接入米家与HomeKit：网页与蓝牙配网操作教程\n'
          'comment=Tutorial https://midea.00571.top/ ; music Clear Air by Kevin MacLeod (incompetech.com), CC BY 4.0\n')
    for j,(name,start) in enumerate(chapters):
        end=chapters[j+1][1] if j+1<len(chapters) else total
        meta+=f'[CHAPTER]\nTIMEBASE=1/1000\nSTART={round(start*1000)}\nEND={round(end*1000)}\ntitle={name}\n'
    (WORK/'website-chapters.ffmetadata').write_text(meta,encoding='utf-8')
    chtext='\n'.join(f'{int(t)//60:02d}:{int(t)%60:02d} {name}' for name,t in chapters)
    (DEST/'网页与蓝牙操作版-章节定位.txt').write_text(chtext,encoding='utf-8')
    md=['# 含网页与蓝牙操作版','','教程：'+SITE,'','蓝牙配网：'+BLE,'','仅直接显示网址，不制作网站二维码。','','## 章节','',chtext]
    for e in entries:
        md += ['',f"## {int(e['start'])//60:02d}:{int(e['start'])%60:02d} {e['title']}",'',e['narration']]
    (DEST/'网页与蓝牙操作版-旁白与章节.md').write_text('\n'.join(md),encoding='utf-8')
    (WORK/'website-timeline.json').write_text(json.dumps([{k:v for k,v in e.items() if k not in ('voice','clip','caps')} for e in entries],ensure_ascii=False,indent=2),encoding='utf-8')

def build():
    entries,total=build_entries()
    print(f'TIMELINE {total:.2f}s, {len(entries)} scenes',flush=True)
    write_text(entries,total)
    files=''.join(f"file '{e['clip'].name}'\nduration {e['duration']:.6f}\n" for e in entries)
    (WORK/'website-picture-list.txt').write_text(files,encoding='utf-8')
    picture=WORK/'website-picture.mp4'
    run(['-f','concat','-safe','0','-i',WORK/'website-picture-list.txt','-map','0:v:0','-an','-c:v','copy',picture])
    print('Mastering synchronized narration',flush=True)
    voice=join_voice(entries,total)
    remix.TOTAL=total
    music=remix.loop_music()
    graph=('[1:a]aformat=sample_fmts=fltp:sample_rates=48000:channel_layouts=stereo,asplit=2[voice][detector];'
           '[2:a]aformat=sample_fmts=fltp:sample_rates=48000:channel_layouts=stereo[bed];'
           '[bed][detector]sidechaincompress=threshold=0.04:ratio=3:attack=70:release=850:makeup=1[ducked];'
           '[voice][ducked]amix=inputs=2:duration=first:dropout_transition=0,volume=2,'
           'alimiter=limit=0.87:level=0,aformat=sample_rates=48000:channel_layouts=stereo[mix]')
    print('Mixing final website edition',flush=True)
    run(['-i',picture,'-i',voice,'-i',music,'-f','ffmetadata','-i',WORK/'website-chapters.ffmetadata',
         '-filter_complex',graph,'-map','0:v:0','-map','[mix]','-map_metadata','3','-map_chapters','3','-t',total,
         '-c:v','copy','-c:a','aac','-b:a','192k','-ac','2','-ar','48000','-movflags','+faststart',OUT])
    run(['-i',OUT,'-t','75','-map','0:v:0','-map','0:a:0','-map_chapters','-1','-c','copy','-movflags','+faststart',DEST/'网页与蓝牙操作版-开头预览.mp4'])
    start=next(e['start'] for e in entries if e['title']==WEB_SEGMENTS[1]['title'])
    end=next(e['start']+e['duration'] for e in entries if e['title']==BASE_SCENES[24]['title'])
    run(['-ss',start,'-i',OUT,'-t',end-start,'-map','0:v:0','-map','0:a:0','-map_chapters','-1','-c:v','libx264',
         '-preset','veryfast','-crf','21','-threads','4','-c:a','aac','-b:a','192k','-movflags','+faststart',DEST/'蓝牙配网操作-单独片段.mp4'])
    OUT.with_suffix('.mp4.sha256').write_text(hashlib.sha256(OUT.read_bytes()).hexdigest()+'  '+OUT.name+'\n',encoding='utf-8')
    (DEST/'网页与蓝牙操作版-制作说明.md').write_text(
        '# 含网页与蓝牙操作版\n\n'
        f'- 网页教程：{SITE}\n- 蓝牙配网：{BLE}\n'
        '- 按要求直接显示网址，不添加网站二维码。片头后介绍网站，配网章节展示入口及逐步操作，片尾再次显示网址。\n'
        '- 网站截图来自实际线上页面；设备选择、设备密码和 Wi-Fi 表单为明确标注的操作步骤示意，未声称录制了真实设备配网成功过程。\n'
        '- 蓝牙操作包含选择设备、系统配对确认、输入设备网页密码、选择 2.4 GHz SSID、填写 Wi-Fi 密码、保存并确认重启。\n'
        '- 保留自然女声、Clear Air 木吉他与钢琴配乐、原接线教程和《固件刷写.mp4》。署名与授权见同目录文件。\n'
        '- 人声按相同延迟拼接为单声道母版后统一转成立体声，避免左右声道错位。\n'
        '- 重新制作：`website_segments.py` 生成经授权的新增配音，`make_website_edition.py` 合成视频。\n',encoding='utf-8')
    print('COMPLETE '+str(OUT),flush=True)

if __name__=='__main__':build()
