# -*- coding: utf-8 -*-
"""Microsoft neural narration. Online text submission explicitly authorized by user."""
from pathlib import Path
import asyncio, hashlib, html, json, subprocess, sys
import imageio_ffmpeg
from storyboard import SCENES

HERE = Path(__file__).resolve().parent
PROJECT = HERE.parent.parent
WORK = PROJECT / 'build/video-tutorial-natural'
WORK.mkdir(parents=True, exist_ok=True)
sys.path.insert(0, str(PROJECT / 'build/video-python'))
import edge_tts

VOICE = 'zh-CN-XiaoxiaoNeural'
RATE = '-6%'
FF = imageio_ffmpeg.get_ffmpeg_exe()

async def synthesize(i, scene, semaphore):
    async with semaphore:
        signature = hashlib.sha256((scene['narration'] + VOICE + RATE + 'neural-v1').encode()).hexdigest()
        mp3, wav = WORK / f'voice-{i:02d}.mp3', WORK / f'voice-{i:02d}.wav'
        stamp = WORK / f'voice-{i:02d}.neural.sha256'
        if wav.exists() and stamp.exists() and stamp.read_text() == signature:
            print(f'VOICE {i + 1}/{len(SCENES)} cached', flush=True)
            return
        for attempt in range(3):
            try:
                marks = []
                communicate = edge_tts.Communicate(scene['narration'], VOICE, rate=RATE,
                    boundary='WordBoundary', connect_timeout=20, receive_timeout=90)
                with mp3.open('wb') as audio:
                    async for chunk in communicate.stream():
                        if chunk['type'] == 'audio':
                            audio.write(chunk['data'])
                        elif chunk['type'] == 'WordBoundary':
                            marks.append({k: html.unescape(chunk[k]) if k == 'text' else chunk[k]
                                          for k in ('offset', 'duration', 'text')})
                if not marks or mp3.stat().st_size < 1000:
                    raise RuntimeError('Missing audio or word timings')
                result = subprocess.run([FF, '-hide_banner', '-loglevel', 'error', '-y', '-i',
                    str(mp3), '-ac', '1', '-ar', '48000', str(wav)], capture_output=True)
                if result.returncode:
                    raise RuntimeError(result.stderr.decode('utf-8', 'replace'))
                (WORK / f'voice-{i:02d}.json').write_text(json.dumps(marks, ensure_ascii=False), encoding='utf-8')
                stamp.write_text(signature)
                print(f'VOICE {i + 1}/{len(SCENES)} neural, {len(marks)} word timings', flush=True)
                return
            except Exception as error:
                print(f'VOICE {i + 1} attempt {attempt + 1}: {type(error).__name__}: {error}', flush=True)
                if attempt == 2:
                    raise
                await asyncio.sleep(2 + attempt * 2)

async def main():
    semaphore = asyncio.Semaphore(2)
    await asyncio.gather(*(synthesize(i, s, semaphore) for i, s in enumerate(SCENES)))

if __name__ == '__main__':
    asyncio.run(main())
