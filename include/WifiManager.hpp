// WifiManager.hpp
// George Lake
// Fall 2025
// 
// Helper to bring up ESP32 station mode WiFi


#pragma once
#include "esp_err.h"

namespace WifiManager {
  // Brings up STA and blocks until IP or timeout.
  // Returns ESP_OK on success, ESP_ERR_TIMEOUT on timeout, or other ESP-IDF error.
  esp_err_t init_sta(const char* ssid, const char* pass, int timeout_ms = 15000);
}
