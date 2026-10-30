#pragma once
#include <WiFi.h>
#include <WebServer.h>
#include <lwip/sockets.h>
#include <errno.h>
#include <new>
#include "http_request_gate.h"
#include "http_response_pump.h"

// Replay bytes collected across TCP segments through the existing parser's
// virtual Stream interface; WiFiClient copies share the underlying RX buffer.
class HttpPrefixClient : public WiFiClient {
 public:
  HttpPrefixClient(const WiFiClient &peer,const String &prefix,void (*hook)()):
      WiFiClient(peer),data_(prefix.c_str()),size_(prefix.length()),service_(hook){}
  int available() override {pump();return int(size_-pos_)+WiFiClient::available();}
  int read() override {pump();return pos_<size_?uint8_t(data_[pos_++]):WiFiClient::read();}
  int read(uint8_t *out,size_t length) override {
    pump();const size_t copied=length<size_-pos_?length:size_-pos_;
    if(copied){memcpy(out,data_+pos_,copied);pos_+=copied;}
    if(copied==length)return int(copied);
    const int extra=WiFiClient::read(out+copied,length-copied);
    return extra>0?int(copied)+extra:copied?int(copied):extra;
  }
  int peek() override {return pos_<size_?uint8_t(data_[pos_]):WiFiClient::peek();}
  uint8_t connected() override {pump();return pos_<size_?1:WiFiClient::connected();}
  void flush() override {pos_=size_;WiFiClient::flush();}
 private:
  const char *data_;size_t size_,pos_=0;void (*service_)();uint32_t servicedAt_=0;
  void pump(){if(service_ && millis()-servicedAt_>=5){servicedAt_=millis();service_();}}
};

// Keep the existing routes/OTA API, but do not enter its blocking parser for a
// speculative connection or a partial ordinary request. All clients remain on
// the Arduino loop; the UART hook is cooperative, not another concurrent task.
class ResponsiveWebServer : public WebServer {
 public:
  explicit ResponsiveWebServer(int port):WebServer(port){}
  void setServiceHook(void (*hook)()){service_=hook;}
  uint32_t inputTimeouts() const{return inputTimeouts_;}
  uint32_t writeTimeouts() const{return writeTimeouts_;}
  void sendFlash(int code,const char *type,const char *content,size_t length) {
    setContentLength(length);
    send(code,type,"");
    if(_currentClient.fd()>=0)response_.begin(content,length,millis());
  }
  void close() override {
    response_.reset();
    _currentClient.stop();_currentUpload.reset();_currentRaw.reset();
    for(size_t i=0;i<SLOTS;++i){waiting_[i].stop();prefix_[i].reset();}
    WebServer::close();
  }
  void handleClient() override {
    const uint32_t now=millis();
    if(response_.active()) {
      const auto result=response_.tick(now,[this](const char *data,size_t size){
        return ::send(_currentClient.fd(),data,size,MSG_DONTWAIT);
      });
      if(result==HttpResponsePump::Result::Pending){if(_nullDelay)delay(1);return;}
      if(result==HttpResponsePump::Result::Timeout)++writeTimeouts_;
      _currentClient.stop();_currentStatus=HC_NONE;
      return;
    }
    int freeSlot=-1,idleSlot=-1;
    for(size_t i=0;i<SLOTS;++i) {
      // A complete request queued behind a slow response must still be read.
      if(waiting_[i] && (!waiting_[i].connected() ||
          (now-acceptedAt_[i]>=3500 && !waiting_[i].available() && headers_[i].state==HttpRequestGate::State::Wait))) {
        if(waiting_[i].connected())++inputTimeouts_;
        waiting_[i].stop();prefix_[i].reset();
      }
      if(!waiting_[i]){freeSlot=int(i);prefix_[i].reset();}
      else if(!prefix_[i] && !waiting_[i].available() && (idleSlot<0 || acceptedAt_[i]<acceptedAt_[idleSlot]))idleSlot=int(i);
    }
    // A browser can preconnect several sockets. Evict an empty one rather than
    // leave a real pending request behind it in the TCP accept queue.
    if(freeSlot<0 && idleSlot>=0 && _server.hasClient()) {
      waiting_[idleSlot].stop();prefix_[idleSlot].reset();freeSlot=idleSlot;
    }
    if(freeSlot>=0) {
      WiFiClient incoming=_server.available();
      if(incoming){incoming.setNoDelay(true);waiting_[freeSlot]=incoming;acceptedAt_[freeSlot]=now;
        prefix_[freeSlot].reset();headers_[freeSlot]={HttpRequestGate::State::Wait,200,false};}
    }
    for(size_t n=0;n<SLOTS;++n) {
      const size_t slot=(nextSlot_+n)%SLOTS;
      WiFiClient &candidate=waiting_[slot];
      if(!candidate)continue;
      auto &readiness=headers_[slot];
      if(!prefix_[slot]) {
        if(!candidate.available())continue;
        prefix_[slot].reset(new(std::nothrow) String());
        if(!prefix_[slot])readiness={HttpRequestGate::State::Reject,503,false};
      }
      if(!readiness.headerBytes && readiness.state!=HttpRequestGate::State::Reject) {
        const size_t room=HttpRequestGate::HEADER_LIMIT-prefix_[slot]->length();
        const int available=candidate.available();
        if(available<=0)continue;
        const size_t take=room<sizeof(chunk_)-1?room:sizeof(chunk_)-1;
        const int received=candidate.read(reinterpret_cast<uint8_t*>(chunk_),size_t(available)<take?size_t(available):take);
        if(received<=0)continue;
        chunk_[received]=0; // Arduino String::concat copies length+1 bytes.
        if(!prefix_[slot]->concat(chunk_,unsigned(received))) {
          readiness={HttpRequestGate::State::Reject,503,false};
        } else readiness=HttpRequestGate::inspect(prefix_[slot]->c_str(),prefix_[slot]->length(),prefix_[slot]->length()+candidate.available());
      }
      if(readiness.headerBytes && readiness.state==HttpRequestGate::State::Wait &&
          prefix_[slot]->length()+candidate.available()>=readiness.headerBytes+readiness.bodyBytes)
        readiness.state=HttpRequestGate::State::Ready;
      if(readiness.state==HttpRequestGate::State::Wait)continue;
      _currentClient=candidate;candidate=WiFiClient();nextSlot_=(slot+1)%SLOTS;
      _currentStatus=HC_WAIT_READ;_statusChange=now;
      if(readiness.state==HttpRequestGate::State::Reject) {
        const char *response=readiness.status==503?"HTTP/1.1 503 Service Unavailable\r\n":
          readiness.status==413?"HTTP/1.1 413 Payload Too Large\r\n":
          readiness.status==431?"HTTP/1.1 431 Request Header Fields Too Large\r\n":"HTTP/1.1 400 Bad Request\r\n";
        writeCooperatively(response,strlen(response));
        constexpr char finish[]="Connection: close\r\nContent-Length: 0\r\n\r\n";
        writeCooperatively(finish,sizeof(finish)-1);
      } else {
        // Normal requests are already buffered. Multipart remains streamed for
        // OTA and keeps the existing upload parser and integrity checks.
        HttpPrefixClient buffered(_currentClient,*prefix_[slot],service_);
        // Multipart OTA can pause briefly while Wi-Fi retransmits or the flash
        // writer yields. A one-second per-byte timeout aborted those uploads.
        static_cast<Stream&>(buffered).setTimeout(readiness.upload?10000:100);
        if(_parseRequest(buffered)) {
          _contentLength=CONTENT_LENGTH_NOT_SET;
          // The base class relies on this per-request reset; skipping it would
          // append every request's response headers to the previous response.
          _responseCode=0;
          _clearResponseHeaders();
          _handleRequest();
        }
      }
      _currentUpload.reset();_currentRaw.reset();prefix_[slot].reset();
      if(!response_.active()){_currentClient.stop();_currentStatus=HC_NONE;}
      return;
    }
    if(_nullDelay)delay(1);
  }
 protected:
  size_t _currentClientWrite(const char *data,size_t length) override{return writeCooperatively(data,length);}
  size_t _currentClientWrite_P(PGM_P data,size_t length) override{return writeCooperatively(data,length);}
 private:
  static constexpr size_t SLOTS=4;
  WiFiClient waiting_[SLOTS];uint32_t acceptedAt_[SLOTS]={};size_t nextSlot_=0;
  std::unique_ptr<String> prefix_[SLOTS];HttpRequestGate::Result headers_[SLOTS];char chunk_[513];
  void (*service_)()=nullptr;
  uint32_t inputTimeouts_=0,writeTimeouts_=0;
  HttpResponsePump response_;
  size_t writeCooperatively(const char *data,size_t length) {
    size_t sent=0;uint32_t progressAt=millis();
    while(sent<length && _currentClient.fd()>=0) {
      if(service_)service_();
      const size_t chunk=length-sent>1024?1024:length-sent;
      const int result=::send(_currentClient.fd(),data+sent,chunk,MSG_DONTWAIT);
      if(result>0){sent+=size_t(result);progressAt=millis();continue;}
      if(result==0 || (errno!=EAGAIN && errno!=EWOULDBLOCK && errno!=EINTR)){_currentClient.stop();break;}
      if(millis()-progressAt>=2000){++writeTimeouts_;_currentClient.stop();break;}
      delay(1); // The next iteration services UART even when TCP cannot write.
    }
    return sent;
  }
};
