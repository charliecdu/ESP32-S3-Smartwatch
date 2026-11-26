/*
 * image_preprocessing.h
 * Crop and convert camera pixels into grayscale model input
 */
#pragma once
#include <stdint.h>
#include "system_config.h"


inline bool makeModelInput(const uint8_t *rgb, uint8_t *gray) {
  if (!rgb || !gray) {  // Both buffers must be supplied by the caller
    return false;
  }

  for (int y = 0; y < INPUT_H; y++) {
    // Sample the center of each output row
    int sy = CROP_Y + ((2 * y + 1) * CROP_SIZE) / (2 * INPUT_H);

    for (int x = 0; x < INPUT_W; x++) {
      // Use the same sampling rule for columns
      int sx = CROP_X + ((2 * x + 1) * CROP_SIZE) / (2 * INPUT_W);

      const uint8_t *p = rgb + 3 * (sy * RAW_W + sx);  // Locate the packed RGB pixel

      // Average the channels with integer rounding
      gray[y * INPUT_W + x] = (unsigned(p[0]) + p[1] + p[2] + 1) / 3;
    }
  }

  return true;
}
