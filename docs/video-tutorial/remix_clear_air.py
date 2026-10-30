# -*- coding: utf-8 -*-
"""Repair intro channel timing; replace the complete score from clean narration."""
from pathlib import Path
import hashlib, json, math, shutil, subprocess, wave
import numpy as np
import make_natural_audio as media
import add_follow_intro as intro

WORK, DEST, run = media.WORK, media.DEST, media.run
SOURCE = WORK / 'Clear Air-Kevin MacLeod.mp3'
MASTER = WORK / '无配乐母版.mp4'
OUT = DEST / '美的空调接入米家与HomeKit-详细教程-配音配乐修正版-1080P.mp4'
SOURCE_URL = 'https://incompetech.com/music/royalty-free/index.html?isrc=USUAN1100626'
LICENSE_URL = 'https://creativecommons.org/licenses/by/4.0/'
INTRO_SECONDS = 8.8
TOTAL = 1260.14

def loop_music():
    short = WORK / 'clear-air-quiet.wav'
    if not short.exists():
        run(['-i', SOURCE, '-af', 'aformat=channel_layouts=stereo,loudnorm=I=-29:TP=-9:LRA=6,'
             'aformat=sample_rates=48000:channel_layouts=stereo', '-ac', '2', '-ar', '48000', short])
    with wave.open(str(short), 'rb') as source:
        sr = source.getframerate()
        audio = np.frombuffer(source.readframes(source.getnframes()), dtype='<i2').reshape(-1, 2).astype(np.float32) / 32768
    cross = round(6 * sr)
    phase = np.linspace(0, np.pi / 2, cross, dtype=np.float32)[:, None]
    transition = audio[-cross:] * np.cos(phase) + audio[:cross] * np.sin(phase)
    path = WORK / 'clear-air-full-bed.wav'
    remaining, cursor = round(TOTAL * sr), 0
    with wave.open(str(path), 'wb') as sink:
        sink.setparams((2, 2, sr, 0, 'NONE', 'not compressed'))
        def emit(part):
            nonlocal remaining, cursor
            # Work in small chunks to keep memory use bounded.
            for start in range(0, len(part), sr * 4):
                n = min(sr * 4, len(part) - start, remaining)
                if n <= 0: return
                block = part[start:start+n].copy()
                seconds = (np.arange(n, dtype=np.float32) + cursor) / sr
                gain = np.minimum(1, seconds / 2.5) * np.clip((TOTAL - seconds) / 4, 0, 1)
                block *= gain[:, None]
                sink.writeframes((np.clip(block, -1, 1) * 32767).astype('<i2').tobytes())
                remaining -= n
                cursor += n
        emit(audio[:-cross])
        while remaining:
            emit(transition)
            emit(audio[cross:-cross])
    return path

def make_picture():
    intro.draw(music_credit='Music: “Clear Air” — Kevin MacLeod (incompetech.com)')
    picture_intro = WORK / 'follow-intro-repaired-picture.mp4'
    graph = ('[0:v][1:v]overlay=x=(W-w)/2:y=640-8*sin(min(t\\,1.2)/1.2*PI),'
             'ass=follow-intro.ass,fade=t=in:st=0:d=0.3,fade=t=out:st=8.5:d=0.3[v]')
    run(['-loop', '1', '-framerate', '25', '-i', WORK / '关注片头-底图.png',
         '-loop', '1', '-framerate', '25', '-i', WORK / '关注片头-按钮.png',
         '-filter_complex', graph, '-map', '[v]', '-an', '-t', INTRO_SECONDS,
         '-c:v', 'libx264', '-preset', 'veryfast', '-crf', '21', '-threads', '4',
         '-pix_fmt', 'yuv420p', '-r', '25', picture_intro])
    (WORK / 'clear-air-picture-list.txt').write_text(
        "file 'follow-intro-repaired-picture.mp4'\nduration 8.800\nfile '无配乐母版.mp4'\n", encoding='utf-8')
    picture = WORK / 'clear-air-picture.mp4'
    run(['-f', 'concat', '-safe', '0', '-i', WORK / 'clear-air-picture-list.txt',
         '-map', '0:v:0', '-c:v', 'copy', '-an', picture])
    return picture

def make_voice():
    clean_intro = WORK / 'follow-intro-voice-synchronized.wav'
    # Specify BOTH channels: adelay=350 alone delays only the first negotiated channel.
    run(['-i', intro.VOICE, '-af',
         'aformat=channel_layouts=stereo,adelay=350|350,apad,atrim=duration=8.8,'
         'loudnorm=I=-16:TP=-1.5:LRA=9,aformat=sample_rates=48000:channel_layouts=stereo',
         '-ac', '2', '-ar', '48000', clean_intro])
    voice = WORK / 'clear-air-clean-narration.wav'
    run(['-i', clean_intro, '-i', MASTER, '-filter_complex',
         '[0:a]asetpts=PTS-STARTPTS[a];[1:a]asetpts=PTS-STARTPTS[b];[a][b]concat=n=2:v=0:a=1[v]',
         '-map', '[v]', '-t', TOTAL, '-ac', '2', '-ar', '48000', voice])
    return voice

def build():
    print('Preparing synchronized original narration', flush=True)
    voice = make_voice()
    print('Preparing acoustic guitar / piano music, with smooth loop joins', flush=True)
    music = loop_music()
    print('Updating intro picture and music credit', flush=True)
    picture = make_picture()
    meta = (WORK / '含片头章节.ffmetadata').read_text(encoding='utf-8')
    meta = meta.replace('comment=晓晓自然女声、原创轻音乐、关注片头',
        'comment=晓晓女声；Clear Air by Kevin MacLeod (incompetech.com), CC BY 4.0; channel timing repaired')
    (WORK / 'clear-air-chapters.ffmetadata').write_text(meta, encoding='utf-8')
    graph = ('[1:a]aformat=sample_fmts=fltp:sample_rates=48000:channel_layouts=stereo,asplit=2[voice][detector];'
             '[2:a]aformat=sample_fmts=fltp:sample_rates=48000:channel_layouts=stereo[bed];'
             '[bed][detector]sidechaincompress=threshold=0.04:ratio=3:attack=70:release=850:makeup=1[ducked];'
             '[voice][ducked]amix=inputs=2:duration=first:dropout_transition=0,volume=2,'
             'alimiter=limit=0.87:level=0,aformat=sample_rates=48000:channel_layouts=stereo[mix]')
    print('Mixing one synchronized narration with the replacement music', flush=True)
    run(['-i', picture, '-i', voice, '-i', music, '-f', 'ffmetadata', '-i', WORK / 'clear-air-chapters.ffmetadata',
         '-filter_complex', graph, '-map', '0:v:0', '-map', '[mix]', '-map_metadata', '3', '-map_chapters', '3',
         '-t', TOTAL, '-c:v', 'copy', '-c:a', 'aac', '-b:a', '192k', '-ac', '2', '-ar', '48000',
         '-movflags', '+faststart', OUT])
    preview = DEST / '前60秒预览-修正版.mp4'
    run(['-i', OUT, '-t', '60', '-map', '0:v:0', '-map', '0:a:0', '-map_chapters', '-1',
         '-c', 'copy', '-movflags', '+faststart', preview])
    # Replace previous public outputs as well, so their saved paths no longer contain the error.
    shutil.copy2(OUT, media.FINAL)
    shutil.copy2(preview, DEST / '前60秒试听.mp4')
    run(['-i', OUT, '-t', '30', '-vn', '-map', '0:a:0', '-c:a', 'libmp3lame', '-b:a', '192k',
         '-metadata', 'comment=Music: Clear Air by Kevin MacLeod (incompetech.com); CC BY 4.0; https://creativecommons.org/licenses/by/4.0/',
         DEST / '新版配音试听.mp3'])
    run(['-ss', '2', '-i', OUT, '-frames:v', '1', '-q:v', '2', DEST / '关注片头预览.jpg'])
    credit = ('# 背景音乐署名与来源\n\n'
              '“Clear Air” — Kevin MacLeod (incompetech.com)\n\n'
              f'来源：{SOURCE_URL}\n\nLicensed under Creative Commons: By Attribution 4.0\n{LICENSE_URL}\n\n'
              '视频中降低音量、循环、交叉淡化，并在旁白期间自动压低音乐；片头画面和 MP4 元数据均保留署名。\n'
              '下载文件：Clear Air.mp3；ISRC：USUAN1100626。中文旁白、教程画面和接线内容来自本项目。\n')
    (DEST / '背景音乐署名与来源.md').write_text(credit, encoding='utf-8')
    (WORK / 'clear-air-source.json').write_text(json.dumps(dict(source=SOURCE_URL, license=LICENSE_URL,
        downloaded_sha256=hashlib.sha256(SOURCE.read_bytes()).hexdigest()), ensure_ascii=False, indent=2), encoding='utf-8')
    note = DEST / '制作说明.md'
    text = note.read_text(encoding='utf-8')
    text = text.replace('- 配乐：本项目原创、72 BPM 的柔和电钢琴与氛围和弦，无人声。背景声基础响度约 -28 LUFS，讲解时自动压低。',
        '- 配乐：Kevin MacLeod 的《Clear Air》，木吉他和钢琴；CC BY 4.0，署名见片头及《背景音乐署名与来源.md》。基础响度约 -29 LUFS，讲解时自动压低。')
    text += ('\n## 重音修复与配乐替换\n\n片头原有 350 ms 的左右声道延迟差已修正。人声从无配乐母版重新提取，'
             '统一左右声道起点，并从头至尾替换原背景音乐。运行 `remix_clear_air.py` 可重建此修正版。\n')
    note.write_text(text, encoding='utf-8')
    for path in (OUT, media.FINAL):
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        path.with_suffix('.mp4.sha256').write_text(digest + '  ' + path.name + '\n', encoding='utf-8')
    (WORK / '关注版输出.sha256').write_text(hashlib.sha256(media.FINAL.read_bytes()).hexdigest())
    print('REPAIRED ' + str(OUT), flush=True)

if __name__ == '__main__':
    build()
