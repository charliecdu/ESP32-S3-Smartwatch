/*
 * gesture_inference.h
 * Gesture model setup, inference, and unlock confidence checks
 */
#pragma once
#include <Arduino.h>
#include <math.h>
#include <string.h>
#include "system_config.h"

#if __has_include("model_data.h") && __has_include("model_config.h")
#define WATCH_HAS_MODEL 1
#include "camera_driver.h"
#include <Chirale_TensorFlowLite.h>
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "model_data.h"
#include "model_config.h"

constexpr int CLASSES = sizeof(CLASS_LABELS) / sizeof(CLASS_LABELS[0]);
static tflite::MicroInterpreter *interpreter = nullptr;
static TfLiteTensor *inputTensor = nullptr, *outputTensor = nullptr;
static bool gestureError(const char *msg) {
  Serial.printf("Gesture unavailable: %s\n", msg);
  return false;
}

bool fillModelInput() {
  for (int i = 0; i < INPUT_W * INPUT_H; i++) {
    float pixel = grayInput[i] / 255.0f;  // Normalize grayscale before quantization
    long q = lrintf(pixel / INPUT_SCALE) + INPUT_ZERO_POINT;

    if (q < -128) q = -128;  // Clamp to the signed 8-bit input range
    if (q > 127) q = 127;

    inputTensor->data.int8[i] = static_cast<int8_t>(q);
  }

  return true;
}

int decodePrediction(float *scores, const char *&prediction) {
  int best = 0;

  for (int i = 0; i < CLASSES; i++) {
    scores[i] = (outputTensor->data.int8[i] - outputTensor->params.zero_point) * outputTensor->params.scale;

    if (scores[i] > scores[best]) {
      best = i;
    }
  }

  if (scores[best] < ACCEPT_THRESHOLD) {
    prediction = "UNKNOWN";  // Low-confidence results cannot unlock the watch
  }
  else if (best == BACKGROUND_INDEX) {
    prediction = "NO_GESTURE";
  }
  else {
    prediction = CLASS_LABELS[best];
  }

  return best;
}

static bool beginGesture() {
  bool labelFound = false;
  for (int i = 0; i < CLASSES; ++i) {
    if (i != BACKGROUND_INDEX && strcmp(CLASS_LABELS[i], UNLOCK_GESTURE) == 0) labelFound = true;
  }
  if (!labelFound) return gestureError("UNLOCK_GESTURE does not match a trained hand label");
  if(!initCameraFrontend()) return gestureError("Camera init failed: check OPI PSRAM, board and camera connection.");
  auto *backing=(uint8_t*)heap_caps_malloc(ARENA_BYTES+15,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  if(!backing)return gestureError("Arena allocation failed");
  // Align the tensor arena to a 16-byte boundary
  auto *arena=(uint8_t*)((uintptr_t(backing)+15)&~uintptr_t(15));
  const tflite::Model *model=tflite::GetModel(g_model);
  if(model->version()!=TFLITE_SCHEMA_VERSION)return gestureError("Model schema mismatch");
  static tflite::MicroMutableOpResolver<8> resolver;
  if(resolver.AddConv2D()!=kTfLiteOk || resolver.AddMaxPool2D()!=kTfLiteOk ||
     resolver.AddFullyConnected()!=kTfLiteOk || resolver.AddSoftmax()!=kTfLiteOk ||
     resolver.AddReshape()!=kTfLiteOk || resolver.AddQuantize()!=kTfLiteOk ||
     resolver.AddDequantize()!=kTfLiteOk || resolver.AddMean()!=kTfLiteOk)return gestureError("Operator registration failed");
  static tflite::MicroInterpreter instance(model,resolver,arena,ARENA_BYTES);interpreter=&instance;
  if(interpreter->AllocateTensors()!=kTfLiteOk)return gestureError("AllocateTensors failed; read preceding runtime error");
  inputTensor=interpreter->input(0);outputTensor=interpreter->output(0);
  if(inputTensor->type!=kTfLiteInt8 || outputTensor->type!=kTfLiteInt8)return gestureError("Expected INT8 model");
  if(inputTensor->dims->size!=4 || inputTensor->dims->data[0]!=1 || inputTensor->dims->data[1]!=64 ||
     inputTensor->dims->data[2]!=64 || inputTensor->dims->data[3]!=1 || inputTensor->bytes!=4096)return gestureError("Expected [1,64,64,1]");
  if(CLASSES!=4 || outputTensor->dims->size!=2 || outputTensor->dims->data[0]!=1 ||
     outputTensor->dims->data[1]!=CLASSES || outputTensor->bytes!=CLASSES)return gestureError("Expected four-class output");
  if(BACKGROUND_INDEX<0 || BACKGROUND_INDEX>=CLASSES || !isfinite(INPUT_SCALE) || INPUT_SCALE<=0 ||
     !isfinite(OUTPUT_SCALE) || OUTPUT_SCALE<=0 || !isfinite(ACCEPT_THRESHOLD) || ACCEPT_THRESHOLD<0 || ACCEPT_THRESHOLD>1)return gestureError("Invalid model config");
  if(inputTensor->params.zero_point!=INPUT_ZERO_POINT || outputTensor->params.zero_point!=OUTPUT_ZERO_POINT ||
     fabsf(inputTensor->params.scale-INPUT_SCALE)>fmaxf(1e-9f,INPUT_SCALE*1e-5f) ||
     fabsf(outputTensor->params.scale-OUTPUT_SCALE)>fmaxf(1e-9f,OUTPUT_SCALE*1e-5f))return gestureError("Model headers disagree");
  Serial.printf("# MODEL bytes=%u arena_capacity=%u arena_used=%u\n",g_model_len,(unsigned)ARENA_BYTES,(unsigned)interpreter->arena_used_bytes());
  return true;
}

static bool inferUnlockGesture() {
  camera_fb_t *fb = captureFresh();
  if (!fb) return false;
  bool ok = prepareImage(fb);
  esp_camera_fb_return(fb);  // Release the camera buffer after preprocessing
  if (!ok || !fillModelInput()) return false;
  if (interpreter->Invoke() != kTfLiteOk) return false;  // Treat inference failures as non-matches
  float scores[CLASSES] = {};
  const char *prediction = nullptr;
  int best = decodePrediction(scores, prediction);
  if (best < 0 || best >= CLASSES || !prediction) return false;
  Serial.printf("Gesture: %s score=%.3f\n", prediction, scores[best]);
  return best != BACKGROUND_INDEX && strcmp(prediction, UNLOCK_GESTURE) == 0 &&
         scores[best] > UNLOCK_MIN_SCORE && scores[best] >= ACCEPT_THRESHOLD;
}
#else
#define WATCH_HAS_MODEL 0
static bool beginGesture() {
  Serial.println("Gesture model missing: add model_data.h and model_config.h to the smartwatch sketch folder from Lab 7 Task 2.");
  return false;
}
static bool inferUnlockGesture() { return false; }
#endif
