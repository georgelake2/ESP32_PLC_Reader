// WifiManager.cpp
// George Lake
// Fall 2025
//
// Bring up a WiFi STA and connect to the provided SSID/password


#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include <cstring>

#include "WifiManager.hpp"

// ----------------------------------------------VARIABLES ------------------------------------------------------
static const char* TAG = "WIFI";
static EventGroupHandle_t s_evt = nullptr;
static constexpr int WIFI_CONNECTED_BIT = BIT0;


// ----------------------------------------------METHODS -------------------------------------------------------
static void wifi_event_handler(void*, esp_event_base_t base, int32_t id, void* data) {
    //
    // WIFI EVENT HANDLER
    //
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        auto* e = static_cast<ip_event_got_ip_t*>(data);
        ESP_LOGI(TAG, "IP " IPSTR, IP2STR(&e->ip_info.ip));
        if (s_evt) xEventGroupSetBits(s_evt, WIFI_CONNECTED_BIT);
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
    }
}

esp_err_t WifiManager::init_sta(const char* ssid, const char* pass, int timeout_ms) {
    //
    //
    //
    
    // Netif + event loop
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    // Wi-Fi init
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Register concise handler
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, nullptr, nullptr));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, nullptr, nullptr));

    // Config STA (defensively NUL-terminate)
    wifi_config_t wc{};
    std::memset(&wc, 0, sizeof(wc));
    std::strncpy(reinterpret_cast<char*>(wc.sta.ssid),     ssid, sizeof(wc.sta.ssid) - 1);
    std::strncpy(reinterpret_cast<char*>(wc.sta.password), pass, sizeof(wc.sta.password) - 1);
    wc.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wc));

    // One concise line telling what we’re doing:
    ESP_LOGI(TAG, "Connecting to '%s'…", reinterpret_cast<char*>(wc.sta.ssid));

    ESP_ERROR_CHECK(esp_wifi_start());

    // Block until IP (or timeout)
    s_evt = xEventGroupCreate();
    // Ensure a connect attempt is in flight
    esp_wifi_connect();

    EventBits_t bits = xEventGroupWaitBits(
        s_evt, WIFI_CONNECTED_BIT, pdFALSE, pdFALSE, pdMS_TO_TICKS(timeout_ms));

    vEventGroupDelete(s_evt);
    s_evt = nullptr;

    if (bits & WIFI_CONNECTED_BIT) {
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Timed out waiting for Wi-Fi/IP.");
        return ESP_ERR_TIMEOUT;
    }
}