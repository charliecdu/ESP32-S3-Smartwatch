/*
 * image_transform.h
 * Crop camera pixels and convert them into grayscale training inputs
 */
#pragma once
#include <stdint.h>
#include "config.h"


// Match the preprocessing used by the smartwatch firmware.
inline bool makeModelInput(const uint8_t *rgb, uint8_t *gray) {
  // Both source and destination buffers must be supplied.
  if (!rgb || !gray) {
    return false;
  }

  for (int y = 0; y < INPUT_H; y++) {
    // Sample the center of each output row within the crop.
    int sy = CROP_Y + ((2 * y + 1) * CROP_SIZE) / (2 * INPUT_H);

    for (int x = 0; x < INPUT_W; x++) {
      // Use the same center sampling rule for output columns.
      int sx = CROP_X + ((2 * x + 1) * CROP_SIZE) / (2 * INPUT_W);

      // Locate the selected pixel in the packed RGB buffer.
      const uint8_t *p = rgb + 3 * (sy * RAW_W + sx);

      // Average the three channels with integer rounding.
      gray[y * INPUT_W + x] = (unsigned(p[0]) + p[1] + p[2] + 1) / 3;
    }
  }

  return true;
}
