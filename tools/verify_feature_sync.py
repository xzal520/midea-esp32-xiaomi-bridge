"""Run shared behavior tests against S3 headers; no device/network access."""
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
CLASSIC = ROOT / 'ESP32-DevKitC-V4/firmware'
S3 = ROOT / 'ESP32-S3-MINI/firmware'
TESTS = ROOT / 'ESP32-DevKitC-V4/tests/miot'
OUT = ROOT / 'ESP32-S3-MINI/build/host-check'
OUT.mkdir(parents=True, exist_ok=True)

def run(args):
    result = subprocess.run([str(a) for a in args], capture_output=True, text=True,
                            encoding='utf-8', errors='replace')
    if result.returncode:
        raise RuntimeError(result.stdout[-3000:] + result.stderr[-6000:])
    if result.stdout.strip():
        print(result.stdout.strip())

for name in ['miot_bridge.h', 'miot_profile.h', 'miot_simulator.h',
             'miot_simulated_properties.h', 'midea_uart_state.h', 'miot_auto_mode.h',
             'http_request_gate.h', 'responsive_web_server.h', 'device_metrics.h']:
    assert (CLASSIC / name).read_bytes() == (S3 / name).read_bytes(), name + ' differs'
print('PASS: shared MIoT/state/HTTP/metrics implementations match classic 2.5.7')

classic_ino = (CLASSIC / 'esp32_uart0_listener.ino').read_text(encoding='utf-8')
s3_ino = (S3 / 'midea_s3_bridge.ino').read_text(encoding='utf-8')
route = r'server\.on\("([^\"]+)"\s*,\s*(HTTP_\w+)'
assert set(re.findall(route, classic_ino)) == set(re.findall(route, s3_ino)), 'HTTP routes differ'
for name in ['sendStatusQuery', 'sendSetState', 'flushRxFrame', 'serviceSerial',
             'executeAcCommand', 'handleFirmwareUpdateUpload', 'handleFirmwareUpdateResult',
             'handleMockMode', 'handleMiotSimulation', 'handleDisplayConfig', 'handleDisplayControl',
             'serviceBleCommands', 'saveWebPassword', 'handleHomeKitUnpair']:
    def body(source):
        begin = re.search(r'^static [^\n]+\b' + name + r'\([^\n]*\)\s*\{', source, re.M).start()
        return source[begin:source.index('\n}', begin)+2]
    assert body(classic_ino) == body(s3_ino), name + ' differs from classic'
print('PASS: all HTTP routes and control/receive/OTA/BLE/settings handlers match')

zig = ROOT / 'Midea-3AC-HomeKit/tests/device_store/.build/zig-windows-x86_64-0.13.0/zig.exe'
if not zig.is_file():
    zig = ROOT / 'ESP32-DevKitC-V4/build/host-cpp/ziglang/zig.exe'
compiler = shutil.which('g++')
if compiler:
    command = [compiler]
elif zig.is_file():
    command = [zig, 'c++', '-target', 'x86_64-windows-gnu']
    os.environ.setdefault('ZIG_GLOBAL_CACHE_DIR', str(ROOT / '.tmp/fan-regression-zig'))
    os.environ.setdefault('ZIG_LOCAL_CACHE_DIR', str(ROOT / '.tmp/fan-regression-local'))
else:
    raise RuntimeError('No installed native compiler found; this script does not install one.')
for board in [CLASSIC, S3]:
    for name in ['transport_test', 'http_gate_test']:
        exe = OUT / (board.parent.name + '-' + name + '.exe')
        run(command + ['-std=c++17', '-Wall', '-Wextra', '-Werror',
                       '-I' + str(TESTS / 'stubs'), '-I' + str(board),
                       TESTS / (name + '.cpp'), '-o', exe])
        run([exe])

node = shutil.which('node') or r'C:\Program Files\nodejs\node.exe'
for board in [CLASSIC, S3]:
    for name in ['web_pages.h']:
        source = (board / name).read_text(encoding='utf-8-sig')
        for i, script in enumerate(re.findall(r'<script>(.*?)</script>', source, re.S)):
            path = OUT / (board.parent.name + '-' + name + '-' + str(i) + '.js')
            path.write_text(script, encoding='utf-8')
            run([node, '--check', path])
print('PASS: both boards web scripts parse')

ino = (S3 / 'midea_s3_bridge.ino').read_text(encoding='utf-8')
assert 'MIOT_RX_PIN = 7;' in ino and 'MIOT_TX_PIN = 8;' in ino
assert 'MODULE_RX_PIN = 17;' in ino and 'MODULE_TX_PIN = 16;' in ino
assert 'pinMatrixOutDetach(MODULE_TX_PIN' in ino and 'pinMatrixOutAttach(MODULE_TX_PIN, U2TXD_OUT_IDX' in ino
assert 'displayProbe' not in ino and '/api/display/' not in ino
print('PASS: S3 UART pin assignments and on-demand TX gating')
print('Host checks do not verify electrical UART timing, hardware beep, or physical HomeKit pairing.')
