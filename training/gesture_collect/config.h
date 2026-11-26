/*
 * config.h
 * Shared image dimensions and timing settings for gesture collection
 */
#pragma once
#include <stddef.h>

// Capture a 320 by 240 JPEG before cropping.
constexpr int RAW_W = 320;
constexpr int RAW_H = 240;
// Produce the same 64 by 64 input used by watch inference.
constexpr int INPUT_W = 64;
constexpr int INPUT_H = 64;
// Center the square crop horizontally within the camera frame.
constexpr int CROP_X = 40;
constexpr int CROP_Y = 0;
constexpr int CROP_SIZE = 240;
// Retain shared inference arena and polling settings; collection does not use them.
constexpr size_t ARENA_BYTES = 256 * 1024;
constexpr unsigned long AUTO_INTERVAL_MS = 500;
