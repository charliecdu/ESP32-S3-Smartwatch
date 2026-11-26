/*
 * touch_input.h
 * Touch calibration and debounced recording requests
 */
#pragma once
#include <Arduino.h>
#include "system_config.h"

struct TouchState {
  uint8_t touchCount = 0;
  uint8_t releaseCount = 0;
  bool latched = true;  // Require a release before the first trigger
};

static TouchState touchState;
static uint32_t touchBaseline = 0;
static uint32_t lastTouchSample = 0;

static bool captureRequested(bool touched, TouchState &state) {
  if (touched) {
    if (state.touchCount < DEBOUNCE_COUNT) state.touchCount++;
    state.releaseCount = 0;  // A touch interrupts the release sequence
    if (state.touchCount == DEBOUNCE_COUNT && !state.latched) {
      state.latched = true;  // Emit only one request per held touch
      return true;
    }
  }
  else {
    state.touchCount = 0;
    if (state.releaseCount < DEBOUNCE_COUNT) state.releaseCount++;
    if (state.releaseCount == DEBOUNCE_COUNT) state.latched = false;
  }
  return false;
}

static void beginTouch() {
  Serial.println("Leave D0 untouched during calibration.");
  delay(500);
  uint64_t total = 0;
  for (uint8_t i = 0; i < 30; ++i) {
    total += touchRead(TOUCH_PIN);
    delay(TOUCH_INTERVAL_MS);
  }
  touchBaseline = total / 30;  // Average calibration readings to reduce noise
  Serial.printf("Touch baseline=%lu threshold=%lu\n", (unsigned long)touchBaseline,
                (unsigned long)(touchBaseline * TOUCH_THRESHOLD_RATIO));
}

static bool pollTouch(uint32_t now) {
  if (!touchBaseline || now - lastTouchSample < TOUCH_INTERVAL_MS) return false;
  lastTouchSample = now;  // Keep sampling at the configured interval
  uint32_t raw = touchRead(TOUCH_PIN);
  uint32_t threshold = (uint32_t)(touchBaseline * TOUCH_THRESHOLD_RATIO);
  if (TOUCH_PRINT_MS != 0) {
    static uint32_t lastPrint = 0;
    if (now - lastPrint >= TOUCH_PRINT_MS) {
      lastPrint = now;
      Serial.printf("Touch raw=%lu threshold=%lu\n", (unsigned long)raw, (unsigned long)threshold);
    }
  }
  return captureRequested(raw > threshold, touchState);
}
