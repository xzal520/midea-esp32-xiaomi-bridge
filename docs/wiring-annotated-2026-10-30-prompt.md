# Wiring image generation record

Mode: built-in image_gen image editing.

Source: codex-clipboard-59ed640a-0d95-4c99-98ae-1371f8212c54.jpg

Output: wiring-annotated-2026-10-30.png

Pin assignments checked against project README and visible ESP32 solder pads. TX and RX use ESP32 perspective.

## Final prompt

Edit target: the original supplied wiring photograph. Preserve the photograph and all physical components, wiring, solder joints, wire colors, perspective and framing exactly. Add a practical Chinese wiring annotation overlay only. No invented electronics.
Do NOT point arrows at individual PCB pin holes: tiny adjacent pins are too close. Identify the TWO wire harnesses with one leader each, and show exact per-wire mappings in clear grouped cards. Leave the board and solder joints uncovered.

Place a large white card on the empty brown tabletop on the right with title "接线标注" and subtitle "TX / RX 均以 ESP32 为准". Include exactly these groups, colored small line swatches matching actual wire colors:
"电源（两组共用）"
"红线：5V → VIN"
"黑线：GND（共地）"
"美的侧串口"
"白线：TX · GPIO16 → 空调 RX"
"黄线：RX · GPIO17 ← 空调 TX"
"米家侧串口"
"白线：TX · GPIO8 → 米家模块 RX"
"黄线：RX · GPIO7 ← 米家模块 TX"
Use a dark outline around the white wire swatch so it is visible on the white card.
Bottom note in the same card: "5V 是供电；串口信号为 3.3V 逻辑".

On the photo, add a small blue "米家侧线束" tag with a leader ending on the FOUR-WIRE bundle exiting the white plug of the left tilted black board (red, black, white, yellow diagonally going down and right, in the area x=25% image width, y=65% image height). This tag refers to the FOUR wires collectively, not a single wire or individual pin.
Add a small orange "美的侧线束" tag with a leader ending on the FOUR parallel vertical wires (black white yellow red) immediately below the black USB cable and above the white plug on the bottom display PCB, in the area x=38% width, y=66% height. This tag also refers to the FOUR wires collectively.
Add unobtrusive board tags "米家模块" pointing to the left tilted black PCB, "ESP32-S3" pointing to central black ESP board, and "美的显示板" pointing to bottom display PCB.
No leaders to tiny GPIO pads, no circles or dots covering solder joints. Keep text readable and cards away from the hardware. The original photograph must stay intact.

