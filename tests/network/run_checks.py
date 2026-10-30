import os
from pathlib import Path
import re
import subprocess

root = Path(__file__).resolve().parents[2]
out = root / 'build/network-check'
out.mkdir(parents=True, exist_ok=True)
temp = out / 'temp'
temp.mkdir(exist_ok=True)
os.environ['TEMP'] = os.environ['TMP'] = str(temp)
compiler = root.parent / 'ESP32-DevKitC-V4/build/host-cpp/ziglang/zig.exe'
os.environ['ZIG_GLOBAL_CACHE_DIR'] = str(root / 'build/zig-global')
os.environ['ZIG_LOCAL_CACHE_DIR'] = str(root / 'build/zig-local')
def run(args):
    result = subprocess.run([str(x) for x in args], capture_output=True, text=True, encoding='utf-8', errors='replace')
    if result.returncode:
        (out / 'failure.log').write_text(result.stdout + result.stderr, encoding='utf-8')
        raise RuntimeError((result.stdout + result.stderr)[-6000:])
    if result.stdout.strip():
        print(result.stdout.strip(), flush=True)
exe = out / 'health_transport_test.exe'
run([compiler, 'c++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-I' + str(root / 'firmware'), root / 'tests/network/health_transport_test.cpp', '-o', exe])
run([exe])
web = root / 'firmware/web_pages.h'
script = re.search(r'static const char WEB_SCRIPT\[\] PROGMEM = R"JS\((.*?)\)JS";', web.read_text(encoding='utf-8'), re.S).group(1)
script_file = out / 'web-script.js'
script_file.write_text(script, encoding='utf-8')
run(['node', '--check', script_file])
run(['node', root / 'tests/network/loader_test.js', web])
run(['node', root / 'tests/io16_rx/web_test.js', script_file])
