/*
 * gesture_collect.ino
 * Capture paired camera images and model inputs for the training dataset
 */
#include "collection_support.h"

// Capture once, preprocess that frame, and send both representations.
bool captureExample() {
  camera_fb_t *fb = captureTimed();

  // Stop before accessing a failed camera capture.
  if (fb == nullptr) {
    return false;
  }

  bool success = prepareTimed(fb);

  // Send a sample only when grayscale preprocessing succeeded.
  if (success) {
    success = sendExample(fb);
  }

  // Release the camera frame even when preprocessing or transmission fails.
  esp_camera_fb_return(fb);

  return success;
}

// Initialize serial communication and the camera once at startup.
void setup() {
  beginCollection();
}

// Process host capture and readiness commands continuously.
void loop() {
  pollCollection();
}
