# -*- coding: utf-8 -*-
"""Add the user-requested follow card, with the same narrator and quiet music."""
import asyncio, hashlib, json, math, re, subprocess, sys, wave
from pathlib import Path
from PIL import Image, ImageDraw
import make_natural_audio as media

TEXT = '教程开始前，帮忙点个关注吧。后续继续分享智能家居实用教程。'
WORK, DEST = media.WORK, media.DEST
VOICE = WORK / '关注片头.wav'
MARKS = WORK / '关注片头.json'
FINAL = media.FINAL
render = media.render

async def synthesize():
    sys.path.insert(0, str(render.PROJECT / 'build/video-python'))
    import edge_tts
    signature = hashlib.sha256((TEXT + 'Xiaoxiao-6').encode()).hexdigest()
    stamp = WORK / '关注片头.sha256'
    if VOICE.exists() and stamp.exists() and stamp.read_text() == signature:
        print('INTRO voice cached', flush=True)
        return
    marks = []
    communicate = edge_tts.Communicate(TEXT, 'zh-CN-XiaoxiaoNeural', rate='-6%', boundary='WordBoundary')
    with (WORK / '关注片头.mp3').open('wb') as audio:
        async for chunk in communicate.stream():
            if chunk['type'] == 'audio': audio.write(chunk['data'])
            elif chunk['type'] == 'WordBoundary': marks.append({k: chunk[k] for k in ('offset', 'duration', 'text')})
    media.run(['-i', WORK / '关注片头.mp3', '-ac', '1', '-ar', '48000', VOICE])
    MARKS.write_text(json.dumps(marks, ensure_ascii=False), encoding='utf-8')
    stamp.write_text(signature)
    print('INTRO neural voice ready', flush=True)
    await asyncio.sleep(.25)

def draw(music_credit=None):
    im = Image.new('RGB', (1920, 1080), '#eef3ee')
    d = ImageDraw.Draw(im)
    d.ellipse((-220, -450, 700, 470), fill='#e1ece2')
    d.ellipse((1490, 700, 2240, 1440), fill='#dae9df')
    d.rectangle((0, 0, 1920, 12), fill=render.GREEN)
    def center(y, text, size, color=render.INK, bold=False):
        d.text((960, y), text, font=render.font(size, bold), fill=color, anchor='mm')
    center(158, 'ESP32-S3  /  智能家居实用教程', 30, render.MUTED, True)
    center(310, '教程开始前', 54, render.GREEN, True)
    center(435, '帮忙点个关注', 104, render.INK, True)
    center(555, '接线 · 刷机 · 配网 · 设置，一步步讲清楚', 36, render.MUTED)
    center(816, '美的空调接入米家与 HomeKit', 38, render.GREEN, True)
    if music_credit:
        center(879, music_credit, 21, render.MUTED)
        center(912, 'CC BY 4.0  ·  https://creativecommons.org/licenses/by/4.0/', 20, render.MUTED)
    d.rectangle((0, 941, 1920, 1080), fill='#112e27')
    im.save(WORK / '关注片头-底图.png')
    button = Image.new('RGBA', (600, 116), (0, 0, 0, 0))
    bd = ImageDraw.Draw(button)
    bd.rounded_rectangle((0, 0, 599, 115), radius=58, fill=render.GREEN)
    bd.text((300, 56), '＋ 关注，继续一起折腾', font=render.font(36, True), fill='white', anchor='mm')
    button.save(WORK / '关注片头-按钮.png')
    # Preserve exactly the same subtitle style as the rest of the tutorial.
    header = (WORK / 'scene-00.ass').read_text(encoding='utf-8-sig').split('Dialogue:')[0]
    marks = json.loads(MARKS.read_text(encoding='utf-8'))
    split = TEXT.index('后续')
    first_words = []; count = 0
    for mark in marks:
        if count >= split: break
        first_words.append(mark); count += len(mark['text'])
    # Punctuation is absent in word metadata; find the second sentence by its first word.
    second = next(m for m in marks if m['text'].startswith('后续'))
    start = marks[0]['offset'] / 1e7 + .35
    split_time = second['offset'] / 1e7 + .35
    end = (marks[-1]['offset'] + marks[-1]['duration']) / 1e7 + .47
    caps = [(start, split_time - .12, TEXT[:split]), (split_time, end, TEXT[split:])]
    for a, b, text in caps:
        header += f'Dialogue: 0,{render.timestamp(a, True)},{render.timestamp(b, True)},Default,,0,0,0,,{text}\n'
    (WORK / 'follow-intro.ass').write_text(header, encoding='utf-8-sig')
    return caps

def build():
    caps = draw()
    with wave.open(str(VOICE), 'rb') as f: voice_seconds = f.getnframes() / f.getframerate()
    seconds = math.ceil((voice_seconds + 1.45) * 25) / 25
    intro = WORK / '关注片头.mp4'
    graph = ('[0:v][1:v]overlay=x=(W-w)/2:y=640-8*sin(min(t\\,1.2)/1.2*PI),'
             f'ass=follow-intro.ass,fade=t=in:st=0:d=0.3,fade=t=out:st={seconds-.3}:d=0.3[v];'
             '[2:a]adelay=350|350,apad,aformat=channel_layouts=stereo,'
             'loudnorm=I=-16:TP=-1.5:LRA=9,aformat=sample_fmts=fltp:sample_rates=48000:channel_layouts=stereo,'
             'asplit=2[voice][detector];'
             '[3:a]aformat=sample_fmts=fltp:sample_rates=48000:channel_layouts=stereo,volume=1.15[bed];'
             '[bed][detector]sidechaincompress=threshold=0.04:ratio=3:attack=70:release=850:makeup=1[ducked];'
             '[voice][ducked]amix=inputs=2:duration=longest:dropout_transition=0,volume=2,'
             f'alimiter=limit=0.891:level=0,afade=t=in:st=0:d=0.4,afade=t=out:st={seconds-.4}:d=0.4[a]')
    # Limit the audio input before loudness analysis; apad must also have a finite duration.
    graph = graph.replace('adelay=350|350,apad,', f'adelay=350|350,apad,atrim=duration={seconds},')
    media.run(['-loop', '1', '-framerate', '25', '-i', WORK / '关注片头-底图.png',
               '-loop', '1', '-framerate', '25', '-i', WORK / '关注片头-按钮.png',
               '-i', VOICE, '-t', seconds, '-i', WORK / '原创轻音乐.wav', '-filter_complex', graph,
               '-map', '[v]', '-map', '[a]', '-t', seconds, '-c:v', 'libx264', '-preset', 'veryfast',
               '-crf', '21', '-threads', '4', '-pix_fmt', 'yuv420p', '-r', '25', '-c:a', 'aac',
               '-b:a', '192k', '-ac', '2', '-ar', '48000', intro])
    timeline = json.loads((WORK / 'timeline.json').read_text(encoding='utf-8'))
    chapter_starts = [('片头 · 帮忙点个关注', 0)]
    seen = set()
    for item in timeline:
        if item['chapter'] not in seen:
            chapter_starts.append((item['chapter'], item['start'] + seconds))
            seen.add(item['chapter'])
    total = seconds + timeline[-1]['start'] + timeline[-1]['duration'] + 1.1
    meta = ';FFMETADATA1\ntitle=美的空调接入米家与HomeKit：详细操作教程\ncomment=晓晓自然女声、原创轻音乐、关注片头\n'
    for j, (name, start) in enumerate(chapter_starts):
        end = chapter_starts[j + 1][1] if j + 1 < len(chapter_starts) else total
        meta += f'[CHAPTER]\nTIMEBASE=1/1000\nSTART={round(start*1000)}\nEND={round(end*1000)}\ntitle={name}\n'
    (WORK / '含片头章节.ffmetadata').write_text(meta, encoding='utf-8')
    # The immutable body prevents accidental double-prepending on a repeated run.
    body = WORK / '完整教程-自然配音配乐-无片头.mp4'
    last_output = WORK / '关注版输出.sha256'
    current_digest = hashlib.sha256(FINAL.read_bytes()).hexdigest()
    if not body.exists() or (last_output.exists() and last_output.read_text() != current_digest):
        import shutil
        shutil.copy2(FINAL, body)
    (WORK / '含片头合并.txt').write_text("file '关注片头.mp4'\nduration %.3f\nfile '完整教程-自然配音配乐-无片头.mp4'\n" % seconds, encoding='utf-8')
    merged = WORK / '自然配音配乐-含关注片头.mp4'
    media.run(['-f', 'concat', '-safe', '0', '-i', WORK / '含片头合并.txt',
               '-f', 'ffmetadata', '-i', WORK / '含片头章节.ffmetadata', '-map', '0:v', '-map', '0:a',
               '-map_metadata', '1', '-map_chapters', '1', '-c:v', 'copy', '-c:a', 'aac', '-b:a', '192k',
               '-ac', '2', '-ar', '48000', '-movflags', '+faststart', merged])
    import shutil
    shutil.copy2(merged, FINAL)
    # Rebuild independent subtitles and chapter text from the body timings.
    allcaps = list(caps)
    for i, item in enumerate(timeline):
        allcaps += [(seconds + item['start'] + a, seconds + item['start'] + b, text)
                    for a, b, text in media.captions(i, item['voice'])]
    srt = '\n'.join(f'{j+1}\n{render.timestamp(a)} --> {render.timestamp(b)}\n{text}\n'
                    for j, (a, b, text) in enumerate(allcaps))
    (DEST / '详细教程-中文字幕.srt').write_text(srt, encoding='utf-8-sig')
    (DEST / '章节定位.txt').write_text('\n'.join(f'{int(t)//60:02d}:{int(t)%60:02d} {name}'
                                            for name, t in chapter_starts), encoding='utf-8')
    md = ['# 自然配音与配乐版 · 旁白及章节', '', '开头包含关注片头；正文保留完整的 49 步教程。', '', '## 章节', '']
    md += [f'- {int(t)//60:02d}:{int(t)%60:02d} {name}' for name, t in chapter_starts]
    md += ['', '## 片头', '', TEXT]
    for scene, item in zip(render.SCENES, timeline):
        t = seconds + item['start']
        md += ['', f"## {int(t)//60:02d}:{int(t)%60:02d} {scene['title']}", '', scene['narration']]
    (DEST / '详细教程-旁白与章节.md').write_text('\n'.join(md), encoding='utf-8')
    media.run(['-i', FINAL, '-t', '60', '-map', '0:v', '-map', '0:a', '-map_chapters', '-1',
               '-c', 'copy', '-movflags', '+faststart', DEST / '前60秒试听.mp4'])
    media.run(['-ss', '2', '-i', intro, '-frames:v', '1', '-q:v', '2', DEST / '关注片头预览.jpg'])
    digest = hashlib.sha256(FINAL.read_bytes()).hexdigest()
    FINAL.with_suffix('.mp4.sha256').write_text(digest + '  ' + FINAL.name + '\n', encoding='utf-8')
    last_output.write_text(digest)
    note = DEST / '制作说明.md'
    text = note.read_text(encoding='utf-8')
    if '## 关注片头' not in text:
        note.write_text(text + f'\n## 关注片头\n\n按用户要求增加 {seconds:.2f} 秒开场画面，同一位晓晓女声邀请关注；独立字幕和章节已整体顺延。片头由 `add_follow_intro.py` 制作。\n', encoding='utf-8')
    (WORK / '片头时长.json').write_text(json.dumps({'intro_seconds': seconds, 'total_planned': total}), encoding='utf-8')
    print(f'INTRO COMPLETE {seconds:.2f}s; {FINAL}', flush=True)

if __name__ == '__main__':
    if '--synthesize' in sys.argv:
        asyncio.run(synthesize())
    else:
        build()
