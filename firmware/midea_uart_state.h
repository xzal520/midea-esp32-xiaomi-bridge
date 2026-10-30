#pragma once
#include "miot_bridge.h"

// Native relative ON/OFF timers, in minutes. C0 and 0x40 share bytes 14..16.
// Reference: reneklootwijk/midea-uart C0_Airco_parser / setstatus_cmd40.
struct MideaTimers {
  bool known=false, absolute=false, on=false, off=false;
  uint16_t onMinutes=0, offMinutes=0;
  uint32_t receivedAt=0;
  uint8_t wireOn=0x7F, wireOff=0x7F, wireMinutes=0;

  static uint16_t decodeMinutes(uint8_t value,uint8_t elapsed) {
    return uint16_t((value>>2)&31)*60 + ((value&3)+1)*15 - elapsed;
  }
  // Caller has already checked frame bounds, CRC and outer checksum.
  void captureFromValidatedC0(const uint8_t *frame,uint32_t now) {
    known=true; absolute=frame[11]&0x10; receivedAt=now;
    wireOn=frame[14];wireOff=frame[15];wireMinutes=frame[16];
    on=wireOn&0x80;off=wireOff&0x80;
    onMinutes=on?decodeMinutes(wireOn,wireMinutes>>4):0;
    offMinutes=off?decodeMinutes(wireOff,wireMinutes&15):0;
  }
  uint16_t remaining(bool turnOn,uint32_t now) const {
    const uint16_t minutes=turnOn?onMinutes:offMinutes;
    const uint32_t elapsed=absolute?0:uint32_t(now-receivedAt)/60000;
    return elapsed<minutes?minutes-elapsed:0;
  }
  static void encodeMinutes(uint16_t minutes,uint8_t &value,uint8_t &elapsed) {
    if(!minutes){value=0x7F;elapsed=0;return;}
    // Encode as the next quarter-hour minus 0..14 elapsed minutes.
    const uint16_t quarters=(minutes+14)/15;
    value=uint8_t(0x80|(((quarters-1)/4)<<2)|((quarters-1)%4));
    elapsed=uint8_t(quarters*15-minutes);
  }
  void applyTo(uint8_t *frame,bool update) const {
    if(!known)return;
    frame[11]=uint8_t((frame[11]&~0x10)|(absolute?0x10:0));
    // Bit 7 is the timer-write flag, not part of the fan speed.
    frame[13]=uint8_t((frame[13]&0x7F)|(update?0x80:0));
    frame[14]=wireOn;frame[15]=wireOff;frame[16]=wireMinutes;
  }
  void setRelative(uint16_t onDelay,uint16_t offDelay,uint32_t now) {
    known=true;absolute=false;receivedAt=now;
    onMinutes=onDelay;offMinutes=offDelay;on=onDelay>0;off=offDelay>0;
    uint8_t onElapsed,offElapsed;
    encodeMinutes(onDelay,wireOn,onElapsed);encodeMinutes(offDelay,wireOff,offElapsed);
    wireMinutes=uint8_t((onElapsed<<4)|offElapsed);
  }
};

// C0 status flags and 0x40 set flags do not all share the same bit positions.
// Retain the independently mapped features when a full-state command changes
// only power/mode/temperature/fan. Do not copy unknown status bytes verbatim.
struct MideaCommandFlags {
  uint8_t byte8=0, byte9=0, byte10=0, byte17=0, byte21=0, byte22=0;
  void captureFromValidatedC0(const uint8_t *frame,size_t length) {
    byte8=frame[18]&0x08; // power saving
    byte9=((frame[18]&0x40)?0x01:0) | (frame[19]&0x2C); // smart eye, dry, auxiliary heat, anion
    byte10=frame[20]&0x04; // Fahrenheit units; native sleep is passed separately
    byte17=(frame[19]&0x02)?0x40:0; // natural wind
    byte21=length>=34?(frame[31]&0x80):0; // frost protection
    byte22=length>=35?(frame[32]&0x01):0; // comfort
  }
  void applyTo(uint8_t *frame) const {
    frame[18]|=byte8;frame[19]|=byte9;frame[20]|=byte10;
    frame[27]|=byte17;frame[31]|=byte21;frame[32]|=byte22;
  }
  static void applyAuxHeat(uint8_t *frame,bool enabled) {
    frame[19]=uint8_t((frame[19]&~0x08)|(enabled?0x08:0));
  }
};

inline uint8_t mideaStateCrc8(const uint8_t *data, size_t length) {
  uint8_t crc=0;
  for(size_t i=0;i<length;++i){crc^=data[i];for(uint8_t b=0;b<8;++b)crc=(crc&1)?uint8_t((crc>>1)^0x8C):uint8_t(crc>>1);}
  return crc;
}

// Self-clean is property 0x0039, not the C0 dry/anion bits. B0 writes use
// tag-low, tag-high, length, value; B1 queries contain only the two-byte tag.
inline size_t buildMideaSwitchPropertyFrame(uint8_t *frame,size_t capacity,uint16_t tag,bool query,bool enabled,uint8_t sequence) {
  const size_t length=query?17:19;if(capacity<length)return 0;
  memset(frame,0,length);frame[0]=0xAA;frame[1]=uint8_t(length-1);frame[2]=0xAC;
  frame[8]=0x02;frame[9]=query?0x03:0x02;frame[10]=query?0xB1:0xB0;
  frame[11]=1;frame[12]=uint8_t(tag);frame[13]=uint8_t(tag>>8);
  if(!query){frame[14]=1;frame[15]=enabled?1:0;}
  frame[length-3]=sequence;frame[length-2]=mideaStateCrc8(frame+10,length-12);
  uint8_t sum=0;for(size_t i=1;i<length-1;++i)sum+=frame[i];frame[length-1]=uint8_t(0-sum);
  return length;
}
inline size_t buildMideaSelfCleanFrame(uint8_t *frame,size_t capacity,bool query,bool enabled,uint8_t sequence) {
  return buildMideaSwitchPropertyFrame(frame,capacity,0x0039,query,enabled,sequence);
}
inline size_t buildMideaBuzzerFrame(uint8_t *frame,size_t capacity,bool query,bool enabled,uint8_t sequence) {
  return buildMideaSwitchPropertyFrame(frame,capacity,0x022C,query,enabled,sequence);
}

inline bool parseMideaSwitchProperties(const uint8_t *frame,size_t length,bool reserved,uint16_t property,bool &enabled) {
  const size_t end=length-2;size_t cursor=12;bool found=false,state=false,payloadFound=false,payloadState=false;
  for(uint8_t i=0;i<frame[11];++i){
    const size_t header=reserved?4:3;if(cursor+header>end)return false;
    const uint16_t tag=uint16_t(frame[cursor])|(uint16_t(frame[cursor+1])<<8);
    if(reserved && frame[cursor+2]!=0)return false;
    const size_t bytes=frame[cursor+header-1];cursor+=header;
    if(cursor+bytes>end)return false;
    // B5 0x0039 is a capability advertisement, never a running-state report.
    if(tag==property && frame[10]!=0xB5 && bytes){state=frame[cursor]>0;found=true;}
    // New-protocol live-state payload: byte 8 bit 2, including B5 notifies.
    if(property==0x0039 && tag==0x007E && bytes>8){payloadState=(frame[cursor+8]&0x04)!=0;payloadFound=true;}
    cursor+=bytes;
  }
  if(cursor>end || end-cursor>1 || (!found && !payloadFound))return false;
  enabled=payloadFound?payloadState:state;return true;
}

inline bool decodeMideaSwitchProperty(const uint8_t *frame,size_t length,uint16_t property,bool &enabled) {
  if(length<16 || frame[0]!=0xAA || size_t(frame[1])+1!=length || frame[2]!=0xAC)return false;
  uint8_t sum=0;for(size_t i=1;i<length;++i)sum+=frame[i];
  if(sum || mideaStateCrc8(frame+10,length-12)!=frame[length-2])return false;
  const bool query=frame[9]==3 && (frame[10]==0xB0 || frame[10]==0xB1);
  const bool notify=frame[9]==5 && frame[10]==0xB5;
  if(!query && !notify)return false; // A set echo alone does not confirm execution.
  if(frame[10]==0xB5)return parseMideaSwitchProperties(frame,length,false,property,enabled);
  return parseMideaSwitchProperties(frame,length,true,property,enabled) ||
    parseMideaSwitchProperties(frame,length,false,property,enabled);
}
inline bool decodeMideaSelfClean(const uint8_t *frame,size_t length,bool &enabled) {
  return decodeMideaSwitchProperty(frame,length,0x0039,enabled);
}
inline bool decodeMideaBuzzer(const uint8_t *frame,size_t length,bool &enabled) {
  return decodeMideaSwitchProperty(frame,length,0x022C,enabled);
}

// AA length AC ... type C0 ... body-CRC outer-checksum. Never decode a stray C0.
inline bool decodeMideaState(const uint8_t *frame,size_t length,uint32_t now,MideaAcSnapshot &state) {
  if(length<28 || frame[0]!=0xAA || size_t(frame[1])+1!=length || frame[2]!=0xAC ||
     (frame[9]!=2 && frame[9]!=3 && frame[9]!=4) || frame[10]!=0xC0) return false;
  uint8_t sum=0;for(size_t i=1;i<length;++i)sum+=frame[i];
  if(sum || mideaStateCrc8(frame+10,length-12)!=frame[length-2])return false;
  const uint8_t mode=(frame[12]>>5)&7;
  const float target=16+(frame[12]&15)+((frame[12]&16)?0.5f:0);
  if(mode<1 || mode>5 || target<16 || target>31.5f || (frame[13]&0x7F)>102)return false;
  MideaAcSnapshot next;
  next.known=true;next.receivedAt=now;next.power=frame[11]&1;next.mode=mode;
  next.target=target;next.fan=frame[13]&0x7F;next.swing=frame[17]&15;
  next.eco=frame[19]&0x10;next.sleep=frame[20]&1;next.auxHeat=frame[19]&0x08;
  // C0 payload byte 14 is full-frame byte 24 (payload starts at byte 11).
  // Bits 6:4 encode display brightness: 0-6 on, 7 off. Byte 25 carries the
  // room-temperature decimal nibble and must not be used for display state.
  next.displayOn=((frame[24]>>4)&0x07)!=7;
  const uint8_t raw=frame[21], decimal=frame[25]&15;
  if(raw!=0xFF && raw>=20 && raw<=180 && decimal<=9) {
    const int base=int(raw)-50;
    // The decimal nibble refines the integer temperature, not the half-degree bit.
    next.room=decimal?float(base/2)+(base>=0?1:-1)*decimal*0.1f:float(base)*0.5f;
    next.roomKnown=next.room>=-20 && next.room<=80;
  }
  state=next;return true;
}
