#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// Decide readiness without consuming bytes. Non-upload requests are handed to
// Arduino's synchronous parser only after their complete header/body arrived.
namespace HttpRequestGate {
enum class State { Wait, Ready, Reject };
struct Result {
  State state;int status;bool upload;size_t headerBytes,bodyBytes;
  Result(State s=State::Wait,int code=200,bool streaming=false,size_t header=0,size_t body=0):
      state(s),status(code),upload(streaming),headerBytes(header),bodyBytes(body){}
};
static constexpr size_t HEADER_LIMIT=4096, BODY_LIMIT=4096;
inline bool equals(const char *s,size_t size,const char *word) {
  if(size!=strlen(word))return false;
  for(size_t i=0;i<size;++i){char c=s[i];if(c>='A'&&c<='Z')c+=32;if(c!=word[i])return false;}
  return true;
}
inline Result inspect(const char *data,size_t size,size_t available) {
  size_t end=0;
  for(size_t i=0;i+3<size;++i)if(!memcmp(data+i,"\r\n\r\n",4)){end=i+4;break;}
  if(!end)return {size>=HEADER_LIMIT?State::Reject:State::Wait,431,false};
  size_t pos=0;while(pos+1<end && memcmp(data+pos,"\r\n",2))++pos;
  pos+=2;
  size_t body=0;bool hasLength=false,multipart=false;
  while(pos+2<end) {
    size_t stop=pos;while(stop+1<end && memcmp(data+stop,"\r\n",2))++stop;
    size_t colon=pos;while(colon<stop && data[colon]!=':')++colon;
    if(colon==stop)return {State::Reject,400,false};
    size_t value=colon+1;while(value<stop && (data[value]==' ' || data[value]=='\t'))++value;
    size_t valueEnd=stop;while(valueEnd>value && (data[valueEnd-1]==' ' || data[valueEnd-1]=='\t'))--valueEnd;
    if(equals(data+pos,colon-pos,"content-length")) {
      if(hasLength || value==valueEnd)return {State::Reject,400,false};
      hasLength=true;
      for(size_t i=value;i<valueEnd;++i) {
        if(data[i]<'0'||data[i]>'9'||body>(UINT32_MAX-uint32_t(data[i]-'0'))/10)
          return {State::Reject,400,false};
        body=body*10+size_t(data[i]-'0');
      }
    } else if(equals(data+pos,colon-pos,"content-type")) {
      constexpr size_t n=sizeof("multipart/form-data")-1;
      multipart=valueEnd-value>=n && equals(data+value,n,"multipart/form-data");
    } else if(equals(data+pos,colon-pos,"transfer-encoding")) {
      return {State::Reject,400,false}; // The existing Arduino parser has no chunked request support.
    }
    pos=stop+2;
  }
  if(!multipart && body>BODY_LIMIT)return {State::Reject,413,false};
  return {multipart || available>=end+body?State::Ready:State::Wait,200,multipart,end,body};
}
}
