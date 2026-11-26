/*
 * state_machine.h
 * Shared watch state and scoped locking across tasks
 */
#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "system_config.h"
#include "weather_client.h"

enum WatchState { LOCKED, UNLOCKED, RECORDING, RECORDING_READY };
static WatchState state = LOCKED;  // Controls stay locked until the gesture is accepted
static SemaphoreHandle_t watchMutex = nullptr;

class WatchGuard {
public:
  // Wait for exclusive access to shared state
  WatchGuard() { xSemaphoreTake(watchMutex, portMAX_DELAY); }
  ~WatchGuard() { xSemaphoreGive(watchMutex); }  // Release the mutex when the scope ends
  WatchGuard(const WatchGuard &) = delete;
  WatchGuard &operator=(const WatchGuard &) = delete;
};

static bool transferActive = false;
static bool weatherBusy = false;
static bool startPending = false;
static uint32_t lastActivity = 0;
static uint8_t *savedWav = nullptr;  // Retain the last completed note until replacement
static size_t savedPCMBytes = 0;
static uint32_t recordingNumber = 0;
static Weather lastWeather;
static bool haveWeather = false;
static bool weatherFailed = false;
static uint32_t lastWeatherSuccess = 0;

static bool hasAccess(WatchState value) {
  // Only idle, unlocked states allow audio access
  return value == UNLOCKED || value == RECORDING_READY;
}

static const char *stateName(WatchState value) {
  switch (value) {
    case LOCKED: return "LOCKED";
    case UNLOCKED: return "UNLOCKED";
    case RECORDING: return "RECORDING";
    case RECORDING_READY: return "RECORDING READY";
  }
  return "LOCKED";
}

static void recordingFilename(char *name, size_t length, uint32_t number) {
  // Use the same filename on the display and web page
  snprintf(name, length, "note_%04lu.wav", (unsigned long)number);
}
