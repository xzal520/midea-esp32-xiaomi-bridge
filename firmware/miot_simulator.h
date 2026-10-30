#pragma once
#include <Arduino.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

enum class MiotSimulatedType : uint8_t { Bool, Integer, Float, Text };
struct MiotSimulatedProperty {
  uint8_t service, property;
  MiotSimulatedType type;
  bool readable, writable, notify, ranged;
  double minimum, maximum;
  const char *enums, *initial;
};
#include "miot_simulated_properties.h"

// Only C20 properties without a verified physical Midea mapping live here.
// Values are RAM-only; enabling simulation never manufactures Midea feedback.
class MiotC20Simulator {
 public:
  static constexpr size_t count = sizeof(MIOT_SIMULATED_PROPERTIES) / sizeof(MIOT_SIMULATED_PROPERTIES[0]);
  bool read(uint8_t service, uint8_t property, String &out) const {
    const int index = find(service, property);
    if (index < 0 || !MIOT_SIMULATED_PROPERTIES[index].readable) return false;
    out = value(index); return true;
  }
  // Return false only when the ID is not part of the simulated property set.
  bool set(uint8_t service, uint8_t property, const char *wire, int &code) {
    const int index = find(service, property);
    if (index < 0) return false;
    const auto &p = MIOT_SIMULATED_PROPERTIES[index];
    code = -4003;
    if (!p.writable) return true;
    code = -4002;
    String canonical;
    if (p.type == MiotSimulatedType::Bool) {
      if (!strcmp(wire,"true") || !strcmp(wire,"1")) canonical="true";
      else if (!strcmp(wire,"false") || !strcmp(wire,"0")) canonical="false";
      else return true;
    } else if (p.type == MiotSimulatedType::Text) {
      if (!validText(wire)) return true;
      canonical=wire;
    } else {
      if (!*wire) return true;
      char *end=nullptr;
      const double n=strtod(wire,&end);
      if (!end || *end || !isfinite(n) || (p.ranged && (n<p.minimum || n>p.maximum))) return true;
      if (p.type==MiotSimulatedType::Integer && (floor(n)!=n || n < -2147483647.0 || n > 2147483647.0)) return true;
      if (*p.enums) {
        bool found=false;
        for(const char *e=p.enums; *e;) {
          char *next=nullptr; const long candidate=strtol(e,&next,10);
          if (double(candidate)==n) found=true;
          if (next==e || !*next) break;
          e=next+1;
        }
        if (!found) return true;
      }
      canonical=p.type==MiotSimulatedType::Integer?String(long(n)):String(n,2);
    }
    values_[index]=canonical;
    // The filter-reset flag is a writable command; its simulated side effect
    // is resetting the simulated filter lifetime, with no physical action.
    if (service==10 && property==7 && canonical=="true") {
      const int filter=find(15,1); if(filter>=0) values_[filter]="100";
    }
    code=0; return true;
  }
  // Keep each notification small enough for the bridge buffer and the MIoT
  // parser. The bridge tokenizer has 64 slots; command + 20 triples uses 61.
  size_t report(size_t start, String &out, bool realRoomKnown, bool physicalC20Mapped) const {
    out="properties_changed";
    out.reserve(350);
    size_t i=start;
    size_t emitted=0;
    for(;i<count;++i) {
      const auto &p=MIOT_SIMULATED_PROPERTIES[i];
      if(!p.notify || (realRoomKnown && p.service==4 && p.property==7) ||
         (physicalC20Mapped && ((p.service==10 && p.property==1) || (p.service==6 && p.property==2) ||
          (p.service==2 && (p.property==9 || p.property==10)) || (p.service==9 && (p.property==1 || p.property==2)) ||
          (p.service==5 && p.property==1)))) continue;
      const String item=" "+String(p.service)+" "+String(p.property)+" "+value(i);
      if(emitted>=20 || out.length()+item.length()>350) break;
      out+=item;
      ++emitted;
    }
    return i;
  }
 private:
  String values_[count];
  String value(size_t index) const {
    return values_[index].length()?values_[index]:String(MIOT_SIMULATED_PROPERTIES[index].initial);
  }
  static int find(uint8_t service, uint8_t property) {
    for(size_t i=0;i<count;++i) if(MIOT_SIMULATED_PROPERTIES[i].service==service && MIOT_SIMULATED_PROPERTIES[i].property==property) return int(i);
    return -1;
  }
  static bool validText(const char *wire) {
    const size_t length=strlen(wire);
    if(length<2 || length>96 || wire[0]!='"' || wire[length-1]!='"') return false;
    for(size_t i=1;i+1<length;++i) {
      const unsigned char c=wire[i];
      if(c<32 || c=='"') return false;
      if(c=='\\') {
        if(++i>=length-1) return false;
        if(wire[i]=='u') {
          for(uint8_t k=0;k<4;++k) {
            if(++i>=length-1) return false;
            const char h=wire[i]; if(!((h>='0'&&h<='9')||(h>='a'&&h<='f')||(h>='A'&&h<='F'))) return false;
          }
        } else if(!strchr("\"\\/bfnrt",wire[i])) return false;
      }
    }
    return true;
  }
};
