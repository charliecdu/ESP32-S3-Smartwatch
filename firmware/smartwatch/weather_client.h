/*
 * weather_client.h
 * Weather response parsing and validation before publication
 */
#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <math.h>

struct Weather {
  float temperatureC = NAN;  // Missing temperatures start as invalid
  String condition;
  String place;
};

inline bool parseWeather(const String& payload, Weather& output) {
  JsonDocument doc;
  if (deserializeJson(doc, payload.c_str())) return false;

  // Reject responses containing an error object
  if (doc["error"].is<JsonObject>()) return false;

  // Check required field types
  if (!doc["current"]["temp_c"].is<float>()) return false;
  if (!doc["current"]["condition"]["text"].is<const char*>()) return false;

  // Store values in a local Weather
  Weather result;
  result.temperatureC = doc["current"]["temp_c"].as<float>();
  result.condition = doc["current"]["condition"]["text"].as<const char*>();
  result.place = doc["location"]["name"] | "";  // Allow responses without a location name

  // Validate temperature
  if (!isfinite(result.temperatureC)) return false;

  // Validate condition
  result.condition.trim();
  if (result.condition.length() == 0) return false;

  // Update output only after all checks pass
  output = result;
  return true;
}
