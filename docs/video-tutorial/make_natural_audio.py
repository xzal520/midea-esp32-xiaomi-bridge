# -*- coding: utf-8 -*-
"""Neural voice edition with word-timed subtitles and an original, ducked score."""
from pathlib import Path
import hashlib, json, math, re, subprocess, sys, wave
import numpy as np
import make_tutorial as render

HERE = Path(__file__).resolve().parent
WORK = HERE.parent.parent / 'build/video-tutorial-natural'
DEST = HERE / '自然配音版'
DEST.mkdir(parents=True, exist_ok=True)
WORK.mkdir(parents=True, exist_ok=True)
FINAL = DEST / '美的空调接入米家与HomeKit-详细教程-自然配音配乐版-1080P.mp4'
render.WORK, render.HERE, render.OUT = WORK, DEST, WORK / '无配乐母版.mp4'
run = render.run
original_captions = render.captions

def captions(index, duration):
    """Keep the original punctuation while using neural word boundaries."""
    marks = json.loads((WORK / f'voice-{index:02d}.json').read_text(encoding='utf-8'))
    narration = render.SCENES[index]['narration']
    positions, cursor = [], 0
    for mark in marks:
        start = narration.find(mark['text'], cursor)
        if start < 0:
            start = cursor
        end = min(len(narration), start + len(mark['text']))
        positions.append((start, end))
        cursor = end
    result, text, start_time = [], '', None
    for j, mark in enumerate(marks):
        a, b = positions[j]
        next_start = positions[j + 1][0] if j + 1 < len(marks) else len(narration)
        part = narration[a:max(b, next_start)]
        if start_time is None:
            start_time = mark['offset'] / 1e7 + .35
        text += part
        end_time = (mark['offset'] + mark['duration']) / 1e7 + .35
        sentence_end = bool(re.search(r'[。！？；]$', part))
        clause_end = bool(re.search(r'[，：、]$', part))
        next_len = len(marks[j + 1]['text']) if j + 1 < len(marks) else 0
        if sentence_end or (clause_end and len(text) >= 12) or len(text) + next_len > 30 or j == len(marks) - 1:
            result.append((start_time, min(end_time + .12, duration + .35), text))
            text, start_time = '', None
    result = [(a, min(b, result[j + 1][0] - .01) if j + 1 < len(result) else b, t)
              for j, (a, b, t) in enumerate(result)]
    # Reuse the project's established subtitle style.
    original_captions(index, duration)
    path = WORK / f'scene-{index:02d}.ass'
    header = path.read_text(encoding='utf-8-sig').split('Dialogue:')[0]
    path.write_text(header + ''.join(
        f'Dialogue: 0,{render.timestamp(a, True)},{render.timestamp(b, True)},Default,,0,0,0,,{t}\n'
        for a, b, t in result), encoding='utf-8-sig')
    return result

def score(seconds):
    """Original 72 BPM ambient electric-piano score, streamed in short blocks."""
    path = WORK / '原创轻音乐.wav'
    stamp = WORK / '原创轻音乐.sha256'
    signature = hashlib.sha256(f'ambient-rhodes-v3-{seconds:.4f}'.encode()).hexdigest()
    if path.exists() and stamp.exists() and stamp.read_text() == signature:
        return path
    sr, beat = 48000, 60 / 72
    block_len = int(sr * beat * 8)
    chords = [(50, 57, 61, 66), (47, 54, 57, 62), (43, 50, 54, 59), (45, 52, 59, 62),
              (50, 54, 57, 64), (47, 54, 62, 66), (43, 54, 57, 62), (45, 52, 57, 61)]
    rng = np.random.RandomState(20261030)
    total = round(seconds * sr)
    def frequency(note): return 440 * 2 ** ((note - 69) / 12)
    with wave.open(str(path), 'wb') as output:
        output.setparams((2, 2, sr, 0, 'NONE', 'not compressed'))
        for block, offset in enumerate(range(0, total, block_len)):
            size = min(block_len, total - offset)
            t = np.arange(size, dtype=np.float32) / sr
            absolute = t + offset / sr
            stereo = np.zeros((size, 2), np.float32)
            chord = chords[block % len(chords)]
            fade = np.minimum(1, t / .7) * np.minimum(1, (size / sr - t) / .8)
            for n, midi in enumerate(chord):
                f = frequency(midi)
                phase = n * .7
                left = np.sin(2 * np.pi * f * absolute + phase)
                right = np.sin(2 * np.pi * (f * 1.0012) * absolute + phase)
                tremolo = .92 + .08 * np.sin(2 * np.pi * .11 * absolute + n)
                stereo[:, 0] += left * fade * tremolo * .020
                stereo[:, 1] += right * fade * tremolo * .020
            def note(at, midi, gain=.07, decay=1.45, pan=0):
                start = round(at * sr)
                count = min(size - start, round(sr * 4.2))
                if count <= 0: return
                tt = np.arange(count, dtype=np.float32) / sr
                f = frequency(midi)
                # Soft attack and decaying, slightly inharmonic electric-piano partials.
                env = (1 - np.exp(-tt * 100)) * np.exp(-tt * decay)
                sound = (np.sin(2 * np.pi * f * tt) + .18 * np.sin(2 * np.pi * f * 2.002 * tt) * np.exp(-tt * 1.6)
                         + .04 * np.sin(2 * np.pi * f * 3.998 * tt) * np.exp(-tt * 3)) * env * gain
                stereo[start:start + count, 0] += sound * math.sqrt((1 - pan) / 2)
                stereo[start:start + count, 1] += sound * math.sqrt((1 + pan) / 2)
                delay = int(sr * .13)
                echo = min(count, size - start - delay)
                if echo > 0:
                    stereo[start + delay:start + delay + echo, 0] += sound[:echo] * .12
                    stereo[start + delay:start + delay + echo, 1] += sound[:echo] * .18
            for j, midi in enumerate(chord[1:] + chord[1:2]):
                note(j * 2 * beat, midi + 12, .045 + rng.uniform(-.004, .004), 1.2, (-.3, .3)[j % 2])
            note(.02, chord[0] - 12, .05, .9)
            note(4 * beat + .02, chord[0] - 12, .035, 1.1)
            if block % 4 == 3:
                note(5 * beat, chord[2] + 24, .016, 1.7, .4)
                note(7 * beat, chord[1] + 24, .014, 1.7, -.4)
            # Avoid any chord-boundary click, and fade the whole score at both ends.
            ramp = np.minimum(1, t / .02) * np.minimum(1, (size / sr - t) / .03)
            ramp *= np.minimum(1, absolute / 3) * np.minimum(1, (seconds - absolute) / 4)
            stereo *= np.maximum(0, ramp[:, None])
            output.writeframes((np.clip(stereo, -1, 1) * 32767).astype('<i2').tobytes())
    stamp.write_text(signature)
    return path

def mix():
    timeline = json.loads((WORK / 'timeline.json').read_text(encoding='utf-8'))
    # Include the small AAC concat padding, so the bed reaches the very last frame.
    seconds = timeline[-1]['start'] + timeline[-1]['duration'] + 2
    raw_music = score(seconds)
    music = WORK / '原创轻音乐-标准响度.wav'
    print('MUSIC normalizing original electric-piano / ambient score', flush=True)
    run(['-i', raw_music, '-af',
         'aformat=channel_layouts=stereo,loudnorm=I=-28:TP=-8:LRA=7,aformat=sample_rates=48000:channel_layouts=stereo',
         '-ac', '2', '-ar', '48000', music])
    # Speech stays at its mastered level. Music automatically dips during narration.
    graph = ('[0:a]aformat=sample_fmts=fltp:sample_rates=48000:channel_layouts=stereo,asplit=2[voice][detector];'
             '[1:a]aformat=sample_fmts=fltp:sample_rates=48000:channel_layouts=stereo[bed];'
             '[bed][detector]sidechaincompress=threshold=0.04:ratio=3:attack=70:release=850:makeup=1[ducked];'
             '[voice][ducked]amix=inputs=2:duration=first:dropout_transition=0,volume=2,'
             'alimiter=limit=0.891:level=0,aformat=sample_rates=48000:channel_layouts=stereo[mix]')
    print('MIX narration + automatically ducked music', flush=True)
    run(['-i', render.OUT, '-i', music, '-filter_complex', graph, '-map', '0:v', '-map', '[mix]',
         '-map_metadata', '0', '-map_chapters', '0', '-c:v', 'copy', '-c:a', 'aac', '-b:a', '192k',
         '-ac', '2', '-ar', '48000', '-movflags', '+faststart', FINAL])
    run(['-i', music, '-c:a', 'libmp3lame', '-b:a', '160k', DEST / '原创轻音乐.mp3'])
    run(['-i', FINAL, '-t', '60', '-map', '0:v', '-map', '0:a', '-map_chapters', '-1',
         '-c', 'copy', '-movflags', '+faststart', DEST / '前60秒试听.mp4'])
    script = DEST / '详细教程-旁白与章节.md'
    script.write_text(script.read_text(encoding='utf-8').replace('本机 Microsoft Huihui',
        'Microsoft 晓晓神经网络女声，语速 -6%；原创电钢琴与氛围配乐，旁白期间自动降低配乐音量'), encoding='utf-8')
    (DEST / '制作说明.md').write_text(
        '# 自然配音与配乐版\n\n'
        '- 旁白：Microsoft zh-CN-XiaoxiaoNeural（晓晓），语速 -6%，不变调。经用户授权向微软在线语音服务提交教程讲解稿。\n'
        '- 字幕：根据新语音的逐词时间戳重新对齐，保留原讲解稿标点。\n'
        '- 配乐：本项目原创、72 BPM 的柔和电钢琴与氛围和弦，无人声。背景声基础响度约 -28 LUFS，讲解时自动压低。\n'
        '- 画面：保留 49 步讲解、7 章、实物接线图及《固件刷写.mp4》录屏；根据新旁白长度重新安排停留时间。\n'
        '- 输出：1920×1080 / 25 FPS，H.264 / AAC 立体声，附 60 秒试听、独立字幕和章节时间轴。\n\n'
        '重新制作：先运行 `synthesize_natural.py`（需要网络），再运行 `make_natural_audio.py`。旧版本位于上一级目录。\n',
        encoding='utf-8')
    digest = hashlib.sha256(FINAL.read_bytes()).hexdigest()
    FINAL.with_suffix('.mp4.sha256').write_text(digest + '  ' + FINAL.name + '\n', encoding='utf-8')
    print('COMPLETE ' + str(FINAL), flush=True)

if __name__ == '__main__':
    render.captions = captions
    if '--mix-only' not in sys.argv:
        render.build()
    mix()
