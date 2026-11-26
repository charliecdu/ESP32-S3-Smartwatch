/*
 * oled_display.h
 * OLED status display for the clock, weather, and recording controls
 */
#pragma once
#include <Wire.h>
#include <U8x8lib.h>
#include <WiFi.h>
#include <time.h>
#include "state_machine.h"
#include "audio_recorder.h"
#include "touch_input.h"
#include "gesture_inference.h"

static U8X8_SSD1306_128X64_NONAME_HW_I2C oled(U8X8_PIN_NONE, OLED_SCL, OLED_SDA);
static uint32_t lastDisplay = 0;
static bool displayDirty = true;
static char displayCache[4][17] = {};

static void printStatusLine(uint8_t row, const char *text) {
  if (row >= 4) return;

  char line[17];
  snprintf(line, sizeof(line), "%-16.16s", text);  // Pad each row to clear leftover characters
  if (strcmp(displayCache[row], line) != 0) {
    oled.setCursor(0, row + 4);
    oled.print(line);
    memcpy(displayCache[row], line, sizeof(line));  // Remember the row to avoid redundant writes
  }
}

static void beginDisplay() {
  oled.setI2CAddress(OLED_ADDRESS << 1);  // U8x8 expects the shifted I2C address
  oled.begin();
  oled.setPowerSave(0);
  oled.setFlipMode(0);
  oled.setFont(u8x8_font_chroma48medium8_r);
  oled.clearDisplay();
  printStatusLine(1, "SMARTWATCH");
  printStatusLine(2, "Starting...");
}

static void updateDisplay(bool gestureReady, uint8_t consecutiveMatches) {
  uint32_t now = millis();
  if (!displayDirty && now - lastDisplay < DISPLAY_INTERVAL_MS) return;
  displayDirty = false;
  lastDisplay = now;

  WatchState current;
  bool hasWeather, pending, transferring;
  Weather weather;
  size_t pcmBytes;
  uint32_t number, activity;

  {
    WatchGuard lock;
    current = state;  // Copy shared values while holding the mutex
    hasWeather = haveWeather;
    weather = lastWeather;
    pcmBytes = savedPCMBytes;
    number = recordingNumber;
    activity = lastActivity;
    pending = startPending;
    transferring = transferActive;
  }

  char line[48];

  if (current == RECORDING) {
    printStatusLine(0, "RECORDING");
    snprintf(line, sizeof(line), "Elapsed: %.1f s", (double)(now - recordingStart) / 1000.0);
    printStatusLine(1, line);
    printStatusLine(2, "Touch to stop");
    printStatusLine(3, "10 second limit");
    return;
  }

  if (current == RECORDING_READY && !pending && (((millis() - activity) / 4000) % 2 == 0 || transferring)) {
    printStatusLine(0, transferring ? "Sending WAV..." : "RECORDING READY");

    recordingFilename(line, sizeof(line), number);
    printStatusLine(1, line);

    snprintf(line, sizeof(line), "%.2fs %luKB", (double)pcmBytes / (SAMPLE_RATE * sizeof(int16_t)), (unsigned long)((pcmBytes + WAV_HEADER_BYTES + 999) / 1000));
    printStatusLine(2, line);

    printStatusLine(3, WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString().c_str() : "WiFi connecting");
    return;
  }

  printStatusLine(0, "");

  time_t timestamp = time(nullptr);
  struct tm localTime = {};

  // Wait for a plausible clock value before showing the date
  if (timestamp > 1704067200 && localtime_r(&timestamp, &localTime)) {
    strftime(line, sizeof(line), "%Y-%m-%d", &localTime);
    printStatusLine(1, line);

    strftime(line, sizeof(line), "%H:%M:%S", &localTime);
    printStatusLine(2, line);
  } else {
    printStatusLine(1, "Date pending");
    printStatusLine(2, "Time pending");
  }

  if (hasWeather) {
    float temperatureF = weather.temperatureC * 9.0f / 5.0f + 32.0f;

    snprintf(line, sizeof(line), "%-9s %5.1fF", WATCH_CITY, temperatureF);
    printStatusLine(3, line);
  } else {
    printStatusLine(3, WATCH_CITY);
  }
}