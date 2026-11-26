/*
 * wav_encoder.h
 * Little-endian encoding for mono, 16-bit PCM WAV headers
 */
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

static inline void putLE16(uint8_t *p, uint16_t value) {
  p[0] = value & 0xff;  // Write the least significant byte first
  p[1] = (value >> 8) & 0xff;
}

static inline void putLE32(uint8_t *p, uint32_t value) {
  for (int i = 0; i < 4; ++i) p[i] = (value >> (8 * i)) & 0xff;
}

static inline void writeWavHeader(uint8_t *header, uint32_t pcmBytes, uint32_t sampleRate) {
  memcpy(header, "RIFF", 4);
  putLE32(header + 4, 36 + pcmBytes);  // RIFF size excludes its first eight bytes
  memcpy(header + 8, "WAVEfmt ", 8);
  putLE32(header + 16, 16);
  putLE16(header + 20, 1);  // Format code 1 selects uncompressed PCM
  putLE16(header + 22, 1);  // Record a single audio channel
  putLE32(header + 24, sampleRate);
  putLE32(header + 28, sampleRate * 2);  // Each mono sample occupies two bytes
  putLE16(header + 32, 2);
  putLE16(header + 34, 16);
  memcpy(header + 36, "data", 4);
  putLE32(header + 40, pcmBytes);  // Store the audio payload length
}
