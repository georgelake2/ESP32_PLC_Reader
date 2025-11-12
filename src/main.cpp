// main.cpp
// George Lake
// Fall 2025
//
// Connect ESP to Wifi,
// Connect to PLC Via EthernetIP
// Read PLC values via CIP
// 
// Inputs to ESP32 from PLC
//      WDG_Status_Instance.DateTime[0..6]      (DINT)
//      WDG_Status_Instance.AuditValue          (LINT)
//      WDG_Status_Instance.ControllerStatus    (DINT)
//
// Goals: 
//      RQ1: Can ESP32 detect unauthorized PLC logic or parameter changes?
//          - Need to monitor PLC AuditValue, and PID Parameters
//
//      RQ2: What are the latency and rliability trade-offs compared to built-in PLC tools?    
//          
//      RQ3: How reilient is the system to false positives?
//


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include <array>
#include <vector>
#include <cstdio>

#include "WifiManager.hpp"
#include "EnipClient.hpp"
#include "CipCodec.hpp"
#include "EpochTime.hpp"

// ----------------------------------- KNOBS --------------------------------------------------
#ifndef WIFI_SSID
#define WIFI_SSID "YOUR_SSID"
#endif

#ifndef WIFI_PASS
#define WIFI_PASS "YOUR_PASS"
#endif

#ifndef PLC_IP
#define PLC_IP "10.100.10.185"
#endif

#ifndef PLC_PORT
#define PLC_PORT 44818
#endif

#ifndef WDG_BASE
#define WDG_BASE "WDG_Status_Instance"
#endif

#ifndef PLC_TZ_OFFSET_MINUTES
#define PLC_TZ_OFFSET_MINUTES 0
#endif


// ------------------------------------- VARIABLES ------------------------------------------------
static const char* TAG = "MAIN_APP";

// -------------------------------------- HELPERS: READ TAG VALUES --------------------------------
static bool read_tag_scalar(EnipClient& enip, const char* tag, Cip::Value& out) {
    //
    // SCALAR
    //
    auto cip = Cip::build_read_request(tag, 1);
    auto rr  = Cip::wrap_sendrr(cip);
    std::vector<uint8_t> rr_body, cip_reply;
    if (!enip.send_rr_data(rr, rr_body)) return false;
    if (!Cip::extract_cip_from_rr(rr_body, cip_reply)) return false;
    return Cip::parse_read_reply(cip_reply, out);
}

static bool read_dint(EnipClient& enip, const char* tag, int32_t& out) {
    //
    // DINT
    //
    Cip::Value v{};
    if (!read_tag_scalar(enip, tag, v)) return false;
    if (v.type != Cip::Type::DINT) return false;
    out = v.v.i32; return true;
}

static bool read_lint(EnipClient& enip, const char* tag, int64_t& out) {
    //
    // LINT
    //
    Cip::Value v{};
    if (!read_tag_scalar(enip, tag, v)) return false;
    if (v.type != Cip::Type::LINT) return false;
    out = v.v.i64; return true;
}

static bool read_dint_array7(EnipClient& enip, const char* base, std::array<int32_t,7>& out) {
    //
    // DINT[7] ARRAY
    //
    auto cip = Cip::build_read_request(base, /*elements*/7);
    auto rr  = Cip::wrap_sendrr(cip);
    std::vector<uint8_t> rr_body, c;
    if (!enip.send_rr_data(rr, rr_body)) return false;
    if (!Cip::extract_cip_from_rr(rr_body, c)) return false;

    if (c.size() < 4 || (c[0] & 0x80) == 0) return false;
    uint8_t gen = c[2], ext = c[3];
    if (gen != 0) return false;
    size_t data_off = 4 + ext*2;
    if (c.size() < data_off + 2) return false;

    uint16_t type_id = c[data_off] | (c[data_off+1]<<8);
    if (type_id != 0x00C4) return false; // DINT
    size_t val_off = data_off + 2;
    size_t need   = 7 * 4;
    if (c.size() < val_off + need) return false;

    for (int i=0;i<7;++i) {
        size_t p = val_off + i*4;
        uint32_t u = (uint32_t)c[p] | ((uint32_t)c[p+1]<<8) | ((uint32_t)c[p+2]<<16) | ((uint32_t)c[p+3]<<24);
        out[i] = (int32_t)u;
    }
    return true;
}

// =============================================== MAIN =======================================================
extern "C" void app_main(void) {
    //
    //
    //

    // Limit the Wifi responses
    // Turn most components down to WARN globally
    esp_log_level_set("*", ESP_LOG_WARN);

    // Keep *your* tags chatty if you want
    esp_log_level_set("WIFI", ESP_LOG_INFO);
    esp_log_level_set("MAIN_APP", ESP_LOG_INFO);

    // Explicitly quiet the noisy ones
    esp_log_level_set("wifi", ESP_LOG_WARN);
    esp_log_level_set("wpa",  ESP_LOG_WARN);
    esp_log_level_set("phy",  ESP_LOG_WARN);
    esp_log_level_set("esp_netif_handlers", ESP_LOG_ERROR);  // or ESP_LOG_NONE
    esp_log_level_set("main_task", ESP_LOG_ERROR);

    // Initialize Non-Volatale Storage and WiFi
    ESP_ERROR_CHECK(nvs_flash_init());

    if (WifiManager::init_sta(WIFI_SSID, WIFI_PASS, 15000) != ESP_OK) {
    ESP_LOGE(TAG, "Wi-Fi not ready; aborting CIP.");
    return;
    }
    ESP_LOGI(TAG, "Wifi_Connected");
    vTaskDelay(pdMS_TO_TICKS(2000));

    // -------------------------------------- Start ENIP Session ------------------------------------------------ 
    EnipClient enip(PLC_IP, PLC_PORT);
    if (!enip.connect_tcp())        { ESP_LOGE(TAG, "TCP connect failed"); return; }
    if (!enip.register_session())   { ESP_LOGE(TAG, "RegisterSession failed"); enip.close(); return; }

    // 1) Sanity: prove scope with ControllerStatus (DINT)
    int32_t ctrl = -1;
    if (!read_dint(enip, WDG_BASE ".ControllerStatus", ctrl)) {
        ESP_LOGE(TAG, "Read failed: %s.ControllerStatus", WDG_BASE);
        enip.close(); return;
    }
    ESP_LOGI(TAG, "ControllerStatus = %ld", (long)ctrl);

    // 2) Read DateTime[0..6] as DINT[7]
    std::array<int32_t,7> dt{};
        if (!read_dint_array7(enip, WDG_BASE ".DateTime", dt)) {
        ESP_LOGE(TAG, "Read failed: %s.DateTime (DINT[7])", WDG_BASE);
        enip.close(); return;
    }
    ESP_LOGI(TAG, "PLC DateTime: %ld-%02ld-%02ld %02ld:%02ld:%02ld usec=%ld",
            (long)dt[0], (long)dt[1], (long)dt[2], (long)dt[3],
            (long)dt[4], (long)dt[5], (long)dt[6]);

    // 3) Convert to epoch ms on ESP
    auto plc = EpochTime::fromArray(dt);
    int64_t epoch_ms = EpochTime::toEpochMs(plc, PLC_TZ_OFFSET_MINUTES);
    if (epoch_ms >= 0) {
        ESP_LOGI(TAG, "PLC epoch (ms) = %lld", (long long)epoch_ms);
    }
    else {
        ESP_LOGW(TAG, "Invalid PLC time—conversion skipped.");
    }

    // 4) AuditValue (LINT)
    int64_t audit = 0;
    if (read_lint(enip, WDG_BASE ".AuditValue", audit)) {
        ESP_LOGI(TAG, "AuditValue = %lld (0x%016llx)", (long long)audit, (unsigned long long)audit);
    } else {
        ESP_LOGW(TAG, "Read failed: %s.AuditValue", WDG_BASE);
    }

    // 5) DONE with ENIP
    enip.close();
    ESP_LOGI(TAG, "Done.");
    }

// ---------------------------------------------------------------------------------------------------------------------------
