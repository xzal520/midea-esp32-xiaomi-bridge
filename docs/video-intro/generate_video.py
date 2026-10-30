# -*- coding: utf-8 -*-
"""Generate a Chinese intro video for the ESP32-S3 bedroom bridge."""
from __future__ import annotations

import json
import math
import os
import subprocess
import tempfile
import wave
from pathlib import Path

import imageio_ffmpeg
import numpy as np
from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parent
WIDTH, HEIGHT, FPS = 1280, 720, 24
OUT_MP4 = ROOT / "ESP32-S3-主卧项目介绍.mp4"
OUT_SRT = ROOT / "ESP32-S3-主卧项目介绍.srt"
OUT_WAV = ROOT / "ESP32-S3-主卧项目介绍-背景音.wav"
OUT_POSTER = ROOT / "封面.png"
OUT_VO = ROOT / "旁白稿.md"
FONT_PATH = r"C:\Windows\Fonts\simhei.ttf"
FONT_BOLD_PATH = r"C:\Windows\Fonts\msyhbd.ttc"


SCENES = [
    {
        "title": "让熟悉的空调，接入熟悉的生态",
        "kicker": "ESP32-S3 · 主卧空调桥接项目",
        "points": ["美的空调", "米家控制", "HomeKit 与 BLE"],
        "narration": "这是 ESP32-S3 主卧空调桥接项目。它把美的空调接入米家，也让网页、苹果家庭和蓝牙控制可以协同工作。项目已经完成，并在主卧持续使用。",
        "caption": "让美的空调接入米家、HomeKit 与 BLE",
        "visual": "hero",
    },
    {
        "title": "一块 S3，连接两端协议",
        "kicker": "本地串口桥接 · 双向状态同步",
        "points": ["美的空调  ⇄  ESP32-S3  ⇄  米家模块", "9600 / 115200 · 双串口独立通信", "状态回读后再确认控制结果"],
        "narration": "核心是一块 ESP32-S3。它通过两路串口分别连接美的空调和米家模块，在本地完成协议转换。控制指令发出后，设备状态以空调实际回包为准，避免把请求误当成执行成功。",
        "caption": "双串口本地桥接，状态以美的实际回包为准",
        "visual": "bridge",
    },
    {
        "title": "常用控制，按真实回包同步",
        "kicker": "开关 · 模式 · 温度 · 风速 · 摆风",
        "points": ["米家控制和网页控制", "七档风速映射可自定义", "ECO、睡眠、屏显等状态同步"],
        "narration": "日常可以从米家或网页控制开关、模式、温度、风速和摆风，也能同步 ECO、睡眠与屏显。七档风速映射可以按这台空调的实际表现自定义，状态会跟随美的回包更新。",
        "caption": "开关、模式、温度、风速与常用状态同步",
        "visual": "controls",
    },
    {
        "title": "倒计时由美的空调执行",
        "kicker": "开机定时 · 关机定时 · 状态确认",
        "points": ["米家设置开机或关机倒计时", "映射到美的原生定时槽", "读取空调回包，不依赖网页缓存"],
        "narration": "开机和关机倒计时映射到美的空调自身的定时功能。ESP32 发送设置后，还会读取空调回包确认结果。它不是云端预约同步，也不依赖网页一直开着。",
        "caption": "倒计时使用美的原生定时，不依赖网页常开",
        "visual": "timer",
    },
    {
        "title": "网页、HomeKit、BLE 都在身边",
        "kicker": "本地网页管理 · Apple Home · Android BLE",
        "points": ["网页资源由 ESP32 本机提供", "支持 HomeKit 配对与设备控制", "手机蓝牙可读取状态并操作"],
        "narration": "网页直接由 ESP32 本机提供，不依赖 CDN。项目也支持 HomeKit 配对和设备控制，以及 Android 蓝牙连接。访问域名、路由器显示名称、风速映射等常用选项都能在网页设置。",
        "caption": "网页由设备本机提供，支持 HomeKit 与 Android BLE",
        "visual": "ecosystem",
    },
    {
        "title": "每一段通信，都能看得见",
        "kicker": "串口日志 · IO16 独立 RX · 运行指标",
        "points": ["美的与米家通信日志分开显示", "IO16 可切换独立接收调试", "RAM、PSRAM 与响应延迟可查看"],
        "narration": "遇到问题时，可以在网页查看美的和米家串口日志，还能把 IO16 切换为独立接收模式抓取原始数据。运行页面也会显示内存、响应延迟和设备通信情况，方便定位问题。",
        "caption": "串口日志、IO16 抓包和运行指标一目了然",
        "visual": "diagnostics",
    },
    {
        "title": "ESP32-S3 主卧项目",
        "kicker": "本地运行 · 实际使用 · 持续完善",
        "points": ["协议转换在家中本地完成", "状态确认来自空调真实回包", "为日常使用而打磨的个人项目"],
        "narration": "从协议转换到网页管理，再到 HomeKit 和调试工具，这个项目已经在主卧正常使用。它的目标很简单：让原有美的空调融入现有智能家居，同时保持本地可控、状态可查、问题可诊断。",
        "caption": "ESP32-S3 主卧项目 · 已完成并投入日常使用",
        "visual": "end",
    },
]


def font(size: int, bold: bool = False) -> ImageFont.FreeTypeFont:
    return ImageFont.truetype(FONT_BOLD_PATH if bold else FONT_PATH, size)


def draw_text(draw: ImageDraw.ImageDraw, xy, text, size, fill, bold=False, anchor=None):
    draw.text(xy, text, font=font(size, bold), fill=fill, anchor=anchor)


def make_background(index: int) -> Image.Image:
    rows = np.zeros((HEIGHT, WIDTH, 3), dtype=np.uint8)
    ys = np.arange(HEIGHT, dtype=np.float32) / HEIGHT
    rows[:, :, 0] = (8 + 5 * ys).astype(np.uint8)[:, None]
    rows[:, :, 1] = (18 + 12 * ys).astype(np.uint8)[:, None]
    rows[:, :, 2] = (34 + 18 * ys).astype(np.uint8)[:, None]
    base = Image.fromarray(rows, "RGB")
    d = ImageDraw.Draw(base, "RGBA")
    # Subtle technical grid and soft accent orbs.
    for x in range(0, WIDTH, 64):
        d.line((x, 0, x, HEIGHT), fill=(120, 160, 205, 12), width=1)
    for y in range(0, HEIGHT, 64):
        d.line((0, y, WIDTH, y), fill=(120, 160, 205, 12), width=1)
    d.ellipse((820, -160, 1450, 470), fill=(20, 124, 255, 22))
    d.ellipse((-250, 420, 420, 1060), fill=(45, 185, 182, 15))
    d.rounded_rectangle((48, 48, WIDTH - 48, HEIGHT - 48), 26,
                        outline=(130, 176, 220, 28), width=2)
    d.rounded_rectangle((82, 77, 91, 112), 4, fill=(58, 163, 255, 255))
    draw_text(d, (111, 77), "ESP32-S3  /  MIDEA BRIDGE", 19, (165, 190, 216, 255), bold=True)
    draw_text(d, (WIDTH - 100, 82), f"{index + 1:02d} / {len(SCENES):02d}", 17,
              (142, 164, 188, 255), anchor="ra")
    return base


def pill(d, box, label, color=(33, 101, 178, 170)):
    d.rounded_rectangle(box, radius=17, fill=color, outline=(114, 183, 255, 100), width=1)
    draw_text(d, ((box[0] + box[2]) / 2, (box[1] + box[3]) / 2), label, 19,
              (225, 239, 255, 255), bold=True, anchor="mm")


def scene_image(index: int, scene: dict) -> Image.Image:
    im = make_background(index)
    d = ImageDraw.Draw(im, "RGBA")
    draw_text(d, (96, 156), scene["kicker"], 22, (70, 185, 255, 255), bold=True)
    title_size = 46 if len(scene["title"]) < 18 else 40
    draw_text(d, (94, 205), scene["title"], title_size, (246, 249, 253, 255), bold=True)

    visual = scene["visual"]
    if visual == "hero":
        pill(d, (118, 355, 340, 414), "美的空调", (31, 87, 145, 190))
        pill(d, (510, 355, 765, 414), "ESP32-S3", (17, 122, 170, 190))
        pill(d, (935, 355, 1155, 414), "米家 · HomeKit", (92, 67, 161, 190))
        d.line((345, 384, 485, 384), fill=(88, 184, 255, 240), width=4)
        d.polygon([(485, 384), (468, 375), (468, 393)], fill=(88, 184, 255, 240))
        d.line((770, 384, 910, 384), fill=(88, 184, 255, 240), width=4)
        d.polygon([(910, 384), (893, 375), (893, 393)], fill=(88, 184, 255, 240))
        draw_text(d, (WIDTH // 2, 500), "让成熟的设备，融入自己的智能家居", 24,
                  (190, 211, 233, 255), anchor="mm")
    elif visual == "bridge":
        boxes = [(102, 366, 380, 474), (490, 338, 790, 502), (900, 366, 1178, 474)]
        labels = ["美的空调\nUART · 9600 8N1", "ESP32-S3\n协议解析与状态同步", "米家模块\nUART · 115200 8N1"]
        for b, label in zip(boxes, labels):
            d.rounded_rectangle(b, 20, fill=(17, 38, 61, 235), outline=(70, 156, 230, 150), width=2)
            for i, line in enumerate(label.split("\n")):
                draw_text(d, ((b[0] + b[2]) // 2, b[1] + 34 + i * 39), line,
                          23 if i == 0 else 17, (237, 244, 252, 255) if i == 0 else (159, 190, 218, 255),
                          bold=(i == 0), anchor="mm")
        for x1, x2 in [(385, 480), (800, 895)]:
            d.line((x1, 420, x2, 420), fill=(62, 183, 255, 240), width=4)
            d.polygon([(x2, 420), (x2 - 15, 411), (x2 - 15, 429)], fill=(62, 183, 255, 240))
        draw_text(d, (WIDTH // 2, 560), "设置发送后，等待设备回报再同步结果", 21,
                  (177, 202, 227, 255), anchor="mm")
    elif visual == "controls":
        for idx, (label, value) in enumerate([("运行模式", "制冷"), ("设定温度", "26°"), ("风速档位", "01 02 03 04 05 06 07")]):
            x = 108 + idx * 370
            d.rounded_rectangle((x, 350, x + 326, 472), 18, fill=(16, 39, 64, 235), outline=(76, 150, 215, 125), width=2)
            draw_text(d, (x + 24, 373), label, 17, (151, 180, 209, 255))
            draw_text(d, (x + 24, 410), value, 27 if idx < 2 else 20, (241, 247, 253, 255), bold=True)
        d.rounded_rectangle((108, 510, 1172, 568), 16, fill=(18, 53, 80, 155))
        draw_text(d, (WIDTH // 2, 539), "状态显示来自设备回包   ·   风速档位可按设备自定义", 20,
                  (185, 218, 242, 255), anchor="mm")
    elif visual == "timer":
        d.rounded_rectangle((172, 350, 1108, 514), 24, fill=(14, 37, 62, 240), outline=(67, 153, 225, 150), width=2)
        draw_text(d, (238, 395), "关机倒计时", 24, (237, 244, 252, 255), bold=True)
        draw_text(d, (590, 434), "01 : 30", 52, (92, 196, 255, 255), bold=True, anchor="mm")
        draw_text(d, (980, 396), "美的原生定时", 21, (185, 208, 231, 255), anchor="mm")
        draw_text(d, (WIDTH // 2, 548), "不靠网页驻留执行；读回空调状态确认", 20,
                  (176, 199, 223, 255), anchor="mm")
    elif visual == "ecosystem":
        cards = [(102, "本地网页", "设置 · 状态 · 日志"), (495, "Apple Home", "HomeKit 配对控制"), (888, "手机 BLE", "Android 状态与控制")]
        for x, title, sub in cards:
            d.rounded_rectangle((x, 360, x + 280, 496), 21, fill=(17, 41, 67, 238), outline=(86, 154, 215, 142), width=2)
            d.ellipse((x + 110, 383, x + 170, 443), fill=(36, 126, 190, 160), outline=(111, 205, 255, 205), width=2)
            draw_text(d, (x + 140, 413), "↗", 30, (233, 248, 255, 255), bold=True, anchor="mm")
            draw_text(d, (x + 140, 461), title, 22, (243, 247, 252, 255), bold=True, anchor="mm")
            draw_text(d, (x + 140, 532), sub, 17, (170, 198, 224, 255), anchor="mm")
        draw_text(d, (WIDTH // 2, 555), "控制与配置在家庭局域网内完成", 20, (160, 207, 238, 255), anchor="mm")
    elif visual == "diagnostics":
        d.rounded_rectangle((100, 342, 1180, 570), 20, fill=(10, 22, 36, 245), outline=(60, 111, 158, 145), width=2)
        rows = [
            ("00:18:24.106", "米家 → ESP32", "set_properties · mode=制冷 · temp=26.0"),
            ("00:18:24.160", "ESP32 → 美的", "UART2 · 控制帧已发送"),
            ("00:18:24.286", "美的 → ESP32", "C0 · 校验通过 · 状态已更新"),
            ("00:18:24.302", "ESP32 → 米家", "result · 成功")]
        for i, (stamp, direction, msg) in enumerate(rows):
            y = 371 + 44 * i
            d.line((126, y + 29, 1153, y + 29), fill=(95, 127, 156, 44), width=1)
            draw_text(d, (132, y), stamp, 15, (145, 171, 194, 255))
            draw_text(d, (318, y), direction, 16, (111, 196, 255, 255), bold=True)
            draw_text(d, (520, y), msg, 16, (204, 220, 236, 255))
        draw_text(d, (WIDTH // 2, 548), "原始字节、解析结果与运行指标，协助快速定位问题", 19,
                  (170, 200, 226, 255), anchor="mm")
    elif visual == "end":
        pill(d, (415, 358, 865, 428), "ESP32-S3 · 主卧", (23, 93, 152, 220))
        draw_text(d, (WIDTH // 2, 498), "本地桥接  ·  真实回包  ·  日常可用", 27,
                  (222, 237, 250, 255), bold=True, anchor="mm")
        draw_text(d, (WIDTH // 2, 564), "Midea → 米家   |   HomeKit   |   BLE", 20,
                  (128, 193, 236, 255), anchor="mm")

    d.rounded_rectangle((128, 575, 1152, 620), 13, fill=(5, 17, 30, 225), outline=(75, 131, 183, 80), width=1)
    draw_text(d, (WIDTH // 2, 598), scene["caption"], 23, (237, 244, 251, 255), bold=True, anchor="mm")

    d.rounded_rectangle((100, 633, 1180, 670), 12, fill=(14, 34, 54, 200))
    draw_text(d, (WIDTH // 2, 651), "   ·   ".join(scene["points"][:3]), 15,
              (158, 190, 220, 255), anchor="mm")
    return im


def make_audio() -> list[float]:
    durations = [9.0] * len(SCENES)
    sample_rate = 44100
    total = int(sum(durations) * sample_rate)
    t = np.arange(total, dtype=np.float32) / sample_rate
    # Quiet, synthesized ambient bed with slow chord changes and a gentle fade.
    audio = np.zeros(total, dtype=np.float32)
    chords = [(110.0, 164.81, 220.0), (98.0, 146.83, 196.0),
              (130.81, 164.81, 196.0), (110.0, 146.83, 174.61),
              (123.47, 164.81, 207.65), (98.0, 130.81, 164.81),
              (110.0, 164.81, 220.0)]
    cursor = 0
    for duration, chord in zip(durations, chords):
        n = int(duration * sample_rate)
        local = np.arange(n, dtype=np.float32) / sample_rate
        envelope = np.minimum(local / 1.4, 1.0) * np.minimum((duration - local) / 1.8, 1.0)
        pad = sum(np.sin(2 * np.pi * hz * local + phase) for hz, phase in zip(chord, (0.0, 0.7, 1.3))) / 3
        audio[cursor:cursor + n] = pad * np.maximum(envelope, 0.0) * 0.075
        cursor += n
    # Soft overall fade at the head and tail.
    fade = int(1.5 * sample_rate)
    audio[:fade] *= np.linspace(0, 1, fade, dtype=np.float32)
    audio[-fade:] *= np.linspace(1, 0, fade, dtype=np.float32)
    stereo = np.column_stack((audio, audio))
    pcm = np.clip(stereo * 32767, -32768, 32767).astype("<i2")
    with wave.open(str(OUT_WAV), "wb") as out:
        out.setnchannels(2)
        out.setsampwidth(2)
        out.setframerate(sample_rate)
        out.writeframes(pcm.tobytes())
    lines = ["# 视频旁白稿", "", "以下旁白与视频场景顺序一致；可直接录音后替换背景音轨。", ""]
    for i, scene in enumerate(SCENES, 1):
        lines.extend([f"## {i}. {scene['title']}", "", scene["narration"], ""])
    OUT_VO.write_text("\n".join(lines), encoding="utf-8-sig")
    return durations


def write_srt(durations: list[float]) -> None:
    def stamp(seconds: float) -> str:
        ms = int(round(seconds * 1000))
        hh, ms = divmod(ms, 3_600_000)
        mm, ms = divmod(ms, 60_000)
        ss, ms = divmod(ms, 1000)
        return f"{hh:02d}:{mm:02d}:{ss:02d},{ms:03d}"
    cursor = 0.0
    blocks = []
    for i, (scene, duration) in enumerate(zip(SCENES, durations), 1):
        blocks.append(f"{i}\n{stamp(cursor)} --> {stamp(cursor + duration)}\n{scene['caption']}\n")
        cursor += duration
    OUT_SRT.write_text("\n".join(blocks), encoding="utf-8-sig")


def render_video(durations: list[float]) -> None:
    ffmpeg = imageio_ffmpeg.get_ffmpeg_exe()
    images = [scene_image(i, scene) for i, scene in enumerate(SCENES)]
    images[0].save(OUT_POSTER)
    writer = imageio_ffmpeg.write_frames(
        str(OUT_MP4), (WIDTH, HEIGHT), fps=FPS, codec="libx264",
        quality=8, pix_fmt_out="yuv420p",
        output_params=["-preset", "medium", "-movflags", "+faststart", "-an"])
    writer.send(None)
    try:
        for im, duration in zip(images, durations):
            frame = np.asarray(im, dtype=np.uint8)
            count = max(1, int(math.ceil(duration * FPS)))
            for f in range(count):
                # Gentle drift in the lower accent bar makes the slide show feel alive.
                writer.send(np.ascontiguousarray(frame))
    finally:
        writer.close()
    mux = ROOT / "video-with-audio.mp4"
    subprocess.run([ffmpeg, "-y", "-i", str(OUT_MP4), "-i", str(OUT_WAV),
                    "-map", "0:v:0", "-map", "1:a:0", "-c:v", "copy",
                    "-c:a", "aac", "-b:a", "128k", "-shortest", "-movflags", "+faststart",
                    str(mux)], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    mux.replace(OUT_MP4)


def main() -> None:
    durations = make_audio()
    write_srt(durations)
    render_video(durations)
    with wave.open(str(OUT_WAV), "rb") as wf:
        seconds = wf.getnframes() / wf.getframerate()
    print(f"已生成：{OUT_MP4}")
    print(f"时长：{seconds:.1f} 秒；分辨率：{WIDTH}x{HEIGHT}；画面中文字幕与合成背景音")
    print(f"字幕：{OUT_SRT}")
    print(f"封面：{OUT_POSTER}")
    print(f"旁白稿：{OUT_VO}")


if __name__ == "__main__":
    main()
