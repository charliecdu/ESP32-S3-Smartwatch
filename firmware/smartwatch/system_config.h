/*
 * system_config.h
 * Shared hardware, service, and timing settings for the watch
 */
#pragma once
#include <Arduino.h>
#include <stddef.h>

constexpr char WIFI_SSID[] = "HOTSPOT_NAME";
constexpr char WIFI_PASSWORD[] = "HOTSPOT_PASSWORD";
constexpr char WEATHER_API_KEY[] = "WEATHER_API_KEY";
constexpr char WATCH_CITY[] = "Evanston";
// Coordinates keep the weather location unambiguous
constexpr char WEATHER_QUERY[] = "42.0451,-87.6877";
constexpr char WATCH_TIMEZONE[] = "CST6CDT,M3.2.0/2,M11.1.0/2";

constexpr int OLED_SDA = 5;
constexpr int OLED_SCL = 6;
constexpr uint8_t OLED_ADDRESS = 0x3C;
constexpr int TOUCH_PIN = D0;
constexpr float TOUCH_THRESHOLD_RATIO = 1.20f;
constexpr uint8_t DEBOUNCE_COUNT = 3;  // Require consecutive samples for touch changes
constexpr uint32_t TOUCH_INTERVAL_MS = 30;
constexpr uint32_t TOUCH_PRINT_MS = 0;

constexpr int RAW_W = 320;
constexpr int RAW_H = 240;
constexpr int INPUT_W = 64;
constexpr int INPUT_H = 64;
constexpr int CROP_X = 40;  // Center the square crop within the camera frame
constexpr int CROP_Y = 0;
constexpr int CROP_SIZE = 240;
constexpr size_t ARENA_BYTES = 256 * 1024;
constexpr uint32_t AUTO_INTERVAL_MS = 500;
constexpr char UNLOCK_GESTURE[] = "vsign";
constexpr float UNLOCK_MIN_SCORE = 0.65f;
constexpr uint8_t UNLOCK_MATCHES = 3;  // Require repeated matches before unlocking
constexpr uint32_t RELOCK_MS = 60000;

constexpr uint32_t SAMPLE_RATE = 16000;
constexpr int PDM_CLK_PIN = 42;
constexpr int PDM_DATA_PIN = 41;
constexpr uint32_t MAX_RECORD_MS = 10000;
constexpr size_t MAX_PCM_BYTES = SAMPLE_RATE * sizeof(int16_t) * MAX_RECORD_MS / 1000;
constexpr size_t WAV_HEADER_BYTES = 44;  // Standard PCM header size
constexpr size_t AUDIO_BLOCK_BYTES = 512;
constexpr uint32_t DISPLAY_INTERVAL_MS = 1000;
constexpr uint32_t WEATHER_REFRESH_MS = 120000;
constexpr uint32_t WEATHER_STALE_MS = 600000;  // Mark cached weather stale after ten minutes
constexpr uint32_t TRANSFER_TIMEOUT_MS = 15000;
