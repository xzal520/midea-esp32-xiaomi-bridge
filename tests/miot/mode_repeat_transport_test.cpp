#include <cassert>
#include <deque>
#include <iostream>
#include <string>
#include <vector>
#include "miot_bridge.h"

struct Serial : Stream {
  std::deque<char> input;std::vector<std::string> output;std::string line;
  int available() override{return int(input.size());}
  int read() override{char c=input.front();input.pop_front();return uint8_t(c);}
  size_t write(uint8_t c) override{if(c=='\r'){output.push_back(line);line.clear();}else line+=char(c);return 1;}
  void feed(const std::string &s){for(char c:s+"\r")input.push_back(c);}
};
static std::vector<MideaAcSnapshot> writes;
static void capture(bool power,uint8_t mode,float target,uint8_t fan,uint8_t swing,bool eco,bool sleep,bool display){
  MideaAcSnapshot s;s.power=power;s.mode=mode;s.target=target;s.fan=fan;s.swing=swing;s.eco=eco;s.sleep=sleep;s.displayOn=display;writes.push_back(s);
}
struct Bench {
  Serial io;MiotMideaBridge bridge;uint32_t now=1;MideaAcSnapshot actual;
  Bench(uint8_t mode=2,bool power=true){
    writes.clear();bridge.begin(io,nullptr,capture,nullptr,"C20");
    bridge.tick(now);tick(1000);rx("xiaomi.aircondition.c20");rx("cloud");rx("ok");rx("down none");
    assert(bridge.initialized());actual.known=true;actual.power=power;actual.mode=mode;actual.target=26;actual.fan=45;actual.displayOn=true;feedback();
  }
  void tick(uint32_t delta=1){now+=delta;bridge.tick(now);}
  void rx(const std::string &s){
    // The module acknowledges a notification before the next down RPC.
    if(s.rfind("down ",0)==0 && last().rfind("properties_changed",0)==0){io.feed("ok");tick();}
    io.feed(s);tick();
  }
  void feedback(){actual.receivedAt=now;bridge.updateMidea(actual);}
  std::string last()const{return io.output.empty()?"":io.output.back();}
  void repeated(){
    const auto count=writes.size();rx("down set_properties 2 2 6");
    assert(writes.size()==count);assert(!bridge.pending());
    if(last()!="result 2 2 0")std::cerr<<"repeat mode="<<int(actual.mode)<<" known="<<actual.known<<" power="<<actual.power<<" last="<<last()<<"\n";
    assert(last()=="result 2 2 0");
  }
};

int main(){
  // This must not depend on a previous MIoT mode read/write populating a cache.
  for(uint8_t mode:{uint8_t(1),uint8_t(2),uint8_t(3),uint8_t(4),uint8_t(5)}){
    Bench t(mode);const auto display=t.bridge.displayMode();t.repeated();assert(t.bridge.displayMode()==display);
    t.rx("ok");t.repeated(); // Repeated duplicate requests stay inert.
  }
  {
    Bench t;t.rx("down get_properties 2 2");assert(t.last()=="result 2 2 0 2");t.rx("ok");t.repeated();
  }
  for(uint8_t mode:{uint8_t(2),uint8_t(3),uint8_t(4),uint8_t(5)}){
    Bench t(mode);const int miotMode=mode==4?5:mode==5?4:mode;
    t.rx("down set_properties 2 2 "+std::to_string(miotMode));
    assert(writes.empty() && !t.bridge.pending() && t.last()=="result 2 2 0");
  }
  {
    Bench t(4);t.rx("down set_properties 2 2 2");
    assert(writes.size()==1 && writes.back().power && writes.back().mode==2 && t.bridge.pending());
    t.actual.mode=2;t.feedback();t.tick();assert(t.last()=="result 2 2 0");t.rx("ok");t.repeated();
  }
  {
    Bench t(2);t.rx("down set_properties 2 2 5");
    assert(writes.size()==1 && writes.back().power && writes.back().mode==4);
    t.actual.mode=4;t.feedback();t.tick();t.rx("ok");t.repeated();
  }
  {
    Bench t(4);t.rx("down set_properties 2 2 2");
    t.rx("down set_properties 2 2 6");assert(writes.size()==1 && t.bridge.pending());
    t.actual.mode=2;t.feedback();t.tick();assert(!t.bridge.pending());
  }
  {
    Bench t(2,false);t.repeated();t.rx("ok");t.rx("down get_properties 2 1 2 2");
    assert(t.last()=="result 2 1 0 false 2 2 0 6");
  }
  {
    Bench t;t.actual.known=false;t.feedback();t.repeated();assert(t.bridge.displayMode()==-1);
  }
  {
    Bench t;t.tick(16000);t.repeated(); // An inert request cannot replay a stale power state.
  }
  for(const char *command:{"down set_properties 2 2 6 2 4 27", "down set_properties 2 4 27 2 2 6"}){
    Bench t;t.rx(command);assert(writes.size()==1 && writes.back().power && writes.back().mode==2 && writes.back().target==27);
    t.actual.target=27;t.feedback();t.tick();assert(!t.bridge.pending());
  }
  {
    Bench t(1);t.rx("down set_properties 2 2 6 2 4 27");
    assert(writes.size()==1 && writes.back().power && writes.back().mode==1 && writes.back().target==27);
  }
  for(const char *command:{"down set_properties 2 2 6 2 2 2", "down set_properties 2 2 2 2 2 6"}){
    Bench t(4);t.rx(command);assert(writes.size()==1 && writes.back().power && writes.back().mode==2);
  }
  for(const char *command:{"down set_properties 2 2 6 2 1 false", "down set_properties 2 1 false 2 2 6"}){
    Bench t;t.rx(command);assert(writes.size()==1 && !writes.back().power && writes.back().mode==2);
    t.actual.power=false;t.feedback();t.tick();assert(!t.bridge.pending());
  }
  {
    Bench t;t.rx("down set_properties 2 1 false");assert(writes.size()==1 && !writes.back().power);
  }
  {
    Bench t(2,false);t.rx("down set_properties 2 2 6 2 1 true");
    assert(writes.size()==1 && writes.back().power && writes.back().mode==2);
  }
  {
    Bench t;t.rx("down set_properties 2 2 6 2 4 invalid");
    assert(writes.empty() && !t.bridge.pending() && t.last()=="result 2 2 0 2 4 -4002");
  }
  {
    Bench t;t.bridge.setSimulationEnabled(true);t.repeated();
  }
  {
    Bench t;t.bridge.setMockMode(true);t.repeated();
  }
  {
    Bench t;t.bridge.setMideaControlEnabled(false);t.repeated();
  }
  for(const char *invalid:{"0","1","7","6.5","\"6\""}){
    Bench t;t.rx(std::string("down set_properties 2 2 ")+invalid);
    assert(writes.empty() && !t.bridge.pending() && t.last()=="result 2 2 -4002");
  }
  std::cout<<"PASS: C20 repeated mode=6 never writes AC state; real mode/power/temperature changes and pending feedback remain correct\n";
}
