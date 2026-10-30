#include <cassert>
#include <deque>
#include <iostream>
#include <string>
#include <vector>
#include "miot_bridge.h"
#include "midea_uart_state.h"

struct Serial : Stream {
  std::deque<char> input;
  std::vector<std::string> output;
  std::string line;
  int available() override { return int(input.size()); }
  int read() override { char c=input.front();input.pop_front();return uint8_t(c); }
  size_t write(uint8_t c) override {
    if(c=='\r'){output.push_back(line);line.clear();}else line+=char(c);
    return 1;
  }
  void feed(const std::string &s){for(char c:s+"\r")input.push_back(c);}
};
static unsigned writes=0;
static bool enabled=false,targetPower=false,accept=true;
static uint16_t minutes=0;
static bool captureTimer(bool active,bool power,uint16_t delay) {
  ++writes;enabled=active;targetPower=power;minutes=delay;return accept;
}
static bool readTimer(String &value){value="1,5,1";return true;}
struct Bench {
  Serial io;
  MiotMideaBridge bridge;
  uint32_t now=1;
  Bench() {
    bridge.begin(io,nullptr,nullptr,nullptr,"C20",nullptr,captureTimer,readTimer);
    bridge.tick(now);tick(1000);rx("xiaomi.aircondition.c20");rx("cloud");rx("ok");rx("down none");
    assert(bridge.initialized());
    MideaAcSnapshot state;state.known=true;state.receivedAt=now;bridge.updateMidea(state);
  }
  void tick(uint32_t delta=1){now+=delta;bridge.tick(now);}
  void rx(const std::string &s){io.feed(s);tick();}
  std::string last() const {return io.output.empty()?"":io.output.back();}
};
int main() {
  {
    // Exact captured wire command: quotes survive tokenization and must be decoded.
    Bench t;const auto before=writes;
    t.rx("down set_properties 10 3 \"1,5,1\"");
    assert(writes==before+1 && enabled && targetPower && minutes==5);
    MideaTimers native;native.setRelative(minutes,0,t.now);
    uint8_t frame[17]={};native.applyTo(frame,true);
    MideaTimers feedback;feedback.captureFromValidatedC0(frame,t.now);
    assert(feedback.on && feedback.onMinutes==5 && !feedback.off);
    assert(t.bridge.pending() && t.last()!="result 10 3 0");
    t.bridge.resolveTimer(true);t.tick();
    assert(!t.bridge.pending() && t.last()=="result 10 3 0");
  }
  {
    Bench t;const auto before=writes;t.rx("down set_properties 10 3 \"0\"");
    assert(writes==before+1 && !enabled && minutes==0 && t.bridge.pending());
    t.bridge.resolveTimer(true);t.tick();assert(t.last()=="result 10 3 0");
  }
  {
    Bench t;t.rx("down set_properties 10 3 \"1,2,0\"");
    assert(enabled && !targetPower && minutes==2 && t.bridge.pending());
    t.bridge.resolveTimer(false);t.tick();assert(t.last()=="result 10 3 -4004");
  }
  {
    Bench t;t.rx("down get_properties 10 3");
    assert(t.last()=="result 10 3 0 \"1,5,1\"");
  }
  for(const uint16_t delay:{1,5,25,60,255,256,1440}) {
    for(const bool power:{false,true}) {
      Bench t;const auto before=writes;
      t.rx(std::string("down set_properties 10 3 \"1,")+std::to_string(delay)+","+(power?"1":"0")+"\"");
      assert(writes==before+1 && enabled && targetPower==power && minutes==delay && t.bridge.pending());
      MideaTimers native;native.setRelative(power?minutes:0,power?0:minutes,t.now);
      uint8_t frame[17]={};native.applyTo(frame,true);
      MideaTimers feedback;feedback.captureFromValidatedC0(frame,t.now);
      assert((power?feedback.onMinutes:feedback.offMinutes)==delay);
      t.bridge.resolveTimer(true);t.tick();assert(t.last()=="result 10 3 0");
    }
  }
  for(const char *value:{"\"1,0,1\"","\"1,1441,1\"","\"1,-5,1\"","\"1,5,2\"","\"1,5,1x\"","\"\""}) {
    Bench t;const auto before=writes;t.rx(std::string("down set_properties 10 3 ")+value);
    assert(writes==before && !t.bridge.pending() && t.last()=="result 10 3 -4002");
  }
  {
    Bench t;const auto before=writes;t.rx("down set_properties 10 3 0");
    assert(writes==before+1 && !enabled && minutes==0);
    t.bridge.resolveTimer(true);t.tick();assert(t.last()=="result 10 3 0");
  }
  {
    Bench t;accept=false;t.rx("down set_properties 10 3 \"1,5,1\"");accept=true;
    assert(!t.bridge.pending() && t.last()=="result 10 3 -4004");
  }
  std::cout<<"PASS: minute units from MIoT wire writes through native timer bytes, cancellation, feedback and invalid inputs\n";
}
