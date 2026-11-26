/*
 * audio_recorder.h
 * Microphone capture and PSRAM storage for the latest voice note
 */
#pragma once
#include <Arduino.h>
#include <ESP_I2S.h>
#include <esp_heap_caps.h>
#include "state_machine.h"
#include "wav_encoder.h"

static I2SClass microphone;
static uint8_t *workingWav = nullptr;
static size_t recordedBytes = 0;
static uint32_t recordingStart = 0;
static bool audioReady = false;
static String audioMessage;

static bool beginAudioStorage() {
  if (!psramFound()) return false;  // Audio buffers need external RAM
  savedWav = (uint8_t *)heap_caps_malloc(WAV_HEADER_BYTES + MAX_PCM_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  workingWav = (uint8_t *)heap_caps_malloc(WAV_HEADER_BYTES + MAX_PCM_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!savedWav || !workingWav) {
    free(savedWav);
    free(workingWav);
    savedWav = workingWav = nullptr;
    return false;
  }
  return true;
}

static bool beginMicrophone() {
  microphone.setPinsPdmRx(PDM_CLK_PIN, PDM_DATA_PIN);
  microphone.setTimeout(30);
  bool microBegin = microphone.begin(I2S_MODE_PDM_RX, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO);
  return microBegin;
}

static void requestRecording() {
  WatchGuard lock;
  if (!hasAccess(state) || transferActive) return;
  if (!audioReady) {
    audioMessage = "No audio memory";
    return;
  }
  startPending = !startPending;  // A second touch cancels a queued start
  lastActivity = millis();
  audioMessage = "";
}

static void startRecordingIfReady() {
  {
    WatchGuard lock;
    if (!startPending || !hasAccess(state) || transferActive || weatherBusy) return;
    startPending = false;
    state = RECORDING;
  }
  recordedBytes = 0;
  if (!beginMicrophone()) {
    microphone.end();
    WatchGuard lock;
    state = savedPCMBytes ? RECORDING_READY : UNLOCKED;
    lastActivity = millis();
    audioMessage = "Mic init failed";
    Serial.println("Microphone initialization failed.");
    return;
  }
  recordingStart = millis();
  Serial.println("Recording started; a completed recording replaces the previous note.");
}

static void finishRecording() {
  microphone.end();
  recordedBytes &= ~(size_t)1;  // Keep only complete 16-bit samples
  WatchGuard lock;
  if (recordedBytes >= sizeof(int16_t)) {
    writeWavHeader(workingWav, recordedBytes, SAMPLE_RATE);
    uint8_t *old = savedWav;
    savedWav = workingWav;
    workingWav = old;  // Reuse the previous buffer for the next recording
    savedPCMBytes = recordedBytes;
    ++recordingNumber;
    audioMessage = "Saved";
    Serial.printf("Saved %lu bytes, %.2f seconds\n", (unsigned long)savedPCMBytes,
                  (double)savedPCMBytes / (SAMPLE_RATE * sizeof(int16_t)));
  } else {
    audioMessage = "No audio saved";
  }
  state = savedPCMBytes ? RECORDING_READY : UNLOCKED;
  lastActivity = millis();
}

static void captureAudioBlock(bool stopRequested) {
  if (stopRequested || millis() - recordingStart >= MAX_RECORD_MS || recordedBytes >= MAX_PCM_BYTES) {
    finishRecording();
    return;
  }
  // Limit each read to the remaining buffer space
  size_t requested = min((size_t)AUDIO_BLOCK_BYTES, MAX_PCM_BYTES - recordedBytes);
  size_t received = microphone.readBytes((char *)(workingWav + WAV_HEADER_BYTES + recordedBytes), requested);
  recordedBytes += min(received, requested) & ~(size_t)1;
  if (millis() - recordingStart >= MAX_RECORD_MS || recordedBytes >= MAX_PCM_BYTES) finishRecording();
}
