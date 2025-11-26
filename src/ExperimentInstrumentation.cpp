// ExperimentInstrumentation.cpp
// George Lake
// Fall 2025

#include "ExperimentInstrumentation.hpp"
#include "EpochTime.hpp"
#include "json_log.hpp"
#include "json_encode.hpp"
#include "iso8601.hpp"

#include "esp_log.h"
#include <stdio.h>
#include <string>
#include <inttypes.h>

namespace {
    static const char* TAG = "EXPERIMENT";
    // static const char* CSV_TAG = "EXPCSV";

    // Single global metrics instance for this firmware
    ExperimentMetrics g_metrics;

    // Track whether currently in a "comm fault" interval
    bool g_comm_fault_active = false;
    int64_t g_comm_fault_start_ms = 0;

    inline int64_t now_ms() {
        // Monotonic ms from ESP boot
        return EpochTime::espNowMs();
    }

    // Helper: set first_detection_ms if not set yet
    void mark_first_detection_if_needed() {
        if (g_metrics.first_detection_ms < 0) {
            g_metrics.first_detection_ms = now_ms();
        }
    }
} // anonymous namespace

namespace Experiment {
        void init (const char*  scenario_id,
                    const char* scenario_variant,
                    int         trial_id,
                    bool        change_expected,
                    const char* change_type,
                    uint32_t    poll_period_ms) {
            // Initialize metrics and scenario label
            g_metrics = ExperimentMetrics{};
            g_metrics.scenario_id           = scenario_id;
            g_comm_fault_active             = false;
            g_comm_fault_start_ms           = 0;
            g_metrics.scenario_variant      = scenario_variant;
            g_metrics.trial_id              = trial_id;
            g_metrics.change_expected       = change_expected;
            g_metrics.change_type           = change_type;
            g_metrics.poll_period_ms        = poll_period_ms;

            // Firmware version (UPDATE WITH ACTUAL)
            g_metrics.esp_firmware_version = "v11.11.11";
            g_metrics.plc_firmware_version = "37.11.11";

            ESP_LOGI(TAG, "Experiment initialized (scenario='%s')", scenario_id ? scenario_id : "(null)");

            // // CSV-style header (optional)
            // ESP_LOGI(CSV_TAG,
            //         "type,t_ms,scenario,event,authorized,"
            //         "auth_audit,unauth_audit,auth_pid,unauth_pid,"
            //         "read_fail,comm_faults,baseline_ms,first_det_ms,comm_fault_ms");
        }

        void reset_metrics() {
            const char* id = g_metrics.scenario_id;
            g_metrics = ExperimentMetrics{};
            g_metrics.scenario_id = id;
            g_comm_fault_active = false;
            g_comm_fault_start_ms = 0;

            ESP_LOGI(TAG, "Experiment metrics reset (scenario='%s')", id ? id : "(null)");
        }

        void mark_baseline_established() {
            g_metrics.baseline_established_ms = now_ms();
            ESP_LOGI(TAG, "Baselines established at t=%lld ms",
                    (long long)g_metrics.baseline_established_ms);

            ESP_LOGI(TAG,
                    "EVENT,%lld,%s,BASELINE,-1,"
                    "%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%lld,%lld,%lld",
                    (long long)g_metrics.baseline_established_ms,
                    g_metrics.scenario_id ? g_metrics.scenario_id : "(null)",
                    g_metrics.authorized_audit_changes,
                    g_metrics.unauthorized_audit_changes,
                    g_metrics.authorized_pid_changes,
                    g_metrics.unauthorized_pid_changes,
                    g_metrics.read_failures,
                    g_metrics.comm_fault_intervals,
                    (long long)g_metrics.baseline_established_ms,
                    (long long)g_metrics.first_detection_ms,
                    (long long)g_metrics.total_comm_fault_dur_ms);
        }

        void record_audit_change(bool authorized) {
            if (authorized) {
                ++g_metrics.authorized_audit_changes;
            } else {
                ++g_metrics.unauthorized_audit_changes;
            }
            mark_first_detection_if_needed();

            int64_t t = now_ms();
            ESP_LOGI(TAG,
                    "EVENT,%lld,%s,AUDIT,%d,"
                    "%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%lld,%lld,%lld",
                    (long long)t,
                    g_metrics.scenario_id ? g_metrics.scenario_id : "(null)",
                    authorized ? 1 : 0,
                    g_metrics.authorized_audit_changes,
                    g_metrics.unauthorized_audit_changes,
                    g_metrics.authorized_pid_changes,
                    g_metrics.unauthorized_pid_changes,
                    g_metrics.read_failures,
                    g_metrics.comm_fault_intervals,
                    (long long)g_metrics.baseline_established_ms,
                    (long long)g_metrics.first_detection_ms,
                    (long long)g_metrics.total_comm_fault_dur_ms);
        }

        void record_pid_change(bool authorized) {
            if (authorized) {
                ++g_metrics.authorized_pid_changes;
            } else {
                ++g_metrics.unauthorized_pid_changes;
            }
            mark_first_detection_if_needed();

            int64_t t = now_ms();
            ESP_LOGI(TAG,
                    "EVENT,%lld,%s,PID,%d,"
                    "%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%lld,%lld,%lld",
                    (long long)t,
                    g_metrics.scenario_id ? g_metrics.scenario_id : "(null)",
                    authorized ? 1 : 0,
                    g_metrics.authorized_audit_changes,
                    g_metrics.unauthorized_audit_changes,
                    g_metrics.authorized_pid_changes,
                    g_metrics.unauthorized_pid_changes,
                    g_metrics.read_failures,
                    g_metrics.comm_fault_intervals,
                    (long long)g_metrics.baseline_established_ms,
                    (long long)g_metrics.first_detection_ms,
                    (long long)g_metrics.total_comm_fault_dur_ms);
        }

        void record_read_failure() {
            ++g_metrics.read_failures;

            int64_t t = now_ms();
            ESP_LOGI(TAG,
                "EVENT,%lld,%s,READ_FAIL,-1,"
                "%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%lld,%lld,%lld",
                (long long)t,
                g_metrics.scenario_id ? g_metrics.scenario_id : "(null)",
                g_metrics.authorized_audit_changes,
                g_metrics.unauthorized_audit_changes,
                g_metrics.authorized_pid_changes,
                g_metrics.unauthorized_pid_changes,
                g_metrics.read_failures,
                g_metrics.comm_fault_intervals,
                (long long)g_metrics.baseline_established_ms,
                (long long)g_metrics.first_detection_ms,
                (long long)g_metrics.total_comm_fault_dur_ms);
        }

        void record_comm_fault_start() {
            if (g_comm_fault_active) return; // already in fault
            g_comm_fault_active = true;
            g_comm_fault_start_ms = now_ms();
            ++g_metrics.comm_fault_intervals;
            ESP_LOGW(TAG, "COMM_FAULT_START at t = %lld ms", (long long)g_comm_fault_start_ms);

            ESP_LOGI(TAG,
                "EVENT,%lld,%s,COMM_FAULT_START,-1,"
                "%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%lld,%lld,%lld",
                (long long)g_comm_fault_start_ms,
                g_metrics.scenario_id ? g_metrics.scenario_id : "(null)",
                g_metrics.authorized_audit_changes,
                g_metrics.unauthorized_audit_changes,
                g_metrics.authorized_pid_changes,
                g_metrics.unauthorized_pid_changes,
                g_metrics.read_failures,
                g_metrics.comm_fault_intervals,
                (long long)g_metrics.baseline_established_ms,
                (long long)g_metrics.first_detection_ms,
                (long long)g_metrics.total_comm_fault_dur_ms);
        }

        void record_comm_fault_end() {
            if (!g_comm_fault_active) return;
            int64_t end_ms = now_ms();
            g_comm_fault_active = false;

            if (end_ms > g_comm_fault_start_ms) {
                g_metrics.total_comm_fault_dur_ms += (end_ms - g_comm_fault_start_ms);
            }
            ESP_LOGW(TAG, "COMM_FAULT_END at t=%lld ms (interval=%lld ms)", (long long)end_ms, (long long)(end_ms - g_comm_fault_start_ms));

            ESP_LOGI(TAG,
                "EVENT,%lld,%s,COMM_FAULT_END,-1,"
                "%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%lld,%lld,%lld",
                (long long)end_ms,
                g_metrics.scenario_id ? g_metrics.scenario_id : "(null)",
                g_metrics.authorized_audit_changes,
                g_metrics.unauthorized_audit_changes,
                g_metrics.authorized_pid_changes,
                g_metrics.unauthorized_pid_changes,
                g_metrics.read_failures,
                g_metrics.comm_fault_intervals,
                (long long)g_metrics.baseline_established_ms,
                (long long)g_metrics.first_detection_ms,
                (long long)g_metrics.total_comm_fault_dur_ms);
        }

        void fill_log_entry_context(LogEntry& entry) {
            // Scenario context
            entry.scenario_id      = g_metrics.scenario_id      ? g_metrics.scenario_id : "";
            entry.scenario_variant = g_metrics.scenario_variant ? g_metrics.scenario_variant : "";
            entry.trial_id         = std::to_string(g_metrics.trial_id);
            entry.change_expected  = g_metrics.change_expected;
            entry.change_type      = g_metrics.change_type      ? g_metrics.change_type : "";

            // Timing: poll_seq will be set by the caller; timestamps here
            int64_t ms = now_ms();
            entry.esp32_timestamp_ms  = static_cast<long>(ms);
            entry.esp32_timestamp_iso = make_iso8601_from_millis(ms);

            // PLC time – not yet wired per-poll; mark as NA for now
            entry.plc_time.plc_timestamp_ms  = -1;
            entry.plc_time.plc_timestamp_iso = "NA";

            // Metadata
            entry.metadata.poll_period_ms       = g_metrics.poll_period_ms;
            entry.metadata.esp_firmware_version = g_metrics.esp_firmware_version ? g_metrics.esp_firmware_version : "";
            entry.metadata.plc_firmware_version = g_metrics.plc_firmware_version ? g_metrics.plc_firmware_version : "";
        }

        void emit_log_entry(const LogEntry& entry) {
            std::string json = encode_log_to_json(entry);
            ESP_LOGI("JSON", "%s", json.c_str());
        }

        void dump_summary() {
            int64_t t = now_ms();
            
            ESP_LOGI(TAG,
                "Scenario='%s' Metrics: "
                "auth_audit=%" PRIu32 " unauth_audit=%" PRIu32 " "
                "auth_pid=%" PRIu32 " unauth_pid=%" PRIu32 " "
                "read_fail=%" PRIu32 " comm_faults=%" PRIu32 " "
                "baseline_ms=%lld first_det_ms=%lld "
                "comm_fault_total_ms=%lld",
                g_metrics.scenario_id ? g_metrics.scenario_id : "(null)",
                g_metrics.authorized_audit_changes,
                g_metrics.unauthorized_audit_changes,
                g_metrics.authorized_pid_changes,
                g_metrics.unauthorized_pid_changes,
                g_metrics.read_failures,
                g_metrics.comm_fault_intervals,
                (long long)g_metrics.baseline_established_ms,
                (long long)g_metrics.first_detection_ms,
                (long long)g_metrics.total_comm_fault_dur_ms);

                
            // CSV summary line
            ESP_LOGI(TAG,
                "SUMMARY,%lld,%s,-1,-1,"
                "%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%lld,%lld,%lld",
                (long long)t,
                g_metrics.scenario_id ? g_metrics.scenario_id : "(null)",
                g_metrics.authorized_audit_changes,
                g_metrics.unauthorized_audit_changes,
                g_metrics.authorized_pid_changes,
                g_metrics.unauthorized_pid_changes,
                g_metrics.read_failures,
                g_metrics.comm_fault_intervals,
                (long long)g_metrics.baseline_established_ms,
                (long long)g_metrics.first_detection_ms,
                (long long)g_metrics.total_comm_fault_dur_ms);
        }

        const ExperimentMetrics& current() {
            return g_metrics;
        }

} // namespace Experiment