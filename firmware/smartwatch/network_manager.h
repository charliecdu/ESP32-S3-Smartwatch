/*
 * network_manager.h
 * Background Wi-Fi, weather updates, and local audio endpoints
 */
#pragma once
#include <WiFi.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <WebServer.h>
#include <lwip/sockets.h>
#include <errno.h>
#include <time.h>
#include "state_machine.h"
#include "web_interface.h"

static WebServer server(80);
static bool networkTaskReady = false;

static bool fetchWeather(Weather &result) {
  if (String(WEATHER_API_KEY).startsWith("YOUR_")) return false;
  NetworkClientSecure client;
  client.useBuiltinCACertBundle();  // Verify the weather service certificate
  client.setHandshakeTimeout(5);
  HTTPClient http;
  String url = String("https://api.weatherapi.com/v1/current.json?key=") + WEATHER_API_KEY + "&q=" + WEATHER_QUERY + "&aqi=no";
  if (!http.begin(client, url)) return false;
  http.setConnectTimeout(4000);
  http.setTimeout(4000);
  http.addHeader("User-Agent", "XIAO-Smartwatch/1.0");
  int status = http.GET();
  String payload;
  if (status == HTTP_CODE_OK) payload = http.getString();
  http.end();
  return status == HTTP_CODE_OK && payload.length() && parseWeather(payload, result);
}

static void handleRoot() {
  server.sendHeader("Cache-Control", "no-store");
  // Serve the page directly from flash
  server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
}

static void handleStatus() {
  JsonDocument doc;
  {
    WatchGuard lock;
    bool access = hasAccess(state);  // Apply the same access rules as the audio endpoint
    doc["state"] = stateName(state);
    doc["access"] = access;
    doc["transfer"] = transferActive;
    if (access && savedPCMBytes) {
      char name[32];
      recordingFilename(name, sizeof(name), recordingNumber);
      doc["filename"] = name;
      doc["duration"] = (double)savedPCMBytes / (SAMPLE_RATE * sizeof(int16_t));
    } else {
      doc["filename"] = "";
      doc["duration"] = 0;
    }
  }
  String body;
  serializeJson(doc, body);
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", body);
}

static bool sendBounded(int fd, const uint8_t *data, size_t length, uint32_t started) {
  while (length && millis() - started < TRANSFER_TIMEOUT_MS) {
    // Use bounded chunks without blocking on the socket
    int sent = send(fd, data, min(length, (size_t)4096), MSG_DONTWAIT);
    if (sent > 0) {
      data += sent;
      length -= sent;
    } else if (sent == 0 || (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)) {
      return false;
    }
    vTaskDelay(1);
  }
  return length == 0;
}

static void handleAudio(bool download) {
  uint8_t *data = nullptr;
  size_t length = 0;
  uint32_t number = 0;
  int error = 0;
  {
    WatchGuard lock;
    if (state == LOCKED) error = 403;
    else if (state == RECORDING || startPending || transferActive) error = 409;
    else if (!hasAccess(state)) error = 403;
    else if (!savedPCMBytes) error = 404;
    else {
      transferActive = true;  // Prevent recording replacement during the transfer
      lastActivity = millis();
      data = savedWav;
      length = WAV_HEADER_BYTES + savedPCMBytes;
      number = recordingNumber;
    }
  }
  if (error) {
    server.sendHeader("Cache-Control", "no-store");
    server.send(error, "text/plain", error == 403 ? "Locked" : error == 409 ? "Busy" : "No recording");
    return;
  }
  char name[32], header[320];
  recordingFilename(name, sizeof(name), number);
  int n = snprintf(header, sizeof(header), "HTTP/1.1 200 OK\r\nContent-Type: audio/wav\r\nContent-Length: %u\r\nContent-Disposition: %s; filename=\"%s\"\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n",
                   (unsigned)length, download ? "attachment" : "inline", name);
  auto client = server.client();
  uint32_t started = millis();
  bool success = n > 0 && (size_t)n < sizeof(header) &&
                 sendBounded(client.fd(), (const uint8_t *)header, n, started) &&
                 sendBounded(client.fd(), data, length, started);
  client.stop();
  {
    WatchGuard lock;
    transferActive = false;
    lastActivity = millis();
  }
  Serial.println(success ? "Audio transfer sent." : "Audio transfer failed; file retained.");
}

static void networkTask(void *) {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  bool configured = !String(WIFI_SSID).startsWith("YOUR_") && strlen(WIFI_SSID) > 0;
  if (configured) WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  else Serial.println("Enter Wi-Fi and WeatherAPI settings in system_config.h.");
  configTzTime(WATCH_TIMEZONE, "pool.ntp.org", "time.nist.gov");
  server.on("/", HTTP_GET, handleRoot);
  server.on("/index.html", HTTP_GET, handleRoot);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/audio.wav", HTTP_GET, []() { handleAudio(false); });
  server.on("/download.wav", HTTP_GET, []() { handleAudio(true); });
  server.onNotFound([]() { server.send(404, "text/plain", "Not found"); });
  server.begin();
  uint32_t lastAttempt = millis() - WEATHER_REFRESH_MS;
  bool wasConnected = false;
  for (;;) {
    bool connected = WiFi.status() == WL_CONNECTED;
    if (connected && !wasConnected) Serial.printf("Open http://%s/\n", WiFi.localIP().toString().c_str());
    wasConnected = connected;
    if (connected) {
      server.handleClient();
      uint32_t now = millis();
      bool request = false;
      if (time(nullptr) > 1704067200 && now - lastAttempt >= WEATHER_REFRESH_MS) {
        WatchGuard lock;
        if (state != RECORDING && !startPending && !transferActive) {
          weatherBusy = true;  // Defer recording startup until the request finishes
          request = true;
          lastAttempt = now;
        }
      }
      if (request) {
        Weather result;
        bool ok = fetchWeather(result);
        WatchGuard lock;
        if (ok) {
          lastWeather = result;
          haveWeather = true;
          lastWeatherSuccess = millis();
        }
        weatherFailed = !ok;
        weatherBusy = false;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

static bool beginNetwork() {
  return xTaskCreate(networkTask, "watch-network", 12288, nullptr, 1, nullptr) == pdPASS;
}
