/*
 * collection_support.h
 * Handle capture commands and send validated image pairs over serial
 */
#pragma once
#include "camera_frontend.h"

bool captureExample();
// Track packet order and timing values for the current capture.
static uint32_t sequence = 0;
static uint32_t captureUs = 0, preprocessUs = 0;

// Update the reflected CRC-32 checksum one byte at a time.
static uint32_t crcUpdate(uint32_t crc, const uint8_t *data, size_t count) {
  while (count--) {
    crc ^= *data++;
    for (int j = 0; j < 8; ++j) crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320UL : 0);
  }
  return crc;
}

// Send bounded chunks and stop if serial output stalls for fifteen seconds.
static bool sendAll(const void *data, size_t count) {
  const uint8_t *p = static_cast<const uint8_t *>(data);
  uint32_t last = millis();
  while (count) {
    size_t n = Serial.write(p, count > 1024 ? 1024 : count);
    if (n) { p += n; count -= n; last = millis(); }
    else if (millis() - last > 15000) return false;
    delay(1);
  }
  return true;
}

// Measure capture time separately from image preprocessing.
static camera_fb_t *captureTimed() {
  const uint32_t started = micros();
  camera_fb_t *fb = captureFresh();
  captureUs = micros() - started;
  return fb;
}

// Record how long JPEG decoding and grayscale conversion take.
static bool prepareTimed(camera_fb_t *fb) {
  const uint32_t started = micros();
  const bool ok = prepareImage(fb);
  preprocessUs = micros() - started;
  return ok;
}

// Protect the JPEG and grayscale payload together with one CRC-32.
static bool sendExample(camera_fb_t *fb) {
  uint32_t crc = crcUpdate(0xffffffff, fb->buf, fb->len);
  crc = crcUpdate(crc, grayInput, sizeof(grayInput)) ^ 0xffffffff;
  // Keep these fourteen fields in the order expected by the Python receiver.
  uint32_t header[] = {1, ++sequence, RAW_W, RAW_H, INPUT_W, INPUT_H,
      CROP_X, CROP_Y, CROP_SIZE, (uint32_t)fb->len, sizeof(grayInput),
      captureUs, preprocessUs, crc};
  // Send the marker, header, original JPEG, and exact model input in order.
  return sendAll("GST7", 4) && sendAll(header, sizeof(header)) &&
      sendAll(fb->buf, fb->len) && sendAll(grayInput, sizeof(grayInput));
}

// Wait for USB serial and announce readiness only after camera setup succeeds.
static void beginCollection() {
  Serial.begin(115200);
  Serial.setTimeout(1000);
  while (!Serial) delay(10);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  if (!initCameraFrontend()) {
    while (true) {
      Serial.println("# ERROR camera/PSRAM initialization failed; enable OPI PSRAM and check camera cable");
      delay(2000);
    }
  }
  Serial.println("# READY GST7 frontend=gesture_gray_v1");
}

// Handle one host command and report capture errors as status lines.
static void pollCollection() {
  if (!Serial.available()) { delay(5); return; }
  String command = Serial.readStringUntil('\n');
  command.trim();
  if (command == "CAPTURE") {
    Serial.println("# CAPTURING");
    // Turn on the active-low LED while a sample is being captured and sent.
    digitalWrite(LED_BUILTIN, LOW);
    const bool ok = captureExample();
    digitalWrite(LED_BUILTIN, HIGH);
    Serial.println(ok ? "# DONE" : "# ERROR capture, preprocessing, or send failed");
  } else if (command == "PING") {
    Serial.println("# READY GST7 frontend=gesture_gray_v1");
  } else {
    Serial.println("# ERROR expected CAPTURE or PING");
  }
}
