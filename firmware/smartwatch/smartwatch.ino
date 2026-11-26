/*
 * smartwatch.ino
 * Smartwatch startup and main control loop
 */
#include <Arduino.h>
#include "system_config.h"
#include "state_machine.h"
#include "touch_input.h"
#include "gesture_inference.h"
#include "audio_recorder.h"
#include "network_manager.h"
#include "oled_display.h"

static bool gestureReady = false;
static uint8_t consecutiveMatches = 0;
static uint32_t nextGesture = 0;

static void runGestureInference(uint32_t now) {
  if (!gestureReady || (int32_t)(now - nextGesture) < 0) return;
  bool match = inferUnlockGesture();
  nextGesture = millis() + AUTO_INTERVAL_MS;
  // Any missed match resets the unlock sequence
  consecutiveMatches = match ? consecutiveMatches + 1 : 0;
  displayDirty = true;
  if (consecutiveMatches >= UNLOCK_MATCHES) {
    WatchGuard lock;
    state = savedPCMBytes ? RECORDING_READY : UNLOCKED;
    lastActivity = millis();
    consecutiveMatches = 0;
    audioMessage = "";
    Serial.println("GESTURE ACCEPTED: recording and audio access unlocked.");
  }
}

static void checkInactivity() {
  WatchGuard lock;
  uint32_t now = millis();
  if (hasAccess(state) && !transferActive && !startPending && now - lastActivity >= RELOCK_MS) {
    state = LOCKED;  // Inactivity locks the controls while retaining the note
    consecutiveMatches = 0;
    nextGesture = now;
    audioMessage = "";
    displayDirty = true;
    Serial.println("Controls relocked.");
  }
}

void setup() {
  Serial.begin(115200);
  watchMutex = xSemaphoreCreateMutex(); 
  beginDisplay();
  if (!watchMutex) {
    printStatusLine(1, "Mutex failed");
    while (true) delay(1000);
  }
  beginTouch();  // Calibrate before normal touch polling begins
  audioReady = beginAudioStorage();
  if (!audioReady) Serial.println("Audio memory unavailable. Enable OPI PSRAM.");
  gestureReady = beginGesture();
  lastActivity = millis();
  nextGesture = millis();
  networkTaskReady = beginNetwork();  // Run network requests outside the main loop
  if (!networkTaskReady) Serial.println("Network task initialization failed.");
  updateDisplay(gestureReady, consecutiveMatches);
}

void loop() {
  uint32_t now = millis();
  bool touched = pollTouch(now);
  WatchState current;
  {
    WatchGuard lock;
    current = state;  // Take a consistent snapshot for this loop iteration
  }
  switch (current) {
    case LOCKED:
      runGestureInference(now);
      break;
    case UNLOCKED:
    case RECORDING_READY:
      if (touched) {
        requestRecording();
        displayDirty = true;
      }
      startRecordingIfReady();
      checkInactivity();
      break;
    case RECORDING:
      captureAudioBlock(touched);  // A new touch stops the current recording
      break;
  }
  {
    WatchGuard lock;
    if (state != current) displayDirty = true;
  }
  updateDisplay(gestureReady, consecutiveMatches);
  delay(1);  // Yield so background tasks can run
}
