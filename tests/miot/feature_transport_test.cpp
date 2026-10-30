#include <cassert>
#include <deque>
#include <iostream>
#include <string>
#include <vector>
#include "miot_bridge.h"
#include "midea_uart_state.h"

struct Serial : Stream {
  std::deque<char> input;std::vector<std::string> output;std::string line;
  int available() override{return int(input.size());}
  int read() override{char c=input.front();input.pop_front();return uint8_t(c);}
  size_t write(uint8_t c) override{if(c=='\r'){output.push_back(line);line.clear();}else line+=char(c);return 1;}
  void feed(const std::string &s){for(char c:s+"\r")input.push_back(c);}
};
static unsigned stateWrites=0,auxWrites=0,cleanWrites=0,buzzerWrites=0;
static bool requestedClean=false,acceptAux=true,acceptClean=true;
static bool requestedBuzzer=true,acceptBuzzer=true;
static MideaAcSnapshot requestedState;
static void captureState(bool,uint8_t,float,uint8_t,uint8_t,bool,bool,bool){++stateWrites;}
static bool captureAux(const MideaAcSnapshot &state){++auxWrites;requestedState=state;return acceptAux;}
static bool captureClean(bool enabled){++cleanWrites;requestedClean=enabled;return acceptClean;}
static bool captureBuzzer(bool enabled){++buzzerWrites;requestedBuzzer=enabled;return acceptBuzzer;}
struct Bench {
  Serial io;MiotMideaBridge bridge;uint32_t now=1;MideaAcSnapshot actual;
  Bench(){
    bridge.begin(io,nullptr,captureState,nullptr,"C20",nullptr,nullptr,nullptr,captureAux,captureClean,captureBuzzer);
    bridge.tick(now);tick(1000);rx("xiaomi.aircondition.c20");rx("cloud");rx("ok");rx("down none");
    assert(bridge.initialized());actual.known=true;actual.mode=4;actual.target=27.5f;feedback();
  }
  void tick(uint32_t delta=1){now+=delta;bridge.tick(now);}
  void rx(const std::string &s){io.feed(s);tick();}
  void feedback(){actual.receivedAt=now;bridge.updateMidea(actual);}
  std::string last()const{return io.output.empty()?"":io.output.back();}
};
static void finishFrame(std::vector<uint8_t> &frame){
  frame[0]=0xAA;frame[1]=uint8_t(frame.size()-1);frame[2]=0xAC;
  frame[frame.size()-2]=mideaStateCrc8(frame.data()+10,frame.size()-12);
  uint8_t sum=0;for(size_t i=1;i<frame.size()-1;++i)sum+=frame[i];frame.back()=uint8_t(0-sum);
}
static std::vector<uint8_t> cleanReply(bool enabled,bool reserved=false,uint8_t kind=3,uint8_t body=0xB1){
  std::vector<uint8_t> frame(12,0);frame[8]=2;frame[9]=kind;frame[10]=body;frame[11]=1;
  frame.push_back(0x39);frame.push_back(0);if(reserved)frame.push_back(0);
  frame.push_back(1);frame.push_back(enabled?1:0);frame.push_back(7);frame.push_back(0);frame.push_back(0);
  finishFrame(frame);return frame;
}
int main(){
  {
    Bench t;const auto before=auxWrites+cleanWrites+buzzerWrites+stateWrites;
    t.bridge.setMideaControlEnabled(false);assert(!t.bridge.mideaControlEnabled());
    t.rx("down set_properties 2 9 true");assert(t.last()=="result 2 9 -4004");
    assert(auxWrites+cleanWrites+buzzerWrites+stateWrites==before);
    t.rx("ok");const auto first=t.io.output.size();t.rx("down get_properties 2 1");t.rx("ok");
    bool replied=false;for(size_t i=first;i<t.io.output.size();++i)if(t.io.output[i]=="result 2 1 0 false")replied=true;
    assert(replied);
  }
  {
    Bench t;t.rx("down set_properties 2 9 true");const auto before=auxWrites;
    t.bridge.setMideaControlEnabled(false);t.tick();assert(!t.bridge.pending() && t.last()=="result 2 9 -4004");
    t.actual.auxHeat=true;t.feedback();t.tick();assert(auxWrites==before);
    t.bridge.setMideaControlEnabled(true);assert(auxWrites==before && t.bridge.mideaControlEnabled());
  }
  {
    Bench t;t.bridge.setMideaControlEnabled(false);const auto before=stateWrites;
    t.rx("down set_properties 2 10 true 9 2 \"1\"");
    assert(t.last()=="result 2 10 0 9 2 0" && stateWrites==before);
  }
  {
    Bench t;t.actual.known=false;t.feedback();
    t.rx("down get_properties 2 10 9 2");assert(t.last()=="result 2 10 0 false 9 2 0 \"\"");
  }
  {
    Bench t;t.actual.known=false;t.feedback();
    const auto before=stateWrites+auxWrites+cleanWrites+buzzerWrites;
    t.rx("down set_properties 2 10 true 9 2 \"1\"");
    assert(t.last()=="result 2 10 0 9 2 0" && !t.bridge.pending());
    assert(stateWrites+auxWrites+cleanWrites+buzzerWrites==before);
    t.rx("ok");t.rx("ok");t.rx("down get_properties 2 10 9 2");
    assert(t.last()=="result 2 10 0 true 9 2 0 \"1\"");
  }
  {
    Bench t;t.bridge.setSimulationEnabled(true);t.rx("down get_properties 5 1");
    assert(t.last()=="result 5 1 -4004"); // Unknown hardware state cannot come from simulation.
  }
  {
    Bench t;const auto before=buzzerWrites,stateBefore=stateWrites;
    t.rx("down set_properties 5 1 false");
    assert(buzzerWrites==before+1 && !requestedBuzzer && stateWrites==stateBefore);
    assert(t.bridge.buzzerPending() && t.bridge.promptToneEnabled());
    t.feedback();t.tick(3000);assert(t.bridge.pending()); // C0 does not report the buzzer switch.
    t.bridge.updateBuzzer(false,t.now);t.tick();
    assert(t.last()=="result 5 1 0" && !t.bridge.pending() && !t.bridge.promptToneEnabled());
  }
  {
    Bench t;t.bridge.updateBuzzer(false,t.now);t.rx("down get_properties 5 1");
    assert(t.last()=="result 5 1 0 false");
  }
  {
    Bench t;t.bridge.updateBuzzer(false,t.now);t.rx("down set_properties 5 1 true");
    assert(requestedBuzzer && t.bridge.pending());
    t.bridge.updateBuzzer(true,t.now);t.tick();assert(t.last()=="result 5 1 0" && t.bridge.promptToneEnabled());
  }
  {
    Bench t;const auto old=t.now;t.rx("down set_properties 5 1 false");
    t.bridge.updateBuzzer(false,old);t.tick();assert(t.bridge.pending());
    t.tick(8001);assert(t.last()=="result 5 1 -4004");
  }
  {
    Bench t;acceptBuzzer=false;t.rx("down set_properties 5 1 false");acceptBuzzer=true;
    assert(!t.bridge.pending() && t.last()=="result 5 1 -4004");
  }
  {
    Bench t;t.rx("down get_properties 2 9");assert(t.last()=="result 2 9 0 false");
  }
  {
    Bench t;const auto before=auxWrites;
    t.rx("down set_properties 2 9 true");
    assert(auxWrites==before+1 && requestedState.auxHeat && !requestedState.power);
    assert(requestedState.mode==4 && requestedState.target==27.5f && t.bridge.pending());
    t.actual.auxHeat=true;t.feedback();t.tick();assert(t.last()=="result 2 9 0");
  }
  {
    Bench t;t.actual.auxHeat=true;t.feedback();t.bridge.setSimulationEnabled(true);
    const auto before=auxWrites;t.rx("down set_properties 2 9 false 2 4 24");
    assert(auxWrites==before+1 && !requestedState.auxHeat && requestedState.target==24);
    t.actual.auxHeat=false;t.actual.target=24;t.feedback();t.tick();
    assert(t.last()=="result 2 9 0 2 4 0");
  }
  {
    Bench t;t.rx("down set_properties 2 9 true");t.feedback();t.tick(2201);
    assert(!t.bridge.pending() && t.last()=="result 2 9 -4004");
  }
  {
    Bench t;t.bridge.setSimulationEnabled(true);t.rx("down get_properties 9 1");
    assert(t.last()=="result 9 1 -4004");
  }
  {
    Bench t;t.bridge.setSimulationEnabled(true);const auto cleanBefore=cleanWrites,stateBefore=stateWrites;
    t.rx("down set_properties 9 1 \"1\"");
    assert(cleanWrites==cleanBefore+1 && requestedClean && stateWrites==stateBefore && t.bridge.selfCleanPending());
    t.feedback();t.tick(3000);assert(t.bridge.pending()); // C0 alone never confirms self-clean.
    t.bridge.updateSelfClean(true,t.now);t.tick();assert(t.last()=="result 9 1 0" && !t.bridge.pending());
  }
  {
    Bench t;t.bridge.updateSelfClean(true,t.now);
    t.rx("down set_properties 9 1 \"0\"");assert(!requestedClean && t.bridge.pending());
    t.bridge.updateSelfClean(false,t.now);t.tick();assert(t.last()=="result 9 1 0");
  }
  {
    Bench t;t.bridge.updateSelfClean(true,t.now);t.rx("down get_properties 9 1");
    assert(t.last()=="result 9 1 0 \"1\"");
  }
  {
    Bench t;const auto old=t.now;t.rx("down set_properties 9 1 \"1\"");
    t.bridge.updateSelfClean(true,old);t.tick();assert(t.bridge.pending());
    t.tick(8001);assert(t.last()=="result 9 1 -4004");
  }
  {
    Bench t;t.rx("down set_properties 9 1 \"1,0\"");assert(t.last()=="result 9 1 -4002");
  }
  {
    Bench t;acceptAux=false;t.rx("down set_properties 2 9 true");acceptAux=true;
    assert(!t.bridge.pending() && t.last()=="result 2 9 -4004");
  }
  {
    Bench t;acceptClean=false;t.rx("down set_properties 9 1 \"1\"");acceptClean=true;
    assert(!t.bridge.pending() && t.last()=="result 9 1 -4004");
  }
  {
    MiotC20Simulator sim;size_t cursor=0;String output;
    while(cursor<MiotC20Simulator::count){cursor=sim.report(cursor,output,false,true);
      const std::string text=output.c_str();assert(text.find(" 2 9 ")==std::string::npos && text.find(" 9 1 ")==std::string::npos && text.find(" 5 1 ")==std::string::npos && text.find(" 2 10 ")==std::string::npos && text.find(" 9 2 ")==std::string::npos);
    }
  }
  {
    std::vector<uint8_t> frame(35,0);frame[9]=3;frame[10]=0xC0;frame[12]=0x88;frame[13]=45;frame[19]=0x08;finishFrame(frame);
    MideaAcSnapshot decoded;assert(decodeMideaState(frame.data(),frame.size(),123,decoded) && decoded.auxHeat);
    frame[19]=0;finishFrame(frame);assert(decodeMideaState(frame.data(),frame.size(),124,decoded) && !decoded.auxHeat);
    frame[19]=0xAC;MideaCommandFlags::applyAuxHeat(frame.data(),false);assert(frame[19]==0xA4);
    MideaCommandFlags::applyAuxHeat(frame.data(),true);assert(frame[19]==0xAC);
  }
  for(bool reserved:{false,true})for(bool active:{false,true}){
    auto frame=cleanReply(active,reserved);bool decoded=!active;
    assert(decodeMideaSelfClean(frame.data(),frame.size(),decoded) && decoded==active);
    frame.back()^=1;assert(!decodeMideaSelfClean(frame.data(),frame.size(),decoded));
    auto buzzer=cleanReply(active,reserved);buzzer[12]=0x2C;buzzer[13]=0x02;finishFrame(buzzer);
    assert(decodeMideaBuzzer(buzzer.data(),buzzer.size(),decoded) && decoded==active);
    assert(!decodeMideaSelfClean(buzzer.data(),buzzer.size(),decoded));
    buzzer.back()^=1;assert(!decodeMideaBuzzer(buzzer.data(),buzzer.size(),decoded));
  }
  {
    bool active=false;auto capability=cleanReply(true,false,5,0xB5);
    assert(!decodeMideaSelfClean(capability.data(),capability.size(),active));
    auto echo=cleanReply(true,false,2,0xB0);assert(!decodeMideaSelfClean(echo.data(),echo.size(),active));
    capability[12]=0x2C;capability[13]=2;finishFrame(capability);
    assert(!decodeMideaBuzzer(capability.data(),capability.size(),active));
    echo[12]=0x2C;echo[13]=2;finishFrame(echo);assert(!decodeMideaBuzzer(echo.data(),echo.size(),active));
    auto malformed=cleanReply(true);malformed[14]=20;finishFrame(malformed);
    assert(!decodeMideaSelfClean(malformed.data(),malformed.size(),active));
    auto live=capability;live[12]=0x7E;live[13]=0;live[14]=9;live.insert(live.begin()+16,8,0);live[23]=4;finishFrame(live);
    assert(decodeMideaSelfClean(live.data(),live.size(),active) && active);
  }
  {
    uint8_t set[19],query[19];const auto n=buildMideaSelfCleanFrame(set,sizeof(set),false,true,7);
    assert(n==19 && set[9]==2 && set[10]==0xB0 && set[12]==0x39 && set[13]==0 && set[14]==1 && set[15]==1);
    assert(mideaStateCrc8(set+10,n-12)==set[n-2]);uint8_t sum=0;for(size_t i=1;i<n;++i)sum+=set[i];assert(sum==0);
    assert(buildMideaSelfCleanFrame(query,sizeof(query),true,false,8)==17 && query[9]==3 && query[10]==0xB1 && query[12]==0x39);
    assert(buildMideaSelfCleanFrame(set,3,false,true,1)==0);
    assert(buildMideaBuzzerFrame(set,sizeof(set),false,false,7)==19 && set[12]==0x2C && set[13]==0x02 && set[15]==0);
    assert(buildMideaBuzzerFrame(query,sizeof(query),true,false,8)==17 && query[9]==3 && query[12]==0x2C && query[13]==0x02);
  }
  std::cout<<"PASS: real aux/clean/buzzer mapping, feedback-only confirmation and independent dryer/examine simulation\n";
}
