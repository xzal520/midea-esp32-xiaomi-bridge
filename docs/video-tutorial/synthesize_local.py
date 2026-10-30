# -*- coding: utf-8 -*-
"""Local Windows SAPI narration; no network requests."""
from pathlib import Path
import hashlib,json,time,wave
import pythoncom
import win32com.client
from storyboard import SCENES

HERE=Path(__file__).resolve().parent
WORK=HERE.parent.parent/'build/video-tutorial'
class VoiceEvents:
    marks=[]
    text=''
    def OnWord(self,StreamNumber,StreamPosition,CharacterPosition,Length):
        VoiceEvents.marks.append((int(StreamPosition),VoiceEvents.text[CharacterPosition:CharacterPosition+Length]))

def main():
    voice=win32com.client.DispatchWithEvents('SAPI.SpVoice',VoiceEvents)
    installed=list(voice.GetVoices())
    voice.Voice=next(v for v in installed if 'Huihui' in v.GetDescription())
    voice.EventInterests=32
    voice.Rate=0;voice.Volume=100
    for i,s in enumerate(SCENES):
        signature=hashlib.sha256((s['narration']+'huihui-rate0-v3').encode()).hexdigest()
        path=WORK/f'voice-{i:02d}.wav';sf=WORK/f'voice-{i:02d}.local.sha256'
        if path.exists() and sf.exists() and sf.read_text()==signature:
            print(f'VOICE {i+1}/{len(SCENES)} cached',flush=True);continue
        stream=win32com.client.Dispatch('SAPI.SpFileStream')
        stream.Format.Type=22
        stream.Open(str(path),3)
        VoiceEvents.text=s['narration'];VoiceEvents.marks=[]
        voice.AudioOutputStream=stream
        voice.Speak(s['narration'],1)
        while not voice.WaitUntilDone(40):pythoncom.PumpWaitingMessages()
        pythoncom.PumpWaitingMessages();stream.Close()
        with wave.open(str(path),'rb') as f:byte_rate=f.getframerate()*f.getsampwidth()*f.getnchannels()
        raw=[(pos/byte_rate,text) for pos,text in VoiceEvents.marks if text]
        marks=[dict(offset=round(a*1e7),duration=round(max(.02,(raw[j+1][0] if j+1<len(raw) else a+.25)-a)*1e7),text=t) for j,(a,t) in enumerate(raw)]
        (WORK/f'voice-{i:02d}.json').write_text(json.dumps(marks,ensure_ascii=False),encoding='utf-8');sf.write_text(signature)
        print(f'VOICE {i+1}/{len(SCENES)} rendered, {len(marks)} marks',flush=True)

if __name__=='__main__':main()
