// AuditMonitor.cpp
// George Lake
// Fall 2025
//
// Polling task for AuditValue and AuthorizedUser
// Refer to AuditMonitor.hpp for notes


#include "AuditMonitor.hpp"
#include "TagReads.hpp"
#include "ExperimentInstrumentation.hpp"
#include "EnipClient.hpp"
#include "json_log.hpp"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "AUDIT_MON";

// Global poll sequence counter
static long g_poll_seq = 0;

struct AuditCfg {
    EnipClient* enip;
    const char* audit_tag;
    const char* auth_tag;
    const char* kp_tag;
    const char* ki_tag;
    const char* kd_tag;
    uint32_t poll_ms;
};

static bool reconnect_enip(EnipClient& enip) {
    enip.close();
    for (;;) {
        ESP_LOGW(TAG, "Attempting ENIP reconnect...");
        if (enip.connect_tcp() && enip.register_session()) {
            ESP_LOGI(TAG, "ENIP reconnect successful");
            return true;
        }
        ESP_LOGW(TAG, "ENIP reconnect failed; retrying in 1s");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static bool nearly_equal(float a, float b, float eps = 1e-6f) {
    //
    //
    //
    float diff = a - b;
    if (diff < 0) diff = -diff;
    return diff <= eps;
}

static void audit_task(void* arg) {
    //
    //
    //
    AuditCfg cfg = *static_cast<AuditCfg*>(arg);
    delete static_cast<AuditCfg*>(arg); // free the heap copy

    // Baselines --------------------------------------------------------------------------------------------
    int64_t last_audit = 0;
    float last_kp = 0.0f, last_ki = 0.0f, last_kd = 0.0f;

    bool have_audit_baseline = false;
    bool have_pid_baseline = false;
    bool baseline_marked = false;

    int consecutive_failures = 0;

    for (;;) {
        int64_t audit = 0;
        int32_t auth = 0;
        float kp = 0.0f, ki = 0.0f, kd = 0.0f;

        bool ok_audit   = read_lint(*cfg.enip, cfg.audit_tag,   audit);
        bool ok_auth    = read_dint(*cfg.enip, cfg.auth_tag,    auth);
        bool ok_kp      = read_real(*cfg.enip, cfg.kp_tag,      kp);
        bool ok_ki      = read_real(*cfg.enip, cfg.ki_tag,      ki);
        bool ok_kd      = read_real(*cfg.enip, cfg.kd_tag,      kd);

        if (!ok_audit || !ok_auth || !ok_kp || !ok_ki || !ok_kd) {
            Experiment::record_read_failure();

            ++consecutive_failures;

            ESP_LOGW(TAG,
                     "Read error (audit_ok=%d auth_ok=%d kp=%d ki=%d kd=%d). "
                     "Fail count=%d",
                     ok_audit, ok_auth, ok_kp, ok_ki, ok_kd, consecutive_failures);

            // After N consecutive failures, attempt a full ENIP reconnect
            if (consecutive_failures >= 5) {
                ESP_LOGW(TAG, "Persistent failures; attempting ENIP reconnect.");
                reconnect_enip(*cfg.enip);
                consecutive_failures = 0;
            }

            vTaskDelay(pdMS_TO_TICKS(cfg.poll_ms));
            continue;
        }
        // Successful read: reset failure counter
        consecutive_failures = 0;

        // AuditValue baseline / change detection -------------------------------------------------------------
        if (!have_audit_baseline) {
            last_audit = audit;
            have_audit_baseline = true;
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

            // Instrumentation: record classification
            Experiment::record_audit_change(auth != 0);

            last_audit = audit;
        }

        // PID Baseline / change detection ---------------------------------------------------------------------
        if (!have_pid_baseline) {
            last_kp = kp;
            last_ki = ki;
            last_kd = kd;
            have_pid_baseline = true;
            ESP_LOGI(TAG, "Baseline PID: Kp=%.6f Ki=%.6f Kd=%.6f", last_kp, last_ki, last_kd);
        } else {
            bool kp_changed = !nearly_equal(kp, last_kp);
            bool ki_changed = !nearly_equal(ki, last_ki);
            bool kd_changed = !nearly_equal(kd, last_kd);

            if (kp_changed || ki_changed || kd_changed) {
                if (auth == 0) {
                    ESP_LOGW(TAG, 
                        "UNAUTHORIZED_PID_CHANGE: "
                        "Kp %.6f->%.6f, Ki %.6f->%.6f, Kd %.6f->%.6f (auth=%d)",
                        last_kp, kp, last_ki, ki, last_kd, kd, (int)auth);
                } else {
                    ESP_LOGI(TAG,
                        "AUTHORIZED_PID_CHANGE: "
                        "Kp %.6f->%.6f, Ki %.6f->%.6f, Kd %.6f->%.6f (auth=%d)",
                        last_kp, kp, last_ki, ki, last_kd, kd, (int)auth);
                }
                // Instrumentation: record classification
                Experiment::record_pid_change(auth != 0);

                last_kp = kp;
                last_ki = ki;
                last_kd = kd;
            }
        }
        // ----------------------------------------------------------------------------------------------------
        // If both baselines are set and it has not been marked yet, mark baseline time
        if (!baseline_marked && have_audit_baseline && have_pid_baseline) {
            Experiment::mark_baseline_established();
            baseline_marked = true;
        }

                // ----------------------------------------------------------------------------------------------------
        // JSON logging: emit one LogEntry per successful poll
        {
            LogEntry log;

            // Fill scenario + timing + metadata (esp/plc versions, poll_period_ms)
            Experiment::fill_log_entry_context(log);

            // Poll sequence number (monotonic per firmware run)
            log.poll_seq = g_poll_seq++;

            // --- Current values ---
            log.current.AuditValue      = std::to_string(static_cast<long long>(audit));
            log.current.AuthorizedUser  = std::to_string(static_cast<int>(auth));
            log.current.Kp              = kp;
            log.current.Ki              = ki;
            log.current.Kd              = kd;

            // Status tags are not yet read here; mark as NA for now
            log.current.ControllerStatus = "NA";
            log.current.AuxStatus        = "NA";
            log.current.ExperimentMarker = "NA";

            // --- Baseline values ---
            if (have_audit_baseline) {
                log.baseline.AuditValue = std::to_string(static_cast<long long>(last_audit));
            } else {
                log.baseline.AuditValue = "NA";
            }

            // You do not track a baseline AuthorizedUser yet
            log.baseline.AuthorizedUser = "NA";

            if (have_pid_baseline) {
                log.baseline.Kp = last_kp;
                log.baseline.Ki = last_ki;
                log.baseline.Kd = last_kd;
            } else {
                log.baseline.Kp = 0.0;
                log.baseline.Ki = 0.0;
                log.baseline.Kd = 0.0;
            }

            log.baseline.ControllerStatus = "NA";
            log.baseline.AuxStatus        = "NA";

            // --- Comparison data ---
            bool audit_changed = have_audit_baseline && (audit != last_audit);
            bool kp_changed    = have_pid_baseline && !nearly_equal(kp, last_kp);
            bool ki_changed    = have_pid_baseline && !nearly_equal(ki, last_ki);
            bool kd_changed    = have_pid_baseline && !nearly_equal(kd, last_kd);

            log.comparison.any_change =
                audit_changed || kp_changed || ki_changed || kd_changed;

            log.comparison.authorized_change =
                log.comparison.any_change && (auth != 0);

            log.comparison.unauthorized_change =
                log.comparison.any_change && (auth == 0);

            // List of changed fields
            log.comparison.changed_fields.clear();
            if (audit_changed) log.comparison.changed_fields.push_back("AuditValue");
            if (kp_changed)    log.comparison.changed_fields.push_back("Kp");
            if (ki_changed)    log.comparison.changed_fields.push_back("Ki");
            if (kd_changed)    log.comparison.changed_fields.push_back("Kd");

            // Per-field flags
            log.comparison.chg_AuditValue       = audit_changed;
            log.comparison.chg_AuthorizedUser   = false;  // no baseline auth yet
            log.comparison.chg_Kp               = kp_changed;
            log.comparison.chg_Ki               = ki_changed;
            log.comparison.chg_Kd               = kd_changed;
            log.comparison.chg_ControllerStatus = false;
            log.comparison.chg_AuxStatus        = false;

            // Deltas for PID gains
            log.comparison.delta_Kp = kp_changed ? (kp - last_kp) : 0.0;
            log.comparison.delta_Ki = ki_changed ? (ki - last_ki) : 0.0;
            log.comparison.delta_Kd = kd_changed ? (kd - last_kd) : 0.0;

            // --- Comm status (success path only for now) ---
            log.comm.comm_status = "OK";
            log.comm.read_ok     = true;
            log.comm.retry_count = 0;  // we'll wire real retries later

            // --- Ground truth (to be filled offline / later tooling) ---
            log.groundtruth.t_change_groundtruth_iso = "NA";
            log.groundtruth.t_change_marker_seen     = "NA";

            // Emit one JSON line
            Experiment::emit_log_entry(log);
        }
        
        vTaskDelay(pdMS_TO_TICKS(cfg.poll_ms));
    }
}

void start_audit_monitor(EnipClient* enip,
                         const char* audit_tag,
                         const char* authorized_tag,
                         const char* kp_tag,
                         const char* ki_tag,
                         const char* kd_tag,
                         uint32_t poll_ms) {
    //
    //
    //
    auto* cfg = new AuditCfg{enip, audit_tag, authorized_tag, kp_tag, ki_tag, kd_tag, poll_ms};
    xTaskCreate(audit_task, "audit_task", 4096, cfg, 5, nullptr);
}
