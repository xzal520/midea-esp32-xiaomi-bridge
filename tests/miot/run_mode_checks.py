import os
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[2]
out = root / 'build/mode-repeat-check'
out.mkdir(parents=True, exist_ok=True)
temp = out / 'temp'
temp.mkdir(exist_ok=True)
os.environ['TEMP'] = os.environ['TMP'] = str(temp)
os.environ['ZIG_GLOBAL_CACHE_DIR'] = str(root / 'build/zig-global')
os.environ['ZIG_LOCAL_CACHE_DIR'] = str(root / 'build/zig-local')
compiler = root.parent / 'ESP32-DevKitC-V4/build/host-cpp/ziglang/zig.exe'
def run(args):
    result = subprocess.run([str(x) for x in args], capture_output=True, text=True, encoding='utf-8', errors='replace')
    if result.returncode:
        (out / 'failure.log').write_text(result.stdout + result.stderr, encoding='utf-8')
        raise RuntimeError((result.stdout + result.stderr)[-6000:])
    if result.stdout.strip():
        print(result.stdout.strip(), flush=True)
for name in ('mode_repeat_transport_test', 'feature_transport_test', 'countdown_transport_test'):
    exe = out / (name + '.exe')
    run([compiler, 'c++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-I' + str(root / 'tests/support'), '-I' + str(root / 'firmware'), root / ('tests/miot/' + name + '.cpp'), '-o', exe])
    run([exe])
