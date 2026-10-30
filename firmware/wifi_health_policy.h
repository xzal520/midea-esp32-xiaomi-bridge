#pragma once
#include <stdint.h>

// Independent of the radio/task APIs so loss bursts, cooldown and clock wrap
// can be checked without real network outages.
struct WifiHealthPolicy {
  uint32_t connectedAt=0,lastGoodAt=0,lastRecoveryAt=0,recoveries=0;
  uint32_t webBaseline=0,cooldownMs=300000,healthySince=0;
  uint8_t badRounds=0,backoff=0;
  bool everGood=false,hasResult=false,healthyRun=false;

  void connected(uint32_t now,uint32_t writes,bool newGateway) {
    connectedAt=now;webBaseline=writes;badRounds=0;hasResult=false;healthyRun=false;
    if(newGateway)everGood=false;
  }
  void observe(uint32_t now,uint32_t sent,uint32_t received) {
    if(sent<4)return; // An interrupted probe is not a failed link.
    hasResult=true;
    if(received>=3) {
      everGood=true;lastGoodAt=now;badRounds=0;
      if(!healthyRun){healthyRun=true;healthySince=now;}
      if(uint32_t(now-healthySince)>=600000){backoff=0;cooldownMs=300000;}
    } else {
      healthyRun=false;if(badRounds<255)++badRounds;
    }
  }
  bool shouldRecover(uint32_t now,uint32_t writes,bool paused) const {
    // Routers that never answer ICMP must not cause reconnection loops unless
    // failed HTTP writes independently corroborate the broken link.
    return !paused && badRounds>=3 && uint32_t(now-connectedAt)>=90000 &&
      (everGood || uint32_t(writes-webBaseline)>=3) &&
      (!recoveries || uint32_t(now-lastRecoveryAt)>=cooldownMs);
  }
  void recovered(uint32_t now,uint32_t writes) {
    ++recoveries;lastRecoveryAt=now;webBaseline=writes;badRounds=0;healthyRun=false;
    cooldownMs=300000u<<backoff;
    if(cooldownMs>1800000)cooldownMs=1800000;
    if(backoff<3)++backoff;
  }
};
