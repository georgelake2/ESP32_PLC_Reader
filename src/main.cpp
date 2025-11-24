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
//      RQ2: What are the latency and reliability trade-offs compared to built-in PLC tools?  
//          Notes: 
//          ESP can be focused on security, PLC focused on process control
//          Create a separate security network that can be monitored for security risks
//          
//      RQ3: How reilient is the system to false positives?
//


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include <array>
#include <cstdio>

#include "WifiManager.hpp"
#include "EnipClient.hpp"
#include "CipCodec.hpp"
#include "EpochTime.hpp"
#include "AuditMonitor.hpp"
#include "TagReads.hpp"
#include "ExperimentInstrumentation.hpp"

// ---------------- User config (can be overridden by -D flags) ----------------
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
// -----------------------------------------------------------------------------

static const char* TAG = "MAIN_APP";

extern "C" void app_main(void) {
    // Quiet logs globally; keep tag at INFO.
    esp_log_level_set("*", ESP_LOG_WARN);
    esp_log_level_set(TAG, ESP_LOG_INFO);

    // Monitor Audit Warnings
    esp_log_level_set("AUDIT_MON", ESP_LOG_INFO);

    // Initialize experiment instrumentation (scenario label)
    // Change scenario label as appropriate
    Experiment::init("S1");

    // NVS + Wi-Fi
    ESP_ERROR_CHECK(nvs_flash_init());
    if (WifiManager::init_sta(WIFI_SSID, WIFI_PASS, 15000) != ESP_OK) {
        ESP_LOGE(TAG, "Wi-Fi not ready; aborting.");
        return;
    }
    ESP_LOGI(TAG, "Wi-Fi connected");

    // ENIP session --------------------------------------------------------------------------
    EnipClient enip(PLC_IP, PLC_PORT);
    if (!enip.connect_tcp())      { ESP_LOGE(TAG, "TCP connect failed"); return; }
    if (!enip.register_session()) { ESP_LOGE(TAG, "RegisterSession failed"); enip.close(); return; }

    // ControllerStatus (DINT) ----------------------------------------------------------------
    int32_t ctrl = -1;
    if (!read_dint(enip, WDG_BASE ".ControllerStatus", ctrl)) {
        ESP_LOGE(TAG, "Read failed: %s.ControllerStatus", WDG_BASE);
        enip.close(); return;
    }
    ESP_LOGI(TAG, "ControllerStatus = %ld", (long)ctrl);

    // DateTime[0..6] (DINT[7]) → epoch ms -----------------------------------------------------
    std::array<int32_t,7> dt{};
    if (!read_dint_array7(enip, WDG_BASE ".DateTime", dt)) {
        ESP_LOGE(TAG, "Read failed: %s.DateTime (DINT[7])", WDG_BASE);
        enip.close(); return;
    }
    ESP_LOGI(TAG, "PLC DateTime: %ld-%02ld-%02ld %02ld:%02ld:%02ld usec=%ld",
             (long)dt[0], (long)dt[1], (long)dt[2], (long)dt[3],
             (long)dt[4], (long)dt[5], (long)dt[6]);

    const auto plc_ts   = EpochTime::fromArray(dt);
    const int64_t epoch = EpochTime::toEpochMs(plc_ts, PLC_TZ_OFFSET_MINUTES);
    if (epoch >= 0) ESP_LOGI(TAG, "PLC epoch (ms) = %lld", (long long)epoch);

    // AuditValue (LINT) once ---------------------------------------------------------------------
    int64_t audit = 0;
    if (read_lint(enip, WDG_BASE ".AuditValue", audit)) {
        ESP_LOGI(TAG, "AuditValue = %lld (0x%016llx)",
                 (long long)audit, (unsigned long long)audit);
    }

    // PID Tuning Constants -----------------------------------------------------------------------
    float kp = 0.0f, ki = 0.0f, kd = 0.0f;
    bool ok_kp = read_real(enip, WDG_BASE ".WDG_Kp", kp);
    bool ok_ki = read_real(enip, WDG_BASE ".WDG_Ki", ki);
    bool ok_kd = read_real(enip, WDG_BASE ".WDG_Kd", kd);

    if (ok_kp && ok_ki && ok_kd) {
        ESP_LOGI(TAG, "WDG PID gains: Kp=%.3f Ki=%.3f Kd=%.3f", kp, ki, kd);
    } else {
        ESP_LOGW(TAG, "PID read failed (Kp=%d Ki=%d Kd=%d)", ok_kp, ok_ki, ok_kd);
    }

    // Start Audit monitor --------------------------------------------------------------------------------
    start_audit_monitor(&enip, 
                        WDG_BASE ".AuditValue", 
                        WDG_BASE ".AuthorizedUser",
                        WDG_BASE ".WDG_Kp",
                        WDG_BASE ".WDG_Ki",
                        WDG_BASE ".WDG_Kd", 
                        /*poll_ms=*/ 200);

    // Periodically dump an audit summary every 10 seconds
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        Experiment::dump_summary();
    }

    // Idle loop
    while (true) vTaskDelay(pdMS_TO_TICKS(1000));

    // Complete
    enip.close();
}
 
// ---------------------------------------------------------------------------------------------------------------------------
