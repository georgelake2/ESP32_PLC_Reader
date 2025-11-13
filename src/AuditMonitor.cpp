// AuditMonitor.cpp
// George Lake
// Fall 2025
//
// Polling task for AuditValue and AuthorizedUser
// Refer to AuditMonitor.hpp for notes


#include "AuditMonitor.hpp"
#include "TagReads.hpp"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "AUDIT_MON";

struct AuditCfg {
    EnipClient* enip;
    const char* audit_tag;
    const char* auth_tag;
    uint32_t poll_ms;
};

static void audit_task(void* arg) {
    AuditCfg cfg = *static_cast<AuditCfg*>(arg);
    delete static_cast<AuditCfg*>(arg); // free the heap copy

    int64_t last_audit = 0;
    bool have_baseline = false;

    for (;;) {
        int64_t audit = 0;
        int32_t auth = 0;

        bool ok_audit = read_lint(*cfg.enip, cfg.audit_tag, audit);
        bool ok_auth  = read_dint(*cfg.enip, cfg.auth_tag,  auth);

        if (!ok_audit || !ok_auth) {
            ESP_LOGW(TAG, "Read error (audit_ok=%d auth_ok=%d). Retrying...", ok_audit, ok_auth);
            vTaskDelay(pdMS_TO_TICKS(cfg.poll_ms));
            continue;
        }

        if (!have_baseline) {
            last_audit = audit;
            have_baseline = true;
            ESP_LOGI(TAG, "Baseline AuditValue = %lld (0x%016llx)",
                     (long long)audit, (unsigned long long)audit);
        } else if (audit != last_audit) {
            if (auth == 0) {
                ESP_LOGW(TAG,
                    "UNAUTHORIZED_CHANGE: AuditValue %lld->%lld (0x%016llx->0x%016llx), auth=%d",
                    (long long)last_audit, (long long)audit,
                    (unsigned long long)last_audit, (unsigned long long)audit,
                    (int)auth);
            } else {
                ESP_LOGI(TAG,
                    "AUTHORIZED_CHANGE: AuditValue %lld->%lld (auth=%d).",
                    (long long)last_audit, (long long)audit, (int)auth);
            }
            last_audit = audit;
        }

        vTaskDelay(pdMS_TO_TICKS(cfg.poll_ms));
    }
}

void start_audit_monitor(EnipClient* enip,
                         const char* audit_tag,
                         const char* authorized_tag,
                         uint32_t poll_ms) {
    auto* cfg = new AuditCfg{enip, audit_tag, authorized_tag, poll_ms};
    xTaskCreate(audit_task, "audit_task", 4096, cfg, 5, nullptr);
}
