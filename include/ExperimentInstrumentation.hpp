// ExperimentInstrumentation.hpp
// George Lake
// Fall 2025
// 
// High-level instrumentation for scenarios S1-S5
// Tracks counts and basic timing metrics


# pragma once

#include <cstdint>
#include <stdint.h>
#include "json_log.hpp"

struct LogEntry;

// Simple container for aggregated metrics.
// Can extend later if more fields are needed.
struct ExperimentMetrics {
    const char* scenario_id = nullptr;
    const char* scenario_variant;
    int         trial_id;
    bool        change_expected;
    const char* change_type;
    uint32_t    poll_period_ms;
    const char* esp_firmware_version;
    const char* plc_firmware_version;

    // Counters
    uint32_t authorized_audit_changes   = 0;
    uint32_t unauthorized_audit_changes = 0;
    uint32_t authorized_pid_changes     = 0;
    uint32_t unauthorized_pid_changes   = 0;
    uint32_t read_failures              = 0;
    uint32_t comm_fault_intervals        = 0;

    // Timing
    // All times are espNowMs() (monotonic, ms since boot)
    int64_t baseline_established_ms     = -1;
    int64_t first_detection_ms          = -1;
    int64_t total_comm_fault_dur_ms     = 0;
};

namespace Experiment {

    // Initialize metrics for a new run (scenario S1-S5)
    // Call once from app_main() before starting the audit task
    void init(const char*   scenario_id, 
              const char*   scenario_variant,
              int           trial_id,
              bool          change_expected,
              const char*   change_type,
              uint32_t      poll_period_ms);

    // Configure time synce once PLC epoch is known
    void set_time_sync(int64_t plc_epoch_ms_at_sync, int64_t esp_ms_at_sync);

    // Reset counters for a fresh repetition of the same scenario
    void reset_metrics();

    // Mark the moment the watchdog baselines are fully established
    // Call from AuditMonitor when both audit and PID baselines are set
    void mark_baseline_established();

    // Record an AuditValue change (authorized or unauthorized)
    void record_audit_change(bool authorized);

    // Record a PID change (authorized or unauthorized)
    void record_pid_change(bool authorized);

    // Record a single read failure (any of the CIP reads failed)
    void record_read_failure();

    // Mark start/end of a "communication fault interval"
    // e.g., sustained read failures for N polls
    void record_comm_fault_start();
    void record_comm_fault_end();

    // Dump a summary to ESP_LOGI
    // Call at the end of a scenario run, or periodically for debugging
    void dump_summary();

    // Fills the scenario/experiment-related fields in a LogEntry
    // Everything that is the same for every poll in a trial
    void fill_log_entry_context(LogEntry& entry);

    // Emits a single JSONL record.
    void emit_log_entry(const LogEntry& entry);

    // Optional accessor to inspect metrics directly:
    const ExperimentMetrics& current();
} // namespace Experiment