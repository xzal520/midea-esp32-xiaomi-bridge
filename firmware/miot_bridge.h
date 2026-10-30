#pragma once
#include <Arduino.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "miot_profile.h"
#include "miot_simulator.h"
#include "miot_auto_mode.h"

// Physical mappings use validated Midea feedback. Optional C20 extras can be
// simulated explicitly; they never become physical Midea state.
struct MideaAcSnapshot {
  bool known = false, power = false, eco = false, sleep = false, displayOn = false, roomKnown = false;
  bool auxHeat = false;
  uint8_t mode = 2, fan = 102, swing = 0;
  float target = 26, room = 0;
  uint32_t receivedAt = 0;
};

class MiotMideaBridge {
 public:
  struct FanMapping {
    uint8_t levels[7]={5,10,30,45,60,80,100};
    bool valid() const {
      for(uint8_t i=0;i<7;++i)
        if(levels[i]<1 || levels[i]>100 || (i && levels[i]<=levels[i-1]))return false;
      return true;
    }
  };
  static const FanMapping &defaultFanMapping() { static const FanMapping mapping;return mapping; }
  const FanMapping &fanMapping() const { return fanMapping_; }
  bool setFanMapping(const FanMapping &mapping) {
    if(pending_ || !mapping.valid())return false;
    fanMapping_=mapping;
    lastReported_="";reportCandidate_="";reportAttempts_=0;reportBlocked_=false;
    return true;
  }
  using Log = void (*)(const char *, const char *);
  using Set = void (*)(bool, uint8_t, float, uint8_t, uint8_t, bool, bool, bool);
  using Query = void (*)();
  using SetDisplay = void (*)(bool);
  using SetTimer = bool (*)(bool, bool, uint16_t);
  using GetTimer = bool (*)(String &);
  using SetAuxHeat = bool (*)(const MideaAcSnapshot &);
  using SetSelfClean = bool (*)(bool);
  using SetBuzzer = bool (*)(bool);
  static constexpr const char *MCU_VERSION = "0264";

  void begin(Stream &port, Log log, Set set, Query query, const char *overrideProfile = "AUTO", SetDisplay setDisplay = nullptr, SetTimer setTimer = nullptr, GetTimer getTimer = nullptr, SetAuxHeat setAuxHeat = nullptr, SetSelfClean setSelfClean = nullptr, SetBuzzer setBuzzer = nullptr) {
    port_ = &port; log_ = log; set_ = set; query_ = query; setDisplay_ = setDisplay; setTimer_ = setTimer; getTimer_ = getTimer; setAuxHeat_ = setAuxHeat; setSelfClean_ = setSelfClean; setBuzzer_=setBuzzer; override_ = overrideProfile;
  }
  void setEnabled(bool enabled) {
    if(enabled_==enabled) return;
    enabled_=enabled;
    // Discard transactions rather than replaying old requests on resume.
    pending_=false;pendingCount_=0;pendingQueries_=0;
    pendingC20SleepUpdate_=false;pendingHasSimulation_=false;testReportPending_=false;
    for(auto &entry:queue_) entry="";
    queueCount_=0;wireBusy_=false;wire_="";lineLength_=0;lineOverflow_=false;
    started_=false;initialized_=false;initAttempts_=0;
    lastModelAt_=lastNetQueryAt_=lastInitAttempt_=lastPollAt_=0;
    pauseUntil_=settleUntil_=0;lastReplyAt_=lastNetAt_=0;
    model_="";net_="";lastReported_="";reportCandidate_="";selfCleanKnown_=false;buzzerKnown_=false;
    reportAttempts_=0;reportBlocked_=false;actual_.known=false;
    while(port_ && port_->available()>0) port_->read();
    result_=enabled?"串口已恢复，等待新的真实状态与模块握手":"串口总开关已关闭，全部转发、查询和上报已停止";
  }
  void setDirections(bool rxEnabled, bool txEnabled) {
    if (rxEnabled_ == rxEnabled && txEnabled_ == txEnabled) return;
    rxEnabled_ = rxEnabled; txEnabled_ = txEnabled;
    pending_=false;pendingCount_=0;pendingQueries_=0;
    pendingC20SleepUpdate_=false;pendingHasSimulation_=false;testReportPending_=false;
    for(auto &entry:queue_) entry="";
    queueCount_=0;wireBusy_=false;wire_="";lineLength_=0;lineOverflow_=false;
    started_=false;initialized_=false;initAttempts_=0;
    lastModelAt_=lastNetQueryAt_=lastInitAttempt_=lastPollAt_=0;
    lastReplyAt_=lastNetAt_=0;lastReported_="";reportCandidate_="";selfCleanKnown_=false;buzzerKnown_=false;
    reportAttempts_=0;reportBlocked_=false;actual_.known=false;
    while(port_ && port_->available()>0) port_->read();
    result_=String("米家 UART 方向已切换：RX ")+(rxEnabled_?"开":"关")+" / TX "+(txEnabled_?"开":"关");
  }
  bool enabled() const { return enabled_; }
  bool mideaControlEnabled() const { return mideaControlEnabled_; }
  void setMideaControlEnabled(bool enabled) {
    mideaControlEnabled_=enabled;
    if(!enabled && pending_){
      for(uint8_t i=0;i<pendingCount_;++i)if(pendingResults_[i].field)pendingResults_[i].code=-4004;
      pendingTimer_=false;complete(false);
    }
  }
  void updateMidea(const MideaAcSnapshot &state) {
    if(!enabled_) return;
    const uint8_t previousFan=actual_.fan;
    actual_ = state;
    // Ignore the fan field in every powered-off Midea status frame. Some
    // models replace it with AUTO (0x66) on shutdown; accepting that value
    // would later put AUTO into the full-state command used to power on.
    if (state.known && state.power) {
      lastPoweredFan_ = state.fan;
      lastPoweredFanKnown_ = true;
    } else if(state.known) {
      actual_.fan=lastPoweredFanKnown_?lastPoweredFan_:previousFan;
    }
    if(state.known && state.power && !strcmp(profileName(),"C20") && state.fan!=1) c20MiotSleep_=false;
    autoModeDisplay_.update(state.known,state.power,state.mode,state.roomKnown,state.room,state.target);
    ++generation_;
  }
  int displayMode() const { return actual_.known ? mappedMode() : -1; }
  bool modeEstimated() const { return actual_.known && actual_.power && actual_.mode==1 && mappedMode()>=0; }
  void setSimulationEnabled(bool enabled) {
    simulationEnabled_=enabled; markSimulationChanged();
    result_=enabled?"米家扩展属性模拟已启用；基本控制仍等待美的实际回包":"米家扩展属性模拟已关闭";
  }
  bool simulationEnabled() const { return simulationEnabled_; }
  bool simulationActive() const { return simulationEnabled_ && !strcmp(profileName(),"C20"); }
  uint32_t simulatedControls() const { return simulatedControls_; }
  bool mockMode_ = false;
  void setMockMode(bool enabled) {
    if(mockMode_!=enabled){selfCleanKnown_=false;buzzerKnown_=false;}
    mockMode_ = enabled;
    if (enabled && enabled_) {
      if (!actual_.known) {
        actual_.known = true;
        actual_.power = false;
        actual_.mode = 2;
        actual_.target = 26.0f;
        actual_.fan = 102;
        actual_.swing = 0;
        actual_.eco = false;
        actual_.sleep = false;
        actual_.displayOn = true;
        actual_.roomKnown = true;
        actual_.room = 25.0f;
      }
      actual_.receivedAt = now_;
    }
  }
  bool mockMode() const { return mockMode_; }
  bool requestQuery(const String &command) {
    if(!enabled_) return false;
    if (command == "sync") {
      if (queueCount_ > 4) return false;
      lastReported_ = ""; reportAttempts_=0; reportBlocked_=false;
      if (query_) query_(); return enqueue("net") && enqueue("model");
    }
    if (command != "model" && command != "version" && command != "net" &&
        command != "mac" && command != "time" && command != "get_down") return false;
    return enqueue(command);
  }
  bool queueTestEnvironmentReport(String &message) {
    if(!enabled_){message="串口总开关已关闭。";return false;}
    if (testReportPending_) { message = "测试环境状态已在发送队列中。"; return false; }
    if (!profile() || strcmp(profileName(), "C20")) { message = "仅支持已识别的 C20 米家设备。"; return false; }
    if (!initialized_) { message = "米家握手尚未就绪，请稍后重试。"; return false; }
    if (net_ != "cloud") { message = "米家模块当前不在 cloud 状态，不能确认 App 上报。"; return false; }
    if (pending_ || (wireBusy_ && wireKind_ != Poll)) { message = "米家串口正在处理其他指令，请稍后重试。"; return false; }
    testReport_ = "properties_changed 4 7 23.0 4 9 75";
    testReportPending_ = true;
    message = "已排队：向米家模块测试上报室温 23°C、相对湿度 75%；不会发送空调控制帧。";
    return true;
  }
  bool provision(String &message) {
    if(!enabled_){message="串口总开关已关闭。";return false;}
    if (net_ == "updating") { message = "米家模块仍报告升级中；请先查看下行事件与升级状态，暂不发送重置。"; return false; }
    if (!lastNetAt_ || now_ - lastNetAt_ > 15000) {
      enqueue("net"); message = "已查询米家网络状态；请稍后重试配网重置。"; return false;
    }
    if (pending_ || (provisionAt_ && now_ - provisionAt_ < 15000)) {
      message = "米家模块正在处理指令，请稍后再试。"; return false;
    }
    if (!enqueue("restore", true)) { message = "米家发送队列已满，请稍后重试。"; return false; }
    provisionAt_ = now_;
    message = "已排队发送 restore；以串口 ok 和后续网络状态确认执行结果。";
    return true;
  }
  const String &model() const { return model_; }
  const String &network() const { return net_; }
  const String &version() const { return version_; }
  const String &lastResult() const { return result_; }
  bool seen(uint32_t now) const { return lastReplyAt_ && now - lastReplyAt_ < 15000; }
  bool pending() const { return pending_; }
  void updateSelfClean(bool enabled,uint32_t receivedAt) {
    if(!enabled_)return;
    selfClean_=enabled;selfCleanKnown_=true;selfCleanAt_=receivedAt;++generation_;
  }
  bool selfCleanPending() const {
    if(!pending_)return false;
    for(uint8_t i=0;i<pendingCount_;++i)if(pendingResults_[i].field==SelfClean)return true;
    return false;
  }
  void updateBuzzer(bool enabled,uint32_t receivedAt) {
    if(!enabled_)return;
    buzzer_=enabled;buzzerKnown_=true;buzzerAt_=receivedAt;++generation_;
  }
  bool buzzerPending() const {
    if(!pending_)return false;
    for(uint8_t i=0;i<pendingCount_;++i)if(pendingResults_[i].field==Buzzer)return true;
    return false;
  }
  bool promptToneEnabled() const { return !buzzerKnown_ || buzzer_; }
  void resolveTimer(bool success) {
    if(!pending_ || !pendingTimer_)return;
    pendingTimer_=false;
    for(uint8_t i=0;i<pendingCount_;++i)if(pendingResults_[i].field==Timer)pendingResults_[i].code=success?0:-4004;
    complete(false);
  }
  bool initialized() const { return initialized_; }
  uint32_t rxLines() const { return rxLines_; }
  uint32_t timeouts() const { return timeouts_; }
  uint32_t reports() const { return reports_; }
  uint32_t confirmed() const { return confirmed_; }
  uint32_t failures() const { return failures_; }
  uint32_t otaRequests() const { return otaRequests_; }
  uint32_t responseMs() const { return responseMs_; }
  uint32_t maxResponseMs() const { return maxResponseMs_; }
  uint32_t maxServiceGapMs() const { return maxServiceGapMs_; }
  uint32_t slowServiceGaps() const { return slowServiceGaps_; }
  uint32_t maxReplyQueueMs() const { return maxReplyQueueMs_; }
  uint32_t unexpectedReplies() const { return unexpectedReplies_; }
  const String &lastTimeoutCommand() const { return lastTimeoutCommand_; }
  int32_t reportAge(uint32_t now) const { return lastReportAt_ ? int32_t(now - lastReportAt_) : -1; }
  int32_t netAge(uint32_t now) const { return lastNetAt_ ? int32_t(now - lastNetAt_) : -1; }
  const char *profileName() const { const auto p = profile(); return p ? p->name : "unsupported"; }

  void tick(uint32_t now) {
    if(!enabled_) return;
    now_ = now;
    if (started_) {
      const uint32_t gap=now_-lastServiceAt_;
      if(gap>maxServiceGapMs_) maxServiceGapMs_=gap;
      if(gap>100) ++slowServiceGaps_;
    }
    lastServiceAt_=now_;
    if (mockMode_) actual_.receivedAt = now_;
    if (!started_) { started_ = true; startedAt_ = now; }
    while (rxEnabled_ && port_ && port_->available() > 0) {
      const int v = port_->read(); if (v < 0) break;
      if (v == '\r' || v == '\n') {
        if (lineOverflow_) {
          lineOverflow_=false;lineLength_=0;
          if(wireBusy_ && wireKind_==Poll) wireBusy_=false;
          reply("error \"serial request too long\" -5001");
          continue;
        }
        if (!lineLength_) continue;
        line_[lineLength_] = 0;
        receive(line_); lineLength_ = 0;
      } else if (!lineOverflow_) {
        if (lineLength_ < sizeof(line_) - 1) line_[lineLength_++] = char(v);
        else { lineOverflow_ = true; lineLength_ = 0; ++failures_; result_ = "米家报文过长，已丢弃"; }
      }
    }
    finishSet();
    if (!txEnabled_) return;
    if (wireBusy_) {
      if (int32_t(now_ - wireDeadline_) < 0) return;
      wireBusy_ = false;
      // `get_down` may legitimately return no line when the module has no
      // queued RPC. Retire the poll quietly; count timeouts only for RPCs.
      if (wireKind_ != Poll) ++timeouts_;
      if (wireKind_ == Poll) lastPollAt_=now_;
      if (wireKind_ != Poll) {
        lastTimeoutCommand_=wire_;
        if(log_) log_("MTO",wire_.c_str());
      }
      if (wireKind_ == Init) {
        lastInitAttempt_ = now_;
        if(initAttempts_>=3) result_="米家 MCU 握手连续失败 3 次，已停止自动重试，等待模块重新请求握手。";
      }
      if (wireKind_ == Report) {lastReportAttempt_=now_;failReportAttempt();}
      if (wireKind_ == SimReport) {
        lastSimulationAttempt_=now_;
        failSimulationAttempt("超时");
      }
      // Untagged late replies must drain before another transaction starts.
      settleUntil_=now_+100;
    }
    if (pauseUntil_ && int32_t(now_ - pauseUntil_) < 0) return;
    pauseUntil_ = 0;
    if(settleUntil_ && int32_t(now_-settleUntil_)<0) return;
    settleUntil_=0;
    if (queueCount_) {
      String next = queue_[0];
      const bool urgent=queueUrgent_[0];
      const uint32_t age=now_-queueAt_[0];
      if(urgent && age>maxReplyQueueMs_) maxReplyQueueMs_=age;
      for (uint8_t i = 1; i < queueCount_; ++i) {
        queue_[i - 1] = queue_[i]; queueUrgent_[i-1]=queueUrgent_[i];queueAt_[i-1]=queueAt_[i];
      }
      queue_[--queueCount_] = "";
      const WireKind kind=next=="get_down"?Poll:
        urgent && !strncmp(next.c_str(),"mcu_version ",12)?Init:urgent?Reply:Normal;
      write(next, kind, true); return;
    }
    if (pending_ || now_ - startedAt_ < 1000) return;
    // Service App requests before periodic diagnostics and bulk notifications.
    if(initialized_ && now_-lastPollAt_>=150) {write("get_down",Poll,false);return;}
    if (testReportPending_) {
      testReportPending_ = false;
      write(testReport_, Reply, true);
      return;
    }
    if (!model_.length() && (!lastModelAt_ || now_ - lastModelAt_ >= 5000)) {
      lastModelAt_ = now_; write("model", Normal, true); return;
    }
    if (!lastNetQueryAt_ || now_ - lastNetQueryAt_ >= 10000) {
      lastNetQueryAt_ = now_; write("net", Normal, true); return;
    }
    if (!initialized_ && initAttempts_<3 && (!lastInitAttempt_ || now_ - lastInitAttempt_ >= 3000)) {
      lastInitAttempt_ = now_; write(String("mcu_version ") + MCU_VERSION, Init, true); return;
    }
    // A live cloud/local module with a recognized profile can receive status
    // notifications even if the optional MCU-version handshake was missed.
    if (networkCanReport() && fresh() && profile() && now_ - lastReportCheckAt_ >= 100) {
      // Unchanged state used to rebuild many temporary Strings on every loop
      // after the 1s send gate elapsed. Check at 10Hz, independently of sends.
      lastReportCheckAt_=now_;
      const String report = properties();
      if(report!=reportCandidate_) {reportCandidate_=report;reportAttempts_=0;reportBlocked_=false;}
      if (!reportBlocked_ && now_-lastReportAttempt_>=1000 &&
          (report != lastReported_ || (lastReportAt_ && now_ - lastReportAt_ >= 30000))) {
        ++reportAttempts_;
        lastReportAttempt_ = now_; write(report, Report, true); return;
      }
    }
    if(networkCanReport() && simulationActive() && !simulationFailed_ && now_-lastSimulationAttempt_>=1000) {
      // Static extras report on enable/change/reconnect, not every 30 seconds.
      if(simulationReporting_) {
        String report;
        simulationNextCursor_=simulator_.report(simulationCursor_,report,fresh() && actual_.roomKnown,
          profile() && !strcmp(profile()->name,"C20"));
        if(report.length()>18) {
          lastSimulationAttempt_=now_; simulationSentGeneration_=simulationGeneration_;
          ++simulationAttempts_;
          write(report,SimReport,true); return;
        }
        simulationReporting_=false; lastSimulationReport_=now_;
      }
    }
    if (now_ - lastPollAt_ >= 150) write("get_down", Poll, false);
  }

 private:
  enum WireKind : uint8_t { Normal, Init, Report, SimReport, Poll, Reply };
  enum Field : uint16_t { Power=1, Mode=2, Target=4, Fan=8, Vertical=16, Horizontal=32, Eco=64, Sleep=128, Display=256, Timer=512, AuxHeat=1024, SelfClean=2048, Buzzer=4096 };
  struct PropertyResult { uint8_t siid=0, piid=0; int code=-4003; uint16_t field=0; };
  Stream *port_ = nullptr;
  bool enabled_=true, rxEnabled_=true, txEnabled_=true,mideaControlEnabled_=true;
  Log log_ = nullptr; Set set_ = nullptr; Query query_ = nullptr; SetDisplay setDisplay_ = nullptr; SetTimer setTimer_ = nullptr; GetTimer getTimer_ = nullptr;
  SetAuxHeat setAuxHeat_ = nullptr; SetSelfClean setSelfClean_ = nullptr;
  SetBuzzer setBuzzer_=nullptr;
  bool buzzerKnown_=false,buzzer_=true,wantedBuzzer_=true;
  uint32_t buzzerAt_=0;
  bool selfCleanKnown_=false,selfClean_=false,wantedSelfClean_=false;
  uint32_t selfCleanAt_=0;
  const char *override_ = "AUTO";
  MideaAcSnapshot actual_, wanted_;
  MiotAutoModeDisplay autoModeDisplay_;
  uint8_t lastPoweredFan_=102;
  FanMapping fanMapping_;
  uint8_t targetHumidity_=0; // C20 app sends 0 when its target-humidity control is turned off.
  bool lastPoweredFanKnown_=false, c20MiotSleep_=false, pendingC20SleepUpdate_=false, pendingC20SleepTarget_=false;
  MiotC20Simulator simulator_;
  bool simulationEnabled_=false, simulationReporting_=false, simulationFailed_=false, pendingHasSimulation_=false;
  size_t simulationCursor_=0, simulationNextCursor_=0;
  uint32_t simulationGeneration_=0, simulationSentGeneration_=0;
  uint32_t lastSimulationAttempt_=0, lastSimulationReport_=0, simulatedControls_=0;
  uint8_t simulationAttempts_=0;
  String model_, net_="unknown", version_, result_="等待米家握手", wire_, lastReported_, testReport_;
  String reportCandidate_,lastTimeoutCommand_;
  uint8_t reportAttempts_=0,initAttempts_=0;
  bool reportBlocked_=false;
  String queue_[8]; bool queueUrgent_[8]={}; uint32_t queueAt_[8]={}; uint8_t queueCount_=0;
  char line_[513]; size_t lineLength_=0; bool lineOverflow_=false;
  bool started_=false, initialized_=false, wireBusy_=false, pending_=false, testReportPending_=false, pendingTimer_=false;
  WireKind wireKind_=Normal;
  uint32_t now_=0, startedAt_=0, wireDeadline_=0, lastReplyAt_=0, lastModelAt_=0;
  uint32_t lastNetAt_=0, lastNetQueryAt_=0, lastInitAttempt_=0, lastReportAttempt_=0, lastReportAt_=0;
  uint32_t lastReportCheckAt_=0;
  uint32_t lastPollAt_=0, pauseUntil_=0, provisionAt_=0, generation_=0;
  uint32_t settleUntil_=0,wireStartedAt_=0,lastServiceAt_=0;
  uint32_t responseMs_=0,maxResponseMs_=0,maxServiceGapMs_=0,slowServiceGaps_=0;
  uint32_t maxReplyQueueMs_=0,unexpectedReplies_=0;
  uint32_t pendingGeneration_=0, pendingAt_=0, pendingQueryAt_=0;
  uint32_t rxLines_=0, timeouts_=0, reports_=0, confirmed_=0, failures_=0, otaRequests_=0;
  PropertyResult pendingResults_[20]; uint8_t pendingCount_=0, pendingQueries_=0;

  void markSimulationChanged() {
    ++simulationGeneration_; simulationCursor_=0; simulationAttempts_=0;
    simulationFailed_=false; simulationReporting_=simulationEnabled_;
  }
  void failSimulationAttempt(const char *reason) {
    if (simulationAttempts_ < 3) return;
    simulationReporting_=false; simulationFailed_=true;
    result_=String("米家扩展属性上报连续失败 3 次（")+reason+")，已停止自动重试；检查模块协议后重新启用模拟。";
  }
  void failReportAttempt() {
    if(reportAttempts_<3) return;
    reportBlocked_=true;
    result_="米家状态上报连续失败 3 次，已停止重试；状态变化或手动同步后重新尝试。";
  }

  const MiotProfile *profile() const {
    static const MiotProfile ma2 = {"MA2",2,1,2,3,4,6,3,1,2,0,4,1,0,0};
    static const MiotProfile m46 = {"M4/M6",2,1,2,4,7,11,3,2,4,6,4,7,0,0};
    // Xiaomi MIoT registry xiaomi-c20 versions 1/2: both axes are booleans.
    static const MiotProfile c20 = {"C20",2,1,2,4,7,11,3,2,4,6,4,7,3,6};
    if (!strcmp(override_,"C20")) return &c20;
    if (!strcmp(override_,"MA2")) return &ma2;
    if (!strcmp(override_,"M4_M6")) return &m46;
    if (model_ == "xiaomi.aircondition.c20" || model_.indexOf(".mc") >= 0 || model_.indexOf(".c20") >= 0) return &c20;
    if (model_.indexOf(".ma2") >= 0) return &ma2;
    if (model_.indexOf(".m4") >= 0 || model_.indexOf(".m6") >= 0) return &m46;
    return nullptr;
  }
  bool fresh() const { return enabled_ && (mockMode_ || (actual_.known && now_ - actual_.receivedAt <= 15000)); }
  bool enqueue(const String &s, bool priority=false) {
    if (!s.length() || s.length() > 512) return false;
    if (queueCount_ >= (priority?8:6)) return false; // Reserve room for module replies.
    uint8_t pos=queueCount_;
    if(priority) {pos=0;while(pos<queueCount_ && queueUrgent_[pos]) ++pos;}
    for(uint8_t i=queueCount_;i>pos;--i) {
      queue_[i]=queue_[i-1];queueUrgent_[i]=queueUrgent_[i-1];queueAt_[i]=queueAt_[i-1];
    }
    queue_[pos]=s;queueUrgent_[pos]=priority;queueAt_[pos]=now_;
    ++queueCount_; return true;
  }
  void write(const String &s, WireKind kind, bool logged) {
    if (!txEnabled_) { wireBusy_=false; wire_=""; if(log_) log_("MTX_BLOCK",s.c_str()); return; }
    wire_=s; wireKind_=kind; wireBusy_=true; wireStartedAt_=now_;
    wireDeadline_=now_+(kind==Poll?300:650);
    if(kind==Poll) lastPollAt_=now_;
    if(kind==Init) ++initAttempts_;
    port_->print(s); port_->write('\r');
    if (logged && log_) log_("MTX",s.c_str());
    if (s == "net") lastNetQueryAt_=now_;
    if (s == "restore") { net_="requested"; lastNetAt_=0; }
  }
  static bool integer(const char *s, long &n) {
    if (!s || !*s) return false;
    char *end=nullptr; n=strtol(s,&end,10); return end && !*end;
  }
  static bool boolean(const char *s, bool &b) {
    if (!strcmp(s,"true") || !strcmp(s,"1")) { b=true; return true; }
    if (!strcmp(s,"false") || !strcmp(s,"0")) { b=false; return true; }
    return false;
  }
  static bool parseCountdown(const char *s,bool &enabled,uint16_t &minutes,bool &targetPower) {
    if(!s)return false;
    // tokenize() keeps quoted string values intact, including their wire quotes.
    // Decode this numeric string here; other string properties keep their own format.
    char decoded[32];const size_t length=strlen(s);
    if(length && s[0]=='"'){
      if(length<2 || s[length-1]!='"' || length-2>=sizeof(decoded))return false;
      memcpy(decoded,s+1,length-2);decoded[length-2]=0;s=decoded;
    }
    // Mi Home also writes the scalar string "0" when the private countdown
    // property is disabled. Treat it as a request to clear both native timers.
    if(!strcmp(s,"0")){enabled=false;minutes=0;targetPower=false;return true;}
    char *end=nullptr;const long active=strtol(s,&end,10);
    if(end==s || *end++!=',')return false;
    char *next=nullptr;const long delay=strtol(end,&next,10);
    if(next==end || *next++!=',')return false;
    char *last=nullptr;const long action=strtol(next,&last,10);
    if(last==next || *last || active<0 || active>1 || delay<0 || delay>1440 || action<0 || action>1 || (active && delay==0))return false;
    // Write payload carries target power: 0 = OFF, 1 = ON. This is separate
    // from the readback display field; do not invert both directions together.
    enabled=active!=0;minutes=static_cast<uint16_t>(delay);targetPower=action!=0;return true;
  }
  static bool isNet(const char *s) {
    return !strcmp(s,"net offline") || !strcmp(s,"net local") || !strcmp(s,"net cloud") ||
      !strcmp(s,"net uap") || !strcmp(s,"net unprov") || !strcmp(s,"net updating") ||
      !strcmp(s,"offline") || !strcmp(s,"local") || !strcmp(s,"cloud") ||
      !strcmp(s,"uap") || !strcmp(s,"unprov") || !strcmp(s,"updating");
  }
  bool networkCanReport() const { return net_=="cloud" || net_=="local"; }
  void updateNetwork(const char *s) {
    if(net_!=s && (!strcmp(s,"cloud") || !strcmp(s,"local"))) {
      lastReported_="";reportAttempts_=0;reportBlocked_=false;markSimulationChanged();
    }
    net_=s;lastNetAt_=now_;
  }
  uint8_t toFan(uint8_t level) const {
    if (!level || level>7) return 102;
    const auto p=profile();
    if (p && p->horizontalSwing) return fanMapping_.levels[level-1];
    static const uint8_t table[]={102,20,40,60,80,80,80,100}; return table[level];
  }
  uint8_t fromFan(uint8_t fan) const {
    const auto p=profile();
    // MIoT C20 explicitly defines level 0 as Auto. Some C0 firmware returns
    // that value instead of 101/102; treating it as the lowest speed made the
    // app lose the Auto selection after a status refresh.
    if (p && p->horizontalSwing && fan==0) return 0;
    if (fan>=101) return 0; // Midea replies may report AUTO as 101 or 102.
    if (p && p->horizontalSwing) {
      uint8_t best=1; int distance=256;
      for(uint8_t i=1;i<=7;++i) { int d=abs(int(fan)-int(toFan(i))); if(d<distance){best=i;distance=d;} }
      return best;
    }
    return fan<=20?1:fan<=40?2:fan<=60?3:fan<=80?4:7;
  }
  uint8_t toFanPercent(uint8_t fan) const {
    const uint8_t level=fromFan(fan);
    if(!level) return 101;  // C20 vendor property uses 101 for AUTO.
    static const uint8_t percentByLevel[]={0,5,10,30,45,60,80,100};
    return percentByLevel[level];
  }
  uint8_t fanLevelFromPercent(uint8_t percent) const {
    static const uint8_t percentByLevel[]={0,5,10,30,45,60,80,100};
    uint8_t best=1;
    int distance=abs(int(percent)-int(percentByLevel[1]));
    for(uint8_t level=2;level<=7;++level) {
      const int candidate=abs(int(percent)-int(percentByLevel[level]));
      if(candidate<distance){best=level;distance=candidate;}
    }
    return best;
  }
  int mappedMode() const {
    const auto p=profile();
    if (!actual_.power && p && p->offMode) return p->offMode;
    return autoModeDisplay_.mapped(actual_.mode);
  }
  bool property(uint8_t s,uint8_t p,String &v) const {
    if(!strcmp(profileName(),"C20") && s==10 && p==3 && getTimer_)return getTimer_(v);
    if(!strcmp(profileName(),"C20") && s==2 && p==14){v=String(targetHumidity_);return true;}
    if(!strcmp(profileName(),"C20") && ((s==2 && p==10) || (s==9 && p==2)))return simulator_.read(s,p,v);
    if(!strcmp(profileName(),"C20") && ((s==2 && p==9) || (s==9 && p==1) || (s==5 && p==1)))return physicalProperty(s,p,v);
    return physicalProperty(s,p,v) || (simulationActive() && simulator_.read(s,p,v));
  }
  bool physicalProperty(uint8_t s,uint8_t p,String &v) const {
    const auto pr=profile(); if (!pr || !fresh()) return false;
    if(!strcmp(pr->name,"C20") && s==2 && p==9){v=actual_.auxHeat?"true":"false";return true;}
    if(!strcmp(pr->name,"C20") && s==9 && p==1){
      if(!selfCleanKnown_ || now_-selfCleanAt_>15000)return false;
      v=selfClean_?"\"1\"":"\"0\"";return true;
    }
    if(!strcmp(pr->name,"C20") && s==5 && p==1){
      if(!buzzerKnown_ || now_-buzzerAt_>15000)return false;
      v=buzzer_?"true":"false";return true;
    }
    if (s==pr->acService) {
      if(p==pr->power){v=actual_.power?"true":"false";return true;}
      if(p==pr->mode){
        int m=mappedMode();if(m<0)return false;v=String(m);return true;
      }
      if(p==pr->temperature){v=String(actual_.target,1);return true;}
      if(pr->eco && p==pr->eco){v=actual_.eco?"true":"false";return true;}
      if(pr->sleep && p==pr->sleep){v=(!strcmp(pr->name,"C20")?c20MiotSleep_:actual_.sleep)?"true":"false";return true;}
    }
    if(s==pr->fanService){
      if(p==pr->fanLevel){
        // The Midea mode bit is authoritative: in AUTO, report MIoT AUTO
        // even if the vendor frame carries a stale/manual fan code.
        const uint8_t fan=(actual_.power && actual_.mode==1)?102:
          ((!actual_.power && lastPoweredFanKnown_)?lastPoweredFan_:actual_.fan);
        v=String(fromFan(fan));return true;
      }
      if(p==pr->verticalSwing){v=(actual_.swing&12)?"true":"false";return true;}
      if(pr->horizontalSwing && p==pr->horizontalSwing){v=(actual_.swing&3)?"true":"false";return true;}
      if(pr->horizontalSwing && (p==7 || p==10)){v=(actual_.swing&3)?"4":"0";return true;}
    }
    if(!strcmp(pr->name,"C20") && s==10 && p==1){
      const uint8_t fan=(actual_.power && actual_.mode==1)?102:
        ((!actual_.power && lastPoweredFanKnown_)?lastPoweredFan_:actual_.fan);
      v=String(toFanPercent(fan));return true;
    }
    if(!strcmp(pr->name,"C20") && s==6 && p==2){v="0";return true;}
    if(s==pr->environmentService && p==pr->environmentTemperature && actual_.roomKnown){v=String(actual_.room,1);return true;}
    // C20 SIID 6 / PIID 1 is the display indicator. C0 payload byte 14
    // (full AA frame byte 25) reports brightness; level 7 means off.
    // The separate LED strip is PIID 3.
    if(pr->horizontalSwing && s==6 && p==1){v=actual_.displayOn?"true":"false";return true;}
    return false;
  }
  String properties() const {
    const auto p=profile(); String out="properties_changed",v;
    out.reserve(256);
    const uint8_t ids[][2]={{p->acService,p->power},{p->acService,p->mode},{p->acService,p->temperature},
      {p->acService,p->eco},{p->acService,p->sleep},{p->fanService,p->fanLevel},{10,1},
      {p->fanService,p->verticalSwing},{p->fanService,p->horizontalSwing},{2,14},{2,9},{9,1},{5,1},{2,10},{9,2},
      {p->environmentService,p->environmentTemperature},{6,1},{6,2}};
    for(const auto &id:ids) {
      const bool c20Percent=!strcmp(p->name,"C20") && id[0]==10 && id[1]==1;
      const bool fanProperty=(id[0]==p->fanService && id[1]==p->fanLevel) || c20Percent;
      if(!actual_.power && fanProperty) continue;
      if(id[1] && property(id[0],id[1],v))
        out+=" "+String(id[0])+" "+String(id[1])+" "+v;
    }
    return out;
  }
  void reply(const String &s) {
    if(!enqueue(s,true)){++failures_;result_="米家应答队列已满";}
  }
  void receive(char *s) {
    ++rxLines_;
    const bool none=!strcmp(s,"down none") || !strcmp(s,"down_none");
    if(!none && log_) log_("MRX",s);
    const bool ok=!strcmp(s,"ok");
    const bool down=!strncmp(s,"down ",5);
    const bool error=!strncmp(s,"error",5);
    const bool modelReply=!strncmp(s,"model ",6) || !strncmp(s,"xiaomi.",7) || !strncmp(s,"miot.",5);
    const bool netReply=isNet(s);
    const bool number=s[0]>='0' && s[0]<='9';
    const bool versionReply=wireBusy_ && wire_=="version" && (number || !strncmp(s,"version ",8));
    const bool timeReply=wireBusy_ && wire_=="time" && (number || !strncmp(s,"time ",5));
    const bool macReply=wireBusy_ && wire_=="mac" && (strlen(s)==17 || !strncmp(s,"mac ",4));
    const bool recognized=ok || none || down || error || modelReply || netReply || versionReply || timeReply || macReply;
    if(modelReply) {
      model_=!strncmp(s,"model ",6)?s+6:s; lastReported_="";
    }
    if(netReply) updateNetwork(!strncmp(s,"net ",4)?s+4:s);
    if(versionReply) version_=s;
    if(!recognized) return; // Boot logs do not prove the MCU protocol is alive.
    lastReplyAt_=now_;
    // Only the response type belonging to the current transaction can release
    // it. In particular, a late down none must not consume a report's ok.
    const bool matched=wireBusy_ && (wireKind_==Poll?(down || none || error):
      error || (wireKind_==Normal?
        ((wire_=="model" && modelReply) || (wire_=="net" && netReply) || versionReply || timeReply || macReply ||
          (wire_=="restore" && ok)) : ok));
    if((ok || error || none) && !matched) ++unexpectedReplies_;
    if(matched) {
      responseMs_=now_-wireStartedAt_;
      if(responseMs_>maxResponseMs_) maxResponseMs_=responseMs_;
      if(ok && wireKind_==Init){initialized_=true;result_="米家 MCU 握手成功";}
      if(ok && wireKind_==Report){lastReported_=wire_;lastReportAt_=now_;++reports_;reportAttempts_=0;}
      if(ok && wireKind_==SimReport) {
        lastReportAt_=now_; ++reports_;
        if(simulationSentGeneration_==simulationGeneration_) {
          simulationAttempts_=0;
          simulationCursor_=simulationNextCursor_;
          if(simulationCursor_>=MiotC20Simulator::count) {
            simulationReporting_=false; lastSimulationReport_=now_;
          }
        }
      }
      if(ok && wire_=="restore"){
        pauseUntil_=now_+8000; initialized_=false; lastReported_=""; lastNetAt_=0;
        lastInitAttempt_=0;initAttempts_=0; lastNetQueryAt_=0; result_="米家已确认 restore，等待重启后网络状态";
      }
      if(error && wireKind_!=Poll){
        ++failures_;result_=s;
        if(wireKind_==SimReport) failSimulationAttempt("模块返回 error");
        if(wireKind_==Report) failReportAttempt();
      }
      wireBusy_=false;
    }
    if(!down || none) return;
    // Finish the get_down transaction before replying or publishing properties.
    char *tokens[64];size_t count=0;
    if(!tokenize(s,tokens,64,count)){reply("error \"malformed or too many arguments\" -5001");return;}
    if(count<2) {reply("error \"missing MIoT method\" -5001");return;}
    if(!strcmp(tokens[1],"MIIO_mcu_version_req")){
      initialized_=false;initAttempts_=0;reportAttempts_=0;reportBlocked_=false;
      lastReported_="";markSimulationChanged();reply(String("mcu_version ")+MCU_VERSION);return;
    }
    if(!strcmp(tokens[1],"MIIO_model_req")){
      reply(model_.length()?String("model ")+model_:String("error \"model unavailable\" -5001"));return;
    }
    if(!strcmp(tokens[1],"MIIO_net_change")){
      if(count>=3 && isNet(tokens[2])) updateNetwork(tokens[2]);
      // This is an unsolicited notification, not an RPC; the official MCU
      // serial protocol requires no result/error acknowledgement here.
      return;
    }
    if(!strcmp(tokens[1],"update_fw")){
      ++otaRequests_; result_="收到原机 MCU OTA 请求，已返回 busy；本桥接固件通过网页更新";
      reply("result \"busy\""); return;
    }
    if(!strcmp(tokens[1],"get_properties")){get(tokens,count);return;}
    if(!strcmp(tokens[1],"set_properties")){set(tokens,count);return;}
    reply("error \"unsupported MIoT method\" -5001");
  }
  // Custom MIoT string properties can contain spaces, commas and escaped
  // quotes. Keep each quoted wire value intact instead of strtok splitting it.
  static bool tokenize(char *s,char **tokens,size_t capacity,size_t &count) {
    count=0; bool quoted=false, escaped=false, token=false;
    for(char *p=s;*p;++p) {
      const char c=*p;
      if(!quoted && (c==' ' || c==',' || c=='\t')) {*p=0;token=false;continue;}
      if(!token) {if(count>=capacity)return false;tokens[count++]=p;token=true;}
      if(escaped) {escaped=false;continue;}
      if(quoted && c=='\\') {escaped=true;continue;}
      if(c=='"') quoted=!quoted;
    }
    return !quoted && !escaped;
  }
  void get(char **t,size_t n) {
    if(n<4 || (n-2)%2){reply("error \"malformed get_properties\" -5001");return;}
    String out="result",v;
    for(size_t i=2;i+1<n;i+=2){
      long s,p;if(!integer(t[i],s)||!integer(t[i+1],p)||s<1||s>255||p<1||p>255){reply("error \"invalid property id\" -5001");return;}
      out+=" "+String(s)+" "+String(p);
      if(!strcmp(profileName(),"C20") && s==10 && p==3){
        if(getTimer_ && getTimer_(v))out+=" 0 \""+v+"\"";
        else out+=" -4004";
        continue;
      }
      if(!strcmp(profileName(),"C20") && ((s==9 && p==1) || (s==5 && p==1))){
        if(property(uint8_t(s),uint8_t(p),v))out+=" 0 "+v;else out+=" -4004";
        continue;
      }
      if(property(uint8_t(s),uint8_t(p),v))out+=" 0 "+v;
      else out+=fresh()?" -4003":" -4004";
      if(out.length()>512){reply("error \"get_properties response too long\" -5001");return;}
    }
    reply(out);
    if(!fresh() && query_)query_();
  }
  void logControlDecision(const char *decision) {
    if(!log_) return;
    String out=String(decision)+" power="+String(actual_.power?1:0)+"->"+String(wanted_.power?1:0);
    out+=" mode="+String(actual_.mode)+"->"+String(wanted_.mode);
    out+=" target="+String(actual_.target,1)+"->"+String(wanted_.target,1);
    out+=" fan="+String(actual_.fan)+"->"+String(wanted_.fan);
    out+=" sleep="+String(actual_.sleep?1:0)+"->"+String(wanted_.sleep?1:0);
    out+=" swing="+String(actual_.swing)+"->"+String(wanted_.swing);
    out+=" eco="+String(actual_.eco?1:0)+"->"+String(wanted_.eco?1:0);
    out+=" display="+String(actual_.displayOn?1:0)+"->"+String(wanted_.displayOn?1:0);
    out+=" auxHeat="+String(actual_.auxHeat?1:0)+"->"+String(wanted_.auxHeat?1:0);
    log_("MCTL",out.c_str());
  }
  void set(char **t,size_t n) {
    if(n<5 || (n-2)%3 || (n-2)/3>20){reply("error \"malformed set_properties\" -5001");return;}
    if(pending_){reply("error \"waiting for Midea feedback\" -4004");return;}
    pendingC20SleepUpdate_=false;
    const auto pr=profile();wanted_=actual_;pendingCount_=0;pendingHasSimulation_=false;
    bool changed=false,repeatedMode=false,timerRequested=false,timerEnabled=false,timerTargetPower=false;
    uint16_t timerMinutes=0;
    // A powered-off Midea status frame commonly reports AUTO (0x66), which
    // must not replace the last user-selected speed in the next full-state
    // command. Keep the cached powered-on fan as the default; an explicit fan
    // property in this same set_properties request will overwrite it below.
    if(!actual_.power && lastPoweredFanKnown_) wanted_.fan=lastPoweredFan_;
    bool sleepFanRequest=false,explicitFanRequested=false;uint8_t sleepFanCode=1;
    // A fan change cannot be applied while the physical Midea unit is in
    // AUTO. If this same MIoT transaction explicitly selects a manual mode,
    // however, allow its fan property regardless of property ordering.
    bool autoModeFanNoop=actual_.mode==1;
    if(autoModeFanNoop && pr){
      for(size_t i=2;i+2<n;i+=3){long s,p,value;
        if(integer(t[i],s)&&integer(t[i+1],p)&&s==pr->acService&&p==pr->mode&&
           integer(t[i+2],value)&&value>=2&&value<=5)autoModeFanNoop=false;
      }
    }
    for(size_t i=2;i+2<n;i+=3){
      long s,p;if(!integer(t[i],s)||!integer(t[i+1],p)||s<1||s>255||p<1||p>255){reply("error \"invalid property id\" -5001");return;}
      auto &r=pendingResults_[pendingCount_++];r.siid=s;r.piid=p;r.field=0;r.code=-4003;
      if(pr && !strcmp(pr->name,"C20") && s==pr->acService && p==pr->mode){
        long value;
        // This C20 App sends mode=6 when the user selects the active mode.
        // Acknowledge without changing ANY physical field or replaying a full
        // state snapshot. Power-off remains an explicit power-property write.
        // This is safe even before a fresh status/mode cache is available.
        if(integer(t[i+2],value) && value==pr->offMode){r.code=0;repeatedMode=true;continue;}
      }
      if(pr && !strcmp(pr->name,"C20") && ((s==2 && p==10) || (s==9 && p==2))){
        simulator_.set(uint8_t(s),uint8_t(p),t[i+2],r.code);
        if(!r.code){pendingHasSimulation_=true;changed=true;markSimulationChanged();}
        continue;
      }
      if(pr && !strcmp(pr->name,"C20") && ((s==2 && p==9) || (s==9 && p==1) || (s==5 && p==1))){
        bool value=false;const bool clean=s==9,buzzer=s==5;
        bool valid=false;
        if(clean){
          if(!strcmp(t[i+2],"\"1\"")){value=true;valid=true;}
          else if(!strcmp(t[i+2],"\"0\""))valid=true;
        }else valid=boolean(t[i+2],value);
        if(!valid){r.code=-4002;continue;}
        if(!fresh() || (!mockMode_ && (clean?!setSelfClean_:(buzzer?!setBuzzer_:!setAuxHeat_)))){r.code=-4004;continue;}
        if(clean){wantedSelfClean_=value;r.field=SelfClean;}
        else if(buzzer){wantedBuzzer_=value;r.field=Buzzer;}
        else{wanted_.auxHeat=value;r.field=AuxHeat;}
        r.code=0;changed=true;continue;
      }
      if(pr && !strcmp(pr->name,"C20") && s==10 && p==3){
        if(!parseCountdown(t[i+2],timerEnabled,timerMinutes,timerTargetPower)){r.code=-4002;continue;}
        if(!fresh()){r.code=-4004;continue;}
        r.field=Timer;r.code=0;changed=true;timerRequested=true;continue;
      }
      if(pr && !strcmp(pr->name,"C20") && s==2 && p==14){
        long value;
        if(!integer(t[i+2],value) || value<0 || value>100){r.code=-4002;continue;}
        targetHumidity_=static_cast<uint8_t>(value);r.code=0;pendingHasSimulation_=true;changed=true;continue;
      }
      if(pr && !strcmp(pr->name,"C20") && s==6 && p==2){
        long value;
        if(!fresh()){r.code=-4004;continue;}
        if(integer(t[i+2],value)&&value>=0&&value<=2){wanted_.displayOn=true;r.field=Display;r.code=0;changed=true;}
        else r.code=-4002;
        continue;
      }
      if(pr && !strcmp(pr->name,"C20") && s==10 && p==1){
        char *end=nullptr;const long value=strtol(t[i+2],&end,10);
        if(!fresh()){r.code=-4004;continue;}
        if(!*t[i+2] || !end || *end || value<0 || value>101){r.code=-4002;continue;}
        if(autoModeFanNoop){r.code=0;continue;}
        if(value==101) wanted_.fan=102;
        else wanted_.fan=toFan(fanLevelFromPercent(static_cast<uint8_t>(value)));
        r.field=Fan;r.code=0;changed=true;explicitFanRequested=true;
        if(pr && !strcmp(pr->name,"C20")){pendingC20SleepUpdate_=true;pendingC20SleepTarget_=false;}
        continue;
      }
      if(simulationActive() && simulator_.set(s,p,t[i+2],r.code)) {
        if(!r.code) {pendingHasSimulation_=true;markSimulationChanged();}
        continue;
      }
      if(!fresh()){r.code=-4004;continue;}
      if(!pr)continue;
      const char *v=t[i+2];bool b=false;long num=0;
      if(s==pr->acService && p==pr->power){if(boolean(v,b)){wanted_.power=b;r.field=Power;}else r.code=-4002;}
      else if(s==pr->acService && p==pr->mode){
        if(integer(v,num) && num>=2 && num<=5){
          wanted_.mode=num==2?2:num==3?3:num==4?5:4;r.field=Mode;
        }
        else r.code=-4002;
      } else if(s==pr->acService && p==pr->temperature){
        char *end=nullptr;float value=strtof(v,&end);
        if(*v && end && !*end && isfinite(value) && value>=16 && value<=31){wanted_.target=fminf(30,fmaxf(17,roundf(value*2)/2));r.field=Target;}else r.code=-4002;
      } else if(s==pr->acService && pr->eco && p==pr->eco){if(boolean(v,b)){wanted_.eco=b;r.field=Eco;}else r.code=-4002;}
      else if(s==pr->acService && pr->sleep && p==pr->sleep){
        if(boolean(v,b)){
          if(!strcmp(pr->name,"C20")){
            // C20 sleep is a local MIoT state, not the Midea sleep bit.
            // On -> raw UART fan code 1; Off -> configured C20 fan level 3.
            pendingC20SleepUpdate_=true;pendingC20SleepTarget_=b;
            r.field=Fan;sleepFanRequest=true;sleepFanCode=b?1:toFan(3);
          }else{wanted_.sleep=b;r.field=Sleep;}
        }else r.code=-4002;
      }
      else if(s==6 && p==1 && pr->horizontalSwing){if(boolean(v,b)){wanted_.displayOn=b;r.field=Display;}else r.code=-4002;}
      else if(s==pr->fanService && p==pr->fanLevel){if(integer(v,num)&&num>=0&&num<=7){if(autoModeFanNoop){r.code=0;continue;}wanted_.fan=toFan(num);r.field=Fan;explicitFanRequested=true;if(!strcmp(pr->name,"C20")){pendingC20SleepUpdate_=true;pendingC20SleepTarget_=false;}}else r.code=-4002;}
      else if(s==pr->fanService && p==pr->verticalSwing){if(boolean(v,b)){wanted_.swing=(wanted_.swing&3)|(b?12:0);r.field=Vertical;}else r.code=-4002;}
      else if(s==pr->fanService && pr->horizontalSwing && p==pr->horizontalSwing){if(boolean(v,b)){wanted_.swing=(wanted_.swing&12)|(b?3:0);r.field=Horizontal;}else r.code=-4002;}
      else if(s==pr->fanService && pr->horizontalSwing && (p==7 || p==10)){
        if(integer(v,num)&&(num==0||num==4)){wanted_.swing=(wanted_.swing&12)|(num?3:0);r.field=Horizontal;}else r.code=-4002;
      }
      if(r.field){r.code=0;changed=true;}
    }
    if(sleepFanRequest && !explicitFanRequested) wanted_.fan=sleepFanCode;
    else if(sleepFanRequest && explicitFanRequested) pendingC20SleepTarget_=false;
    if(!mideaControlEnabled_){
      bool blocked=false;
      for(uint8_t i=0;i<pendingCount_;++i)if(pendingResults_[i].field){pendingResults_[i].code=-4004;blocked=true;}
      if(blocked){complete(false);result_="IO16 RX 调试中，向美的发送已暂停";return;}
    }
    if(timerRequested){
      if(pendingCount_!=1 || !setTimer_){
        for(uint8_t i=0;i<pendingCount_;++i)pendingResults_[i].code=-4002;
        complete(false);return;
      }
      pending_=true;pendingTimer_=true;pendingAt_=now_;pendingQueryAt_=now_;pendingGeneration_=generation_;pendingQueries_=0;
      if(!setTimer_(timerEnabled,timerTargetPower,timerEnabled?timerMinutes:0)){
        pending_=false;pendingTimer_=false;pendingResults_[0].code=-4004;complete(false);return;
      }
      result_="米家倒计时已发送，等待美的回包确认";
      return;
    }
    if(!changed){
      for(uint8_t i=0;i<pendingCount_;++i)if(pendingResults_[i].code)repeatedMode=false;
      logControlDecision(repeatedMode?"skip:repeated C20 mode=6":"skip:no physical change");
      complete(false);
      if(repeatedMode)result_="米家重复选择当前模式，已保持空调原状态，未发送控制";
      if(!fresh()&&query_)query_();return;
    }
    if(pendingHasSimulation_){
      bool hasPhysical=false;
      for(uint8_t i=0;i<pendingCount_;++i)if(pendingResults_[i].field)hasPhysical=true;
      if(!hasPhysical){
        const String state=properties();
        complete(false);
        reply(state);
        lastReported_=state;
        reportCandidate_=state;
        return;
      }
    }
    bool needsPhysicalChange=false;
    for(uint8_t i=0;i<pendingCount_;++i)
      if(pendingResults_[i].field && !matches(pendingResults_[i].field)) needsPhysicalChange=true;
    // An already satisfied request needs no full-state write. In particular,
    // repeated App commands must not replay an unrelated cached AC snapshot.
    if(fresh() && !needsPhysicalChange){logControlDecision("skip:already matched");complete(false,true);return;}
    bool hasAuxHeat=false,hasSelfClean=false,hasBuzzer=false,hasState=false,displayOnly=true;
    for(uint8_t i=0;i<pendingCount_;++i){const auto f=pendingResults_[i].field;
      if(f==SelfClean){hasSelfClean=true;continue;}
      if(f==Buzzer){hasBuzzer=true;continue;}
      if(f){hasState=true;if(f!=Display)displayOnly=false;if(f==AuxHeat)hasAuxHeat=true;}
    }
    if(mockMode_){
      actual_=wanted_;
      actual_.known=true;
      actual_.receivedAt=now_;
      ++generation_;
      lastReported_="";
      for(uint8_t i=0;i<pendingCount_;++i){
        if(pendingResults_[i].field)pendingResults_[i].code=0;
      }
      if(hasSelfClean)updateSelfClean(wantedSelfClean_,now_);
      if(hasBuzzer)updateBuzzer(wantedBuzzer_,now_);
      if(hasState && hasAuxHeat && setAuxHeat_)setAuxHeat_(wanted_);
      else if(hasState && set_)set_(wanted_.power,wanted_.mode,wanted_.target,wanted_.fan,wanted_.swing,wanted_.eco,wanted_.sleep,wanted_.displayOn);
      complete(false);
      result_="脱机模拟：米家控制已生效";
      return;
    }
    pending_=true;pendingAt_=now_;pendingQueryAt_=now_;pendingGeneration_=generation_;pendingQueries_=0;
    logControlDecision("send:MIoT request");
    result_="米家控制已发送，等待美的实际回包";
    if(hasState && hasAuxHeat){
      if(!setAuxHeat_(wanted_))for(uint8_t i=0;i<pendingCount_;++i)if(pendingResults_[i].field && pendingResults_[i].field!=SelfClean && pendingResults_[i].field!=Buzzer){pendingResults_[i].field=0;pendingResults_[i].code=-4004;}
    } else if(hasState && displayOnly && setDisplay_)setDisplay_(wanted_.displayOn);
    else if(hasState && set_)set_(wanted_.power,wanted_.mode,wanted_.target,wanted_.fan,wanted_.swing,wanted_.eco,wanted_.sleep,wanted_.displayOn);
    if(hasSelfClean && !setSelfClean_(wantedSelfClean_))for(uint8_t i=0;i<pendingCount_;++i)if(pendingResults_[i].field==SelfClean){pendingResults_[i].field=0;pendingResults_[i].code=-4004;}
    if(hasBuzzer && !setBuzzer_(wantedBuzzer_))for(uint8_t i=0;i<pendingCount_;++i)if(pendingResults_[i].field==Buzzer){pendingResults_[i].field=0;pendingResults_[i].code=-4004;}
    bool awaiting=false;for(uint8_t i=0;i<pendingCount_;++i)if(pendingResults_[i].field)awaiting=true;
    if(!awaiting)complete(false);
  }
  bool matches(uint16_t field) const {
    switch(field){
      case Power:return actual_.power==wanted_.power;
      case Mode:return actual_.mode==wanted_.mode;
      case Target:return fabsf(actual_.target-wanted_.target)<0.24f;
      case Fan:{const auto p=profile();return (p&&p->horizontalSwing&&actual_.fan==0&&wanted_.fan>=101)||
        (actual_.fan>=101&&wanted_.fan>=101)||actual_.fan==wanted_.fan;}
      case Vertical:return bool(actual_.swing&12)==bool(wanted_.swing&12);
      case Horizontal:return bool(actual_.swing&3)==bool(wanted_.swing&3);
      case Eco:return actual_.eco==wanted_.eco;
      case Sleep:return actual_.sleep==wanted_.sleep;
      case Display:return actual_.displayOn==wanted_.displayOn;
      case AuxHeat:return actual_.auxHeat==wanted_.auxHeat;
      case SelfClean:return selfCleanKnown_ && now_-selfCleanAt_<=15000 &&
        (!pending_ || int32_t(selfCleanAt_-pendingAt_)>=0) && selfClean_==wantedSelfClean_;
      case Buzzer:return buzzerKnown_ && now_-buzzerAt_<=15000 &&
        (!pending_ || int32_t(buzzerAt_-pendingAt_)>=0) && buzzer_==wantedBuzzer_;
      default:return false;
    }
  }
  void finishSet() {
    if(!pending_ || pendingTimer_)return;
    bool all=generation_>pendingGeneration_ && fresh();
    for(uint8_t i=0;i<pendingCount_;++i)if(pendingResults_[i].field && !matches(pendingResults_[i].field))all=false;
    if(all){complete(true);return;}
    if(now_-pendingAt_>=((selfCleanPending() || buzzerPending())?8000U:2200U)){complete(true);return;}
    if(pendingQueries_<3 && now_-pendingQueryAt_>=500 && query_){pendingQueryAt_=now_;++pendingQueries_;query_();}
  }
  void complete(bool checkFeedback, bool alreadyMatched=false) {
    String out="result";bool failed=false,physical=false;
    for(uint8_t i=0;i<pendingCount_;++i){auto &r=pendingResults_[i];
      if(checkFeedback && r.field && (generation_<=pendingGeneration_ || !fresh() || !matches(r.field)))r.code=-4004;
      if(r.code)failed=true;
      if(r.field)physical=true;
      out+=" "+String(r.siid)+" "+String(r.piid)+" "+String(r.code);
    }
    if(pendingC20SleepUpdate_){
      const auto pr=profile();
      if(pr && !strcmp(pr->name,"C20"))for(uint8_t i=0;i<pendingCount_;++i){
        const auto &r=pendingResults_[i];
        if(r.code==0 && ((r.siid==pr->acService && r.piid==pr->sleep) || r.field==Fan))c20MiotSleep_=pendingC20SleepTarget_;
      }
      pendingC20SleepUpdate_=false;
    }
    reply(out);pending_=false;pendingCount_=0;lastReported_="";
    if(failed){++failures_;result_="部分属性不支持、无新鲜美的状态或回包未确认；已返回错误码";}
    else if(physical){
      if(alreadyMatched) result_="请求目标已与新鲜美的状态一致，未重复发送控制";
      else {++confirmed_;result_=pendingHasSimulation_?"基本控制已获美的实际回包；扩展属性按模拟值保存":"美的实际回包已确认米家控制";}
    }
    else if(pendingHasSimulation_){++simulatedControls_;result_="米家扩展属性已在内存中模拟保存，未向美的发送控制";}
  }
};
