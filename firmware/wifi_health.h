#pragma once
#include <atomic>
#include <WiFi.h>
#include <esp_wifi.h>
#include <ping/ping_sock.h>
#include <cJSON.h>
#include "wifi_health_policy.h"

class WifiHealth {
 public:
  void begin(void (*log)(const char*,const char*)=nullptr) {
    log_=log;
    nextProbeAt_=millis()+30000;
    Network.onEvent([this](arduino_event_id_t event,arduino_event_info_t info){
      if(event==ARDUINO_EVENT_WIFI_STA_DISCONNECTED){
        disconnectReason_.store(info.wifi_sta_disconnected.reason);
        disconnects_.fetch_add(1);
      }
    });
  }
  // The ping SDK owns its worker. Only this main-loop method changes Wi-Fi;
  // UART and HomeSpan are never called from the worker/event callbacks.
  bool tick(uint32_t now,uint32_t writes,bool paused) {
    const bool online=WiFi.status()==WL_CONNECTED;
    const uint32_t gateway=online?uint32_t(WiFi.gatewayIP()):0;
    const uint32_t disconnects=disconnects_.load();
    if(disconnects!=seenDisconnects_){seenDisconnects_=disconnects;wasOnline_=false;}
    if(online && (!wasOnline_ || gateway!=observedGateway_)){
      policy_.connected(now,writes,gateway!=gateway_);
      wasOnline_=true;observedGateway_=gateway;++linkEpoch_;nextProbeAt_=now+30000;
      previousBssid_=bssid_;bssid_=WiFi.BSSIDstr();channel_=WiFi.channel();
      char message[200];snprintf(message,sizeof(message),"AP=%s previous=%s channel=%d RSSI=%d disconnects=%lu reason=%lu",
        bssid_.c_str(),previousBssid_.c_str(),channel_,WiFi.RSSI(),(unsigned long)disconnects,(unsigned long)disconnectReason_.load());
      Serial.printf("[NET] %s\n",message);if(log_)log_("NET",message);
    }
    if(finished_.exchange(false,std::memory_order_acquire)){
      running_=false;
      sent_=probeSent_.load();received_=probeReceived_.load();
      meanMs_=received_?rttTotal_.load()/received_:0;
      if(online && !paused && probeEpoch_==linkEpoch_ && gateway_==gateway)policy_.observe(now,sent_,received_);
      nextProbeAt_=now+30000;
    }
    if(!online){wasOnline_=false;return false;}
    if(!running_ && (gateway_!=gateway || pingEpoch_!=linkEpoch_)){
      if(ping_){esp_ping_delete_session(ping_);ping_=nullptr;}
      gateway_=gateway;pingEpoch_=linkEpoch_;
    }
    if(paused)return false;
    if(!running_ && policy_.shouldRecover(now,writes,false)){
      policy_.recovered(now,writes);
      recoveryBssid_=bssid_;recoveryRssi_=WiFi.RSSI();recoveryChannel_=channel_;
      char message[200];snprintf(message,sizeof(message),"recovering Wi-Fi: AP=%s channel=%d RSSI=%d ping=%lu/%lu HTTP timeouts=%lu",
        bssid_.c_str(),channel_,recoveryRssi_,(unsigned long)received_,(unsigned long)sent_,(unsigned long)writes);
      Serial.printf("[NET] %s\n",message);if(log_)log_("NET",message);
      return true;
    }
    if(!running_ && int32_t(now-nextProbeAt_)>=0 && gateway && WiFi.scanComplete()!=WIFI_SCAN_RUNNING){
      if(!ping_ && !createProbe()){nextProbeAt_=now+60000;return false;}
      rttTotal_.store(0);probeEpoch_=linkEpoch_;running_=true;
      if(esp_ping_start(ping_)!=ESP_OK){running_=false;nextProbeAt_=now+60000;}
    }
    return false;
  }
  void addStatus(cJSON *json) const {
    wifi_ps_type_t sleep=WIFI_PS_MIN_MODEM;
    const bool sleepKnown=esp_wifi_get_ps(&sleep)==ESP_OK;
    cJSON_AddStringToObject(json,"bssid",bssid_.c_str());
    cJSON_AddStringToObject(json,"previousBssid",previousBssid_.c_str());
    cJSON_AddNumberToObject(json,"channel",channel_);
    cJSON_AddNumberToObject(json,"sleep",sleepKnown?int(sleep):-1);
    cJSON_AddBoolToObject(json,"probing",running_);
    cJSON_AddBoolToObject(json,"known",policy_.hasResult);
    cJSON_AddNumberToObject(json,"sent",sent_);
    cJSON_AddNumberToObject(json,"received",received_);
    cJSON_AddNumberToObject(json,"meanMs",meanMs_);
    cJSON_AddNumberToObject(json,"badRounds",policy_.badRounds);
    cJSON_AddNumberToObject(json,"recoveries",policy_.recoveries);
    cJSON_AddNumberToObject(json,"disconnects",disconnects_.load());
    cJSON_AddNumberToObject(json,"lastDisconnectReason",disconnectReason_.load());
    cJSON_AddStringToObject(json,"recoveryBssid",recoveryBssid_.c_str());
    cJSON_AddNumberToObject(json,"recoveryRssi",recoveryRssi_);
    cJSON_AddNumberToObject(json,"recoveryChannel",recoveryChannel_);
    cJSON_AddNumberToObject(json,"lastRecoveryAge",policy_.recoveries?uint32_t(millis()-policy_.lastRecoveryAt)/1000:-1.0);
  }
 private:
  WifiHealthPolicy policy_;
  esp_ping_handle_t ping_=nullptr;
  std::atomic<bool> finished_{false};
  std::atomic<uint32_t> probeSent_{0},probeReceived_{0},rttTotal_{0},disconnects_{0},disconnectReason_{0};
  bool running_=false,wasOnline_=false;
  uint32_t gateway_=0,observedGateway_=0,nextProbeAt_=0,sent_=0,received_=0,meanMs_=0;
  uint32_t seenDisconnects_=0,linkEpoch_=0,probeEpoch_=0,pingEpoch_=0;
  String bssid_,previousBssid_,recoveryBssid_;
  int channel_=0,recoveryChannel_=0,recoveryRssi_=0;
  void (*log_)(const char*,const char*)=nullptr;
  bool createProbe() {
    esp_ping_config_t config=ESP_PING_DEFAULT_CONFIG();
    const String address=WiFi.gatewayIP().toString();
    if(!ipaddr_aton(address.c_str(),&config.target_addr))return false;
    config.count=4;config.interval_ms=500;config.timeout_ms=1000;config.data_size=1024;
    esp_ping_callbacks_t callbacks{};callbacks.cb_args=this;
    callbacks.on_ping_success=[](esp_ping_handle_t handle,void *arg){
      auto *self=static_cast<WifiHealth*>(arg);uint32_t time=0;
      esp_ping_get_profile(handle,ESP_PING_PROF_TIMEGAP,&time,sizeof(time));self->rttTotal_.fetch_add(time);
    };
    callbacks.on_ping_end=[](esp_ping_handle_t handle,void *arg){
      auto *self=static_cast<WifiHealth*>(arg);uint32_t sent=0,received=0;
      esp_ping_get_profile(handle,ESP_PING_PROF_REQUEST,&sent,sizeof(sent));
      esp_ping_get_profile(handle,ESP_PING_PROF_REPLY,&received,sizeof(received));
      self->probeSent_.store(sent);self->probeReceived_.store(received);
      self->finished_.store(true,std::memory_order_release);
    };
    return esp_ping_new_session(&config,&callbacks,&ping_)==ESP_OK;
  }
};
