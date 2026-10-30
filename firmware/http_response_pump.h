#pragma once
#include <stdint.h>
#include <stddef.h>
#include <errno.h>

// One nonblocking write per Arduino loop; backing data must remain alive.
class HttpResponsePump {
 public:
  enum class Result { Idle, Pending, Complete, Timeout, Error };
  bool active() const{return data_!=nullptr;}
  void reset(){data_=nullptr;length_=sent_=0;}
  void begin(const char *data,size_t length,uint32_t now){
    data_=data;length_=length;sent_=0;startedAt_=progressAt_=now;
  }
  template<class Writer> Result tick(uint32_t now,Writer write) {
    if(!active())return Result::Idle;
    if(uint32_t(now-progressAt_)>=10000 || uint32_t(now-startedAt_)>=60000){reset();return Result::Timeout;}
    const size_t remaining=length_-sent_;
    if(!remaining){reset();return Result::Complete;}
    const size_t count=remaining>1024?1024:remaining;
    const int result=write(data_+sent_,count);
    if(result>0){
      sent_+=size_t(result);progressAt_=now;
      if(sent_==length_){reset();return Result::Complete;}
      return Result::Pending;
    }
    if(result<0 && (errno==EAGAIN || errno==EWOULDBLOCK || errno==EINTR || errno==ENOMEM || errno==ENOBUFS))return Result::Pending;
    reset();return Result::Error;
  }
 private:
  const char *data_=nullptr;
  size_t length_=0,sent_=0;
  uint32_t startedAt_=0,progressAt_=0;
};
