#include <cassert>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include "miot_bridge.h"

static uint32_t now=1000;
static uint32_t millis(){return now;}
static constexpr int MODULE_TX_PIN=16,MODULE_RX_PIN=17,MODULE_BAUD=9600;
static constexpr size_t MAX_FRAME_BYTES=256,LOG_HEX_CAPACITY=769,IO16_RX_LOG_CAPACITY=128,IO16_RX_FALLBACK_CAPACITY=8;
static constexpr uint32_t RX_FRAME_IDLE_MS=35;
struct LogEntry{uint32_t id,uptimeMs;char direction[8],hex[LOG_HEX_CAPACITY];};
static LogEntry *io16RxLogs=nullptr,io16RxFallback[IO16_RX_FALLBACK_CAPACITY];
static size_t io16RxLogCapacity=0,io16RxLogIndex=0,io16RxLogCount=0,io16RxFrameLength=0;
static uint32_t nextIo16RxLogId=1,io16RxBytes=0,io16RxFrames=0,io16RxOverflows=0,io16RxSignalErrors=0,lastIo16RxByteMs=0;
static uint8_t io16RxFrame[MAX_FRAME_BYTES],rxFrame[MAX_FRAME_BYTES],usbFrame[MAX_FRAME_BYTES];
static size_t rxFrameLength=0,usbFrameLength=0;
static uint32_t lastRxByteMs=0,lastUsbByteMs=0,lastMideaTxAt=0,scheduledQueryAt=0;
static bool io16RxMode=false,timerPending=false,timerMiotPending=false;
static const char *timerResultState="idle";static String timerResult;
static MiotMideaBridge miotBridge;
using QueueHandle_t=void *;static QueueHandle_t io16RxQueue=nullptr;
using gpio_num_t=int;using esp_err_t=int;
static constexpr int ESP_OK=0,UART_NUM_0=0,UART_PIN_NO_CHANGE=-1,UART_DATA_8_BITS=8,UART_PARITY_DISABLE=0;
static constexpr int UART_STOP_BITS_1=1,UART_HW_FLOWCTRL_DISABLE=0,UART_SCLK_DEFAULT=0;
static constexpr int GPIO_MODE_INPUT=1,GPIO_FLOATING=0,GPIO_PULLUP_ONLY=1,U2TXD_OUT_IDX=23,U0RXD_IN_IDX=7,GPIO_MATRIX_CONST_ONE_INPUT=56;
static constexpr int MALLOC_CAP_SPIRAM=1,MALLOC_CAP_8BIT=2,pdTRUE=1;
enum {UART_FIFO_OVF,UART_BUFFER_FULL,UART_FRAME_ERR,UART_PARITY_ERR,UART_BREAK};
struct uart_event_t{int type;};
struct uart_config_t{int baud_rate,data_bits,parity,stop_bits,flow_ctrl,source_clk;};
static bool installed=false,failInstall=false,failConfig=false,failPins=false,failDelete=false,failAllocation=false;
static std::deque<uint8_t> input16;static std::deque<int> events;
static int inputPin=-1,installedCount=0;
struct Route{int func_sel=0;bool oen_sel=false,inv_sel=false,oen_inv_sel=false;};
struct Registers{uint32_t enable=0;Route func_out_sel_cfg[49];} GPIO;
static void gpio_ll_output_disable(Registers *gpio,int pin){assert(pin==16);gpio->enable&=~(1UL<<pin);}
static void gpio_ll_output_enable(Registers *gpio,int pin){assert(pin==16);gpio->enable|=1UL<<pin;}
static void gpio_ll_set_output_enable_ctrl(Registers *gpio,int pin,bool,bool){assert(pin==16);gpio->func_out_sel_cfg[pin].oen_sel=true;}
static void gpio_set_direction(int pin,int mode){assert(pin==16 && mode==GPIO_MODE_INPUT);GPIO.enable&=~(1UL<<pin);}
static void gpio_set_pull_mode(int pin,int){assert(pin==16);}
static void esp_rom_gpio_connect_out_signal(int pin,int signal,bool inv,bool oe){assert(pin==16);GPIO.func_out_sel_cfg[pin]={signal,false,inv,oe};}
static void esp_rom_gpio_connect_in_signal(int pin,int signal,bool inv){assert(signal==U0RXD_IN_IDX && !inv);inputPin=pin;}
static bool uart_is_driver_installed(int uart){assert(uart==0);return installed;}
static int uart_driver_install(int uart,int rx,int tx,int queueSize,QueueHandle_t *queue,int){
  assert(uart==0 && rx==2048 && tx==0 && queueSize==12);if(failInstall)return -1;
  installed=true;++installedCount;*queue=reinterpret_cast<void *>(1);return ESP_OK;
}
static int uart_param_config(int uart,const uart_config_t *c){assert(uart==0 && c->baud_rate==9600 && c->data_bits==8);return failConfig?-1:ESP_OK;}
static int uart_set_pin(int uart,int tx,int rx,int rts,int cts){
  assert(uart==0 && tx==-1 && rx==16 && rts==-1 && cts==-1);inputPin=rx;return failPins?-1:ESP_OK;
}
static int uart_driver_delete(int uart){assert(uart==0);if(failDelete)return -1;installed=false;return ESP_OK;}
static int uart_flush_input(int uart){assert(uart==0);input16.clear();return ESP_OK;}
static int uart_read_bytes(int uart,uint8_t *data,size_t size,int timeout){
  assert(uart==0 && timeout==0);size_t i=0;while(i<size && !input16.empty()){data[i++]=input16.front();input16.pop_front();}return int(i);
}
static int xQueueReceive(QueueHandle_t,uart_event_t *event,int){if(events.empty())return 0;event->type=events.front();events.pop_front();return pdTRUE;}
static void *heap_caps_calloc(size_t n,size_t size,int){return failAllocation?nullptr:std::calloc(n,size);}
struct Port{
  std::deque<uint8_t> input;std::vector<uint8_t> output;
  int available(){return int(input.size());}
  int read(){const auto value=input.front();input.pop_front();return value;}
  size_t write(uint8_t value){output.push_back(value);return 1;}
  size_t write(const uint8_t *data,size_t size){output.insert(output.end(),data,data+size);return size;}
  void flush(){}
} Serial,Serial2;
static std::vector<std::string> normalLogs;
static String bytesToHex(const uint8_t *data,size_t size){
  std::ostringstream out;for(size_t i=0;i<size;++i){if(i)out<<' ';out<<std::uppercase<<std::hex<<std::setw(2)<<std::setfill('0')<<int(data[i]);}return out.str();
}
static void addLog(const char *dir,const uint8_t *data,size_t size){normalLogs.push_back(std::string(dir)+":"+bytesToHex(data,size).c_str());}
static void addTextLog(const char *,const char *){}
static void flushRxFrame(){addLog("RX",rxFrame,rxFrameLength);rxFrameLength=0;}
#include "io16_rx_functions.inc"

int main(){
  static_assert(MODULE_RX_PIN==17,"Primary RX stays on IO17");
  restoreIo16TxRoute();assert(mideaTxRouteReady());String message;
  timerPending=timerMiotPending=true;
  assert(setIo16RxMode(true,message) && installed && inputPin==16 && io16RxMode);
  assert(!miotBridge.mideaControlEnabled() && !timerPending && !timerMiotPending && !std::strcmp(timerResultState,"error"));
  assert(!(GPIO.enable&(1UL<<16)) && !beginMideaTx());
  const auto count=installedCount;assert(setIo16RxMode(true,message) && installedCount==count);
  const uint8_t tx[]={0xAA,0x55};assert(!sendMideaFrame(tx,sizeof(tx)) && Serial2.output.empty());
  input16={0xDE,0xAD,0x00,0xFF};Serial2.input={0xAA,0xBB};Serial.input={0x11,0x22,0x33};
  serviceSerial();assert(Serial.input.empty() && Serial2.input.empty() && Serial2.output.empty());
  assert(io16RxBytes==4 && rxFrameLength==2 && usbFrameLength==0);
  now+=35;serviceSerial();assert(io16RxLogCount==1 && io16RxFrames==1);
  assert(!std::strcmp(io16RxLogs[0].hex,"DE AD 00 FF") && !std::strcmp(io16RxLogs[0].direction,"IO16RX"));
  assert(normalLogs.size()==1 && normalLogs[0]=="RX:AA BB"); // IO17 kept receiving; no IO16 bytes in normal log.
  events={UART_FIFO_OVF,UART_BUFFER_FULL,UART_FRAME_ERR,UART_PARITY_ERR,UART_BREAK};serviceIo16Rx();
  assert(io16RxOverflows==2 && io16RxSignalErrors==3);
  input16={0x42};serviceIo16Rx();assert(io16RxFrameLength==1);
  assert(setIo16RxMode(false,message) && !installed && !io16RxMode && io16RxLogCount==2);
  assert(inputPin==GPIO_MATRIX_CONST_ONE_INPUT && mideaTxRouteReady() && miotBridge.mideaControlEnabled());
  assert(!(GPIO.enable&(1UL<<16)) && scheduledQueryAt==now+200);
  assert(sendMideaFrame(tx,sizeof(tx)) && Serial2.output.size()==2 && !(GPIO.enable&(1UL<<16)));
  installed=true;assert(!setIo16RxMode(true,message) && !io16RxMode);installed=false;
  failInstall=true;assert(!setIo16RxMode(true,message) && !io16RxMode && mideaTxRouteReady());failInstall=false;
  failConfig=true;assert(!setIo16RxMode(true,message) && !installed && !io16RxMode);failConfig=false;
  failPins=true;assert(!setIo16RxMode(true,message) && !installed && !io16RxMode);failPins=false;
  assert(setIo16RxMode(true,message));failDelete=true;
  assert(!setIo16RxMode(false,message) && io16RxMode && !beginMideaTx());failDelete=false;
  for(int i=0;i<130;++i){input16.push_back(uint8_t(i));serviceIo16Rx();now+=35;serviceIo16Rx();}
  assert(io16RxLogCount==IO16_RX_LOG_CAPACITY);
  assert(io16RxLogs[io16RxLogIndex].id==nextIo16RxLogId-IO16_RX_LOG_CAPACITY);
  const auto before=io16RxBytes;for(int i=0;i<3200;++i)input16.push_back(uint8_t(i));
  serviceIo16Rx();assert(io16RxBytes-before==2048 && input16.size()==1152);
  serviceIo16Rx();now+=35;serviceIo16Rx();assert(input16.empty() && io16RxBytes-before==3200);
  assert(normalLogs.size()==1);assert(setIo16RxMode(false,message));
  std::free(io16RxLogs);io16RxLogs=nullptr;io16RxLogIndex=io16RxLogCount=0;nextIo16RxLogId=1;failAllocation=true;
  assert(setIo16RxMode(true,message) && io16RxLogs==io16RxFallback && io16RxLogCapacity==8);
  for(int i=0;i<10;++i){input16.push_back(uint8_t(i));serviceIo16Rx();now+=35;serviceIo16Rx();}
  assert(io16RxLogCount==8 && io16RxLogs[io16RxLogIndex].id==3);assert(setIo16RxMode(false,message));
  std::cout<<"PASS: IO16 RX-only UART, TX blocking/restoration, IO17 continuity, USB discard, isolated bounded logs and failure rollback\n";
}
