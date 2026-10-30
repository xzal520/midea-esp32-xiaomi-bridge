#pragma once

#include <Arduino.h>
#include <cJSON.h>
#include <esp_freertos_hooks.h>
#include <esp_heap_caps.h>
#include <esp_ota_ops.h>
#include <esp_timer.h>
#include <esp_system.h>
#include <freertos/task.h>

// The bundled SDK has task runtime tracing disabled. Sample the task interrupted
// by each core's tick instead; no busy-loop calibration or extra task is needed.
static DRAM_ATTR portMUX_TYPE cpuSampleMux = portMUX_INITIALIZER_UNLOCKED;
static DRAM_ATTR TaskHandle_t cpuIdleTasks[portNUM_PROCESSORS] = {};
static DRAM_ATTR uint32_t cpuBusyTicks[portNUM_PROCESSORS] = {};
static DRAM_ATTR uint32_t cpuTotalTicks[portNUM_PROCESSORS] = {};
static float cpuCoreLoad[portNUM_PROCESSORS] = {};
static bool cpuSamplingAvailable = false;
static bool cpuSampleReady = false;
static uint32_t lastCpuSampleAt = 0;
static uint32_t cpuSampleWindowMs = 0;
static uint32_t runningAppSize = 0;

static void IRAM_ATTR sampleCpuTick() {
  const UBaseType_t core = xPortGetCoreID();
  const bool busy = xTaskGetCurrentTaskHandleForCPU(core) != cpuIdleTasks[core];
  portENTER_CRITICAL_ISR(&cpuSampleMux);
  ++cpuTotalTicks[core];
  if (busy) ++cpuBusyTicks[core];
  portEXIT_CRITICAL_ISR(&cpuSampleMux);
}

static void beginDeviceMetrics() {
  runningAppSize = ESP.getSketchSize();
  for (UBaseType_t core = 0; core < portNUM_PROCESSORS; ++core) {
    cpuIdleTasks[core] = xTaskGetIdleTaskHandleForCPU(core);
    if (!cpuIdleTasks[core] ||
        esp_register_freertos_tick_hook_for_cpu(sampleCpuTick, core) != ESP_OK) {
      for (UBaseType_t registered = 0; registered < core; ++registered)
        esp_deregister_freertos_tick_hook_for_cpu(sampleCpuTick, registered);
      return;
    }
  }
  lastCpuSampleAt = millis();
  cpuSamplingAvailable = true;
}

static void serviceDeviceMetrics() {
  const uint32_t now = millis();
  if (!cpuSamplingAvailable || static_cast<uint32_t>(now - lastCpuSampleAt) < 1000) return;
  uint32_t busy[portNUM_PROCESSORS], total[portNUM_PROCESSORS];
  portENTER_CRITICAL(&cpuSampleMux);
  for (UBaseType_t core = 0; core < portNUM_PROCESSORS; ++core) {
    busy[core] = cpuBusyTicks[core];
    total[core] = cpuTotalTicks[core];
    cpuBusyTicks[core] = cpuTotalTicks[core] = 0;
  }
  portEXIT_CRITICAL(&cpuSampleMux);
  cpuSampleReady = true;
  for (UBaseType_t core = 0; core < portNUM_PROCESSORS; ++core) {
    if (!total[core]) cpuSampleReady = false;
    cpuCoreLoad[core] = total[core] ? 100.0f * busy[core] / total[core] : 0.0f;
  }
  cpuSampleWindowMs = now - lastCpuSampleAt;
  lastCpuSampleAt = now;
}

static void addHeapMetrics(cJSON *parent, const char *name, uint32_t caps) {
  multi_heap_info_t info;
  heap_caps_get_info(&info, caps);
  const size_t total = info.total_allocated_bytes + info.total_free_bytes;
  cJSON *heap = cJSON_AddObjectToObject(parent, name);
  cJSON_AddNumberToObject(heap, "total", total);
  cJSON_AddNumberToObject(heap, "used", info.total_allocated_bytes);
  cJSON_AddNumberToObject(heap, "free", info.total_free_bytes);
  cJSON_AddNumberToObject(heap, "usage", total ? 100.0 * info.total_allocated_bytes / total : 0);
  cJSON_AddNumberToObject(heap, "minFree", info.minimum_free_bytes);
  cJSON_AddNumberToObject(heap, "maxBlock", info.largest_free_block);
}

static cJSON *buildSystemMetrics() {
  serviceDeviceMetrics();
  cJSON *json = cJSON_CreateObject();
  cJSON_AddStringToObject(json, "chip", ESP.getChipModel());
  cJSON_AddNumberToObject(json, "revision", ESP.getChipRevision());
  cJSON_AddNumberToObject(json, "coreCount", portNUM_PROCESSORS);
  cJSON_AddNumberToObject(json, "uptime", esp_timer_get_time() / 1000000ULL);
  cJSON_AddNumberToObject(json, "esp32ResetReason", esp_reset_reason());
  cJSON_AddBoolToObject(json, "esp32BrownoutReset", esp_reset_reason()==ESP_RST_BROWNOUT);
  cJSON_AddStringToObject(json, "build", __DATE__ " " __TIME__);
  cJSON *cpu = cJSON_AddObjectToObject(json, "cpu");
  cJSON_AddBoolToObject(cpu, "available", cpuSamplingAvailable);
  cJSON_AddBoolToObject(cpu, "ready", cpuSampleReady);
  cJSON_AddStringToObject(cpu, "method", "tick-sampling");
  cJSON_AddNumberToObject(cpu, "sampleMs", cpuSampleWindowMs);
  cJSON_AddNumberToObject(cpu, "mhz", ESP.getCpuFreqMHz());
  float totalLoad = 0;
  cJSON *cores = cJSON_AddArrayToObject(cpu, "cores");
  for (UBaseType_t core = 0; core < portNUM_PROCESSORS; ++core) {
    cJSON *entry = cJSON_CreateObject();
    cJSON_AddNumberToObject(entry, "id", core);
    cJSON_AddNumberToObject(entry, "load", cpuCoreLoad[core]);
    cJSON_AddItemToArray(cores, entry);
    totalLoad += cpuCoreLoad[core];
  }
  cJSON_AddNumberToObject(cpu, "load", totalLoad / portNUM_PROCESSORS);
  addHeapMetrics(json, "ram", MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  addHeapMetrics(json, "psram", MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  const esp_partition_t *running = esp_ota_get_running_partition();
  const esp_partition_t *next = esp_ota_get_next_update_partition(nullptr);
  const uint32_t appCapacity = running ? running->size : 0;
  cJSON *flash = cJSON_AddObjectToObject(json, "flash");
  cJSON_AddNumberToObject(flash, "total", ESP.getFlashChipSize());
  cJSON_AddNumberToObject(flash, "appUsed", runningAppSize);
  cJSON_AddNumberToObject(flash, "appTotal", appCapacity);
  cJSON_AddNumberToObject(flash, "appFree", appCapacity > runningAppSize ? appCapacity - runningAppSize : 0);
  cJSON_AddNumberToObject(flash, "appUsage", appCapacity ? 100.0 * runningAppSize / appCapacity : 0);
  cJSON_AddStringToObject(flash, "partition", running ? running->label : "unknown");
  cJSON_AddNumberToObject(flash, "otaCapacity", next ? next->size : 0);
  return json;
}
