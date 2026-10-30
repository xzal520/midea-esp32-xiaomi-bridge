"""Check real MIoT transport and the firmware's actual countdown readback body."""
import os
from pathlib import Path
import re
import shutil
import subprocess

PROJECT = Path(__file__).resolve().parents[2]
REPO = PROJECT.parent
OUT = PROJECT / 'build/host-check'
OUT.mkdir(parents=True, exist_ok=True)
COMPILER = os.environ.get('ZIG') or shutil.which('zig') or str(REPO / 'ESP32-DevKitC-V4/build/host-cpp/ziglang/zig.exe')
os.environ['ZIG_GLOBAL_CACHE_DIR'] = str(PROJECT / 'build/zig-global')
os.environ['ZIG_LOCAL_CACHE_DIR'] = str(PROJECT / 'build/zig-local')

def run(args):
    result = subprocess.run([str(arg) for arg in args], capture_output=True,
                            text=True, encoding='utf-8', errors='replace')
    if result.returncode:
        raise RuntimeError(result.stdout + result.stderr)
    if result.stdout.strip():
        print(result.stdout.strip())

def check(source):
    exe = OUT / (source.stem + '.exe')
    run([COMPILER, 'c++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
         '-I' + str(PROJECT / 'tests/support'),
         '-I' + str(PROJECT / 'firmware'), source, '-o', exe])
    run([exe])

check(PROJECT / 'tests/miot/countdown_transport_test.cpp')
sketch = (PROJECT / 'firmware/midea_s3_bridge.ino').read_text(encoding='utf-8')
getter = re.search(r'^static bool getCountdownForMiot\(String &value\) \{.*?^\}',
                   sketch, re.M | re.S)
assert getter, 'Cannot locate actual countdown readback function'
source = OUT / 'countdown_readback_test.cpp'
source.write_text('''#include <cassert>
#include <iostream>
#include "midea_uart_state.h"
static uint32_t millis(){return 1000;}
static bool mockMode=false,statusKnown=true,statusPower=false;
static uint32_t lastStatusAt=1000;
static MideaTimers mideaTimers,requestedTimers;
''' + getter.group() + '''
int main(){
  String value;
  for(uint16_t minutes:{1,5,25,60,255,256,1440}){
    for(bool power:{false,true}){
      mideaTimers.setRelative(power?minutes:0,power?0:minutes,millis());
      assert(getCountdownForMiot(value));
      assert(value==String("1,")+String(minutes)+","+String(power?0:1));
    }
  }
  // Captured C0 timer bytes: ON disabled (7F), OFF armed for 60/59 minutes (83).
  uint8_t captured[17]={};captured[14]=0x7F;captured[15]=0x83;
  requestedTimers.setRelative(5,0,millis());
  mideaTimers.captureFromValidatedC0(captured,millis());
  assert(getCountdownForMiot(value) && value=="1,60,1");
  captured[16]=1;mideaTimers.captureFromValidatedC0(captured,millis());
  assert(getCountdownForMiot(value) && value=="1,59,1");
  mideaTimers.setRelative(0,0,millis());
  assert(getCountdownForMiot(value) && value=="0,0,0");
  mideaTimers.known=false;assert(!getCountdownForMiot(value));
  std::cout<<"PASS: actual firmware readback preserves native minutes without hour rounding\\n";
}
''', encoding='utf-8')
check(source)
check(PROJECT / 'tests/miot/feature_transport_test.cpp')
builder = re.search(r'^static bool sendSetStateWithFeatures\([^\n]+\) \{.*?^\}', sketch, re.M | re.S)
aux_setter = re.search(r'^static bool setAuxHeatFromMiot\([^\n]+\) \{.*?^\}', sketch, re.M | re.S)
display = re.search(r'^static void sendDisplayToggle\(\) \{.*?^\}', sketch, re.M | re.S)
assert builder and aux_setter and display, 'Cannot locate actual native feature sender'
source = OUT / 'native_feature_sender_test.cpp'
source.write_text('''#include <cassert>
#include <iostream>
#include <vector>
#include "midea_uart_state.h"
static bool mockMode=false,io16RxMode=false,statusKnown=false,statusPower=false,statusEco=false,statusSleep=false;
static bool statusDisplayOn=true,statusAuxHeat=false,roomTemperatureKnown=false;
static uint8_t statusMode=2,statusFan=102,statusSwing=0;
static float statusTemperature=26,roomTemperature=0;
static uint32_t lastStatusAt=0;
static uint32_t millis(){return 1000;}
static MiotMideaBridge miotBridge;
static MideaCommandFlags mideaCommandFlags;
static MideaTimers mideaTimers;
static std::vector<uint8_t> wire;
static bool sendMideaFrame(const uint8_t *frame,size_t size){wire.assign(frame,frame+size);return true;}
static void addLog(const char *,const uint8_t *,size_t){}
static uint8_t crc8Midea(const uint8_t *data,size_t size){return mideaStateCrc8(data,size);}
''' + display.group() + '\n' + builder.group() + '\n' + aux_setter.group() + '''
int main(){
  mideaCommandFlags.byte9=0x2C;mideaTimers.setRelative(17,38,millis());
  MideaAcSnapshot wanted;wanted.mode=4;wanted.target=27.5f;wanted.fan=45;wanted.eco=true;wanted.displayOn=true;
  assert(setAuxHeatFromMiot(wanted));
  assert(wire.size()==37 && wire[11]==0x42 && wire[19]==0xA4); // Clear aux only, keep ECO/dry/anion.
  assert(wire[14]==mideaTimers.wireOn && wire[15]==mideaTimers.wireOff && wire[16]==mideaTimers.wireMinutes);
  assert(!(wire[13]&0x80) && !(wire[11]&1)); // No timer rewrite or accidental power-on.
  uint8_t sum=0;for(size_t i=1;i<wire.size();++i)sum+=wire[i];assert(sum==0);
  assert(mideaStateCrc8(wire.data()+10,25)==wire[35]);
  wanted.auxHeat=true;assert(setAuxHeatFromMiot(wanted));assert(wire[19]==0xAC);
  assert(!statusAuxHeat); // Sending does not manufacture confirmed hardware state.
  miotBridge.updateBuzzer(false,millis());assert(setAuxHeatFromMiot(wanted));
  assert(wire[11]==0x02 && wire[19]==0xAC);
  wanted.power=true;assert(setAuxHeatFromMiot(wanted));assert(wire[11]==0x03);
  sendDisplayToggle();assert(wire.size()==34 && !(wire[11]&0x40));
  miotBridge.updateBuzzer(true,millis());assert(setAuxHeatFromMiot(wanted));assert(wire[11]==0x43);
  sendDisplayToggle();assert((wire[11]&0x40)!=0);
  io16RxMode=true;const auto before=wire;assert(!setAuxHeatFromMiot(wanted) && wire==before);
  std::cout<<"PASS: actual native senders preserve power/timers and honor confirmed buzzer state\\n";
}
''', encoding='utf-8')
check(source)
