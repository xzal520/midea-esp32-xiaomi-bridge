#include <cassert>
#include <cstring>
#include <string>
#include <iostream>
#include "wifi_health_policy.h"
#include "http_response_pump.h"

int main(){
  WifiHealthPolicy p;
  p.connected(0,0,true);
  p.observe(30000,4,4);
  p.observe(60000,4,0);
  assert(!p.shouldRecover(90000,0,false));
  p.observe(90000,4,4);
  assert(p.badRounds==0);
  p.observe(120000,4,1);p.observe(150000,4,1);p.observe(180000,4,1);
  assert(p.shouldRecover(180000,0,false));
  assert(!p.shouldRecover(180000,0,true)); // OTA pause.
  p.recovered(180000,0);p.connected(181000,0,false);
  p.observe(220000,4,0);p.observe(250000,4,0);p.observe(280000,4,0);
  assert(!p.shouldRecover(300000,0,false));
  assert(p.shouldRecover(480000,0,false));
  p.recovered(480000,0);assert(p.cooldownMs==600000);
  p.observe(500000,4,4);p.observe(1100000,4,4);assert(p.backoff==0 && p.cooldownMs==300000);

  WifiHealthPolicy blocked;
  blocked.connected(0,0,true);
  blocked.observe(30000,4,0);blocked.observe(60000,4,0);blocked.observe(90000,4,0);
  assert(!blocked.shouldRecover(1000000,0,false));
  assert(blocked.shouldRecover(1000000,3,false));
  blocked.connected(1100000,3,true);assert(!blocked.everGood);

  WifiHealthPolicy wrap;
  const uint32_t start=0xffff0000u;
  wrap.connected(start,0,true);wrap.observe(start+1000u,4,4);
  wrap.observe(start+30000u,4,0);wrap.observe(start+60000u,4,0);wrap.observe(start+91000u,4,0);
  assert(wrap.shouldRecover(start+91000u,0,false));
  wrap.observe(start+92000u,2,0);assert(wrap.badRounds==3);

  using Result=HttpResponsePump::Result;
  const std::string body(5000,'x');
  HttpResponsePump pump;std::string received;
  pump.begin(body.data(),body.size(),0);
  auto blockedWrite=[](const char*,size_t){errno=EAGAIN;return -1;};
  assert(pump.tick(0,blockedWrite)==Result::Pending);
  assert(pump.tick(3000,blockedWrite)==Result::Pending); // Old 2-second cutoff.
  assert(pump.tick(9000,blockedWrite)==Result::Pending);
  uint32_t now=9001;Result state=Result::Pending;
  while(state==Result::Pending){
    state=pump.tick(now++,[&](const char *data,size_t size){
      const size_t count=size>123?123:size;received.append(data,count);return int(count);
    });
  }
  assert(state==Result::Complete && received==body && !pump.active());
  pump.begin(body.data(),body.size(),0);
  assert(pump.tick(9999,blockedWrite)==Result::Pending);
  assert(pump.tick(10000,blockedWrite)==Result::Timeout);
  pump.begin(body.data(),body.size(),start);
  assert(pump.tick(start+10000u,blockedWrite)==Result::Timeout);
  pump.begin(body.data(),body.size(),0);
  assert(pump.tick(1,[](const char*,size_t){errno=ENOMEM;return -1;})==Result::Pending);
  assert(pump.tick(2,[](const char*,size_t){errno=ECONNRESET;return -1;})==Result::Error);
  pump.begin(body.data(),body.size(),0);
  for(uint32_t t=9000;t<60000;t+=9000)
    assert(pump.tick(t,[](const char*,size_t){return 1;})==Result::Pending);
  assert(pump.tick(60000,blockedWrite)==Result::Timeout);
  std::cout<<"PASS: network recovery thresholds, ICMP-blocked gateways, cooldown, OTA pause, clock wrap and stalled/partial HTTP writes\n";
}
