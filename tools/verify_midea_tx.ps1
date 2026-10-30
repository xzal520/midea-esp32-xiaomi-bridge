$ErrorActionPreference = 'Stop'
$taskProject = Split-Path -Parent $PSScriptRoot
$taskRepo = Split-Path -Parent $taskProject
$taskSource = [IO.File]::ReadAllText((Join-Path $taskProject 'firmware\midea_s3_bridge.ino'))
$taskOut = Join-Path $taskProject 'build\host-check\midea-tx'
New-Item -ItemType Directory -Path $taskOut -Force | Out-Null
$taskZig = Join-Path $taskRepo 'ESP32-DevKitC-V4\build\host-cpp\ziglang\zig.exe'
if (-not (Test-Path -LiteralPath $taskZig)) { throw 'Installed Zig C++ compiler not found.' }
$taskPrelude = @'
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <vector>
using gpio_num_t = int;
constexpr int MODULE_TX_PIN=16, MODULE_RX_PIN=17, MODULE_BAUD=9600;
constexpr int SERIAL_8N1=0, GPIO_MODE_INPUT=1, GPIO_MODE_OUTPUT=2, GPIO_FLOATING=0;
constexpr unsigned U2TXD_OUT_IDX=18, SIG_GPIO_OUT_IDX=256;
struct Route { unsigned func_sel=SIG_GPIO_OUT_IDX, oen_sel=1, inv_sel=0, oen_inv_sel=0; };
struct Registers { Route func_out_sel_cfg[54]; struct { uint32_t val=0; } enable; } GPIO;
static bool floating=false;
static unsigned logs=0, directionChanges=0;
static void gpio_set_direction(gpio_num_t p, int mode) {
  ++directionChanges;
  GPIO.func_out_sel_cfg[p].oen_sel=1;
  if(mode==GPIO_MODE_OUTPUT) {
    // ESP-IDF gpio_output_enable resets the matrix to ordinary GPIO output.
    GPIO.func_out_sel_cfg[p].func_sel=SIG_GPIO_OUT_IDX;
    GPIO.enable.val|=1u<<p;
  } else GPIO.enable.val&=~(1u<<p);
}
static void gpio_set_pull_mode(gpio_num_t, int mode) { floating=(mode==GPIO_FLOATING); }
static void gpio_ll_set_output_enable_ctrl(Registers *r, unsigned p, bool peripheral, bool inverted) {
  r->func_out_sel_cfg[p].oen_sel=!peripheral;
  r->func_out_sel_cfg[p].oen_inv_sel=inverted;
}
static void gpio_ll_output_disable(Registers *r, unsigned p) { r->enable.val&=~(1u<<p); }
static void gpio_ll_output_enable(Registers *r, unsigned p) { r->enable.val|=1u<<p; }
static void startMiotSerialPort() {}
static void addTextLog(const char *, const char *) { ++logs; }
struct MockSerial {
  size_t txBuffer=256;
  unsigned beginCount=0;
  std::vector<uint8_t> fifo, deferred, wire;
  void setRxBufferSize(size_t) {}
  void setTxBufferSize(size_t n) { txBuffer=n; }
  void begin(unsigned, int, int, int tx) {
    ++beginCount;
    GPIO.func_out_sel_cfg[tx].func_sel=U2TXD_OUT_IDX;
    GPIO.enable.val|=1u<<tx;
  }
  size_t write(const uint8_t *p, size_t n) {
    assert(GPIO.enable.val & (1u<<MODULE_TX_PIN));
    assert(GPIO.func_out_sel_cfg[MODULE_TX_PIN].func_sel==U2TXD_OUT_IDX);
    auto &target=txBuffer ? deferred : fifo;
    target.insert(target.end(),p,p+n);
    return n;
  }
  void flush() {
    // The bundled Arduino core checks hardware idle, not its software TX ring.
    if(fifo.empty()) return;
    assert(GPIO.enable.val & (1u<<MODULE_TX_PIN));
    assert(GPIO.func_out_sel_cfg[MODULE_TX_PIN].func_sel==U2TXD_OUT_IDX);
    wire.insert(wire.end(),fifo.begin(),fifo.end());
    fifo.clear();
  }
} Serial2;
'@
$taskFunctions = foreach ($taskName in @('startBridgeSerialPorts','mideaTxRouteReady','beginMideaTx','endMideaTx','sendMideaFrame')) {
    $taskMatch = [regex]::Match($taskSource, '(?ms)^static (?:bool|void) ' + $taskName + '\([^\r\n]*\) \{.*?^\}')
    if (-not $taskMatch.Success) { throw "Missing production function: $taskName" }
    $taskMatch.Value
}
$taskMain = @'
int main() {
  startBridgeSerialPorts();
  assert(mideaTxRouteReady());
  assert(!(GPIO.enable.val & (1u<<MODULE_TX_PIN)) && floating);
  assert(Serial2.txBuffer==0);
  const uint8_t frame[]={0xAA,0x21,0xAC,0x00,0xFF,0x2F};
  for(int i=0;i<3;++i) {
    assert(sendMideaFrame(frame,sizeof(frame)));
    assert(mideaTxRouteReady());
    assert(!(GPIO.enable.val & (1u<<MODULE_TX_PIN)));
    assert(Serial2.fifo.empty() && Serial2.deferred.empty());
  }
  assert(Serial2.wire.size()==3*sizeof(frame));
  for(size_t i=0;i<Serial2.wire.size();++i) assert(Serial2.wire[i]==frame[i%sizeof(frame)]);
  assert(Serial2.beginCount==1 && directionChanges==1);
  const auto sizeBefore=Serial2.wire.size();
  GPIO.func_out_sel_cfg[MODULE_TX_PIN].func_sel=SIG_GPIO_OUT_IDX;
  assert(!sendMideaFrame(frame,sizeof(frame)));
  assert(Serial2.wire.size()==sizeBefore && logs==1);
  assert(!(GPIO.enable.val & (1u<<MODULE_TX_PIN)));
  assert(!sendMideaFrame(nullptr,1) && !sendMideaFrame(frame,0));
  std::puts("PASS: production TX functions preserve UART routing, send complete repeated frames, release the pad, and reject a corrupted route.");
  std::puts("Host model covers SDK GPIO and FIFO ordering; physical levels and contention require hardware measurement.");
}
'@
$taskCpp = Join-Path $taskOut 'midea_tx_gate.cpp'
$taskExe = Join-Path $taskOut 'midea_tx_gate.exe'
[IO.File]::WriteAllText($taskCpp, $taskPrelude + "`n" + ($taskFunctions -join "`n") + "`n" + $taskMain, [Text.UTF8Encoding]::new($false))
$env:ZIG_GLOBAL_CACHE_DIR = Join-Path $taskOut 'zig-global'
$env:ZIG_LOCAL_CACHE_DIR = Join-Path $taskOut 'zig-local'
& $taskZig c++ -std=c++17 -Wall -Wextra -Werror $taskCpp -o $taskExe
if ($LASTEXITCODE -ne 0) { throw 'TX gate host test compilation failed.' }
& $taskExe
if ($LASTEXITCODE -ne 0) { throw 'TX gate regression failed.' }
