"""Execute actual firmware RX/mode functions with simulated UART/GPIO, plus web checks."""
import os
from pathlib import Path
import re
import shutil
import subprocess

PROJECT = Path(__file__).resolve().parents[2]
REPO = PROJECT.parent
COMPILER = os.environ.get('ZIG') or shutil.which('zig') or str(REPO / 'ESP32-DevKitC-V4/build/host-cpp/ziglang/zig.exe')
OUT = PROJECT / 'build/io16-rx-check'
OUT.mkdir(parents=True, exist_ok=True)
os.environ['ZIG_GLOBAL_CACHE_DIR'] = str(PROJECT / 'build/zig-global')
os.environ['ZIG_LOCAL_CACHE_DIR'] = str(PROJECT / 'build/zig-local')
sketch = (PROJECT / 'firmware/midea_s3_bridge.ino').read_text(encoding='utf-8')
names = ['beginIo16RxLogStorage', 'flushIo16RxFrame', 'serviceIo16Rx', 'mideaTxRouteReady',
         'restoreIo16TxRoute', 'setIo16RxMode', 'beginMideaTx', 'endMideaTx',
         'sendMideaFrame', 'serviceSerial']
functions = []
for name in names:
    match = re.search(r'^static [^\n]+\b' + name + r'\([^\n]*\)\s*\{.*?^\}', sketch, re.M | re.S)
    assert match, 'Missing actual firmware function ' + name
    functions.append(match.group())
(OUT / 'io16_rx_functions.inc').write_text('\n\n'.join(functions), encoding='utf-8')

def run(args):
    result = subprocess.run([str(arg) for arg in args], capture_output=True,
                            text=True, encoding='utf-8', errors='replace')
    if result.returncode:
        raise RuntimeError(result.stdout + result.stderr)
    if result.stdout.strip():
        print(result.stdout.strip())

exe = OUT / 'io16_rx_test.exe'
run([COMPILER, 'c++', '-std=c++17',
     '-Wall', '-Wextra', '-Werror', '-I' + str(PROJECT / 'tests/support'),
     '-I' + str(PROJECT / 'firmware'), '-I' + str(OUT),
     PROJECT / 'tests/io16_rx/io16_rx_test.cpp', '-o', exe])
run([exe])
web = (PROJECT / 'firmware/web_pages.h').read_text(encoding='utf-8')
script = re.search(r'static const char WEB_SCRIPT\[\] PROGMEM = R"JS\((.*?)\)JS";', web, re.S).group(1)
(OUT / 'web-script.js').write_text(script, encoding='utf-8')
run(['node', '--check', OUT / 'web-script.js'])
assert 'server.on("/api/io16/rx", HTTP_POST, handleIo16RxMode)' in sketch
assert 'server.on("/api/io16/log", HTTP_GET, handleIo16RxLog)' in sketch
for name in ['handleIo16RxMode', 'handleIo16RxLog']:
    body = re.search(r'^static void ' + name + r'\(\) \{.*?^\}', sketch, re.M | re.S).group()
    assert 'if(!requireLogin())return;' in body
run(['node', PROJECT / 'tests/io16_rx/web_test.js', OUT / 'web-script.js'])
print('Host checks do not verify electrical GPIO levels or actual UART waveforms.')
