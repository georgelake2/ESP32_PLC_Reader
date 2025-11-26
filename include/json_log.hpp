// json_log.hpp
//
// Produce a direct one-to-one mapping with JSON structure

#pragma once
#include <string>
#include <vector>

struct PLCDateTime{
    std::string plc_timestamp_iso;
    long plc_timestamp_ms;
};

struct CurrentValues {
    std::string AuditValue;
    std::string AuthorizedUser;

    double Kp;
    double Ki;
    double Kd;

    std::string ControllerStatus;
    std::string AuxStatus;

    std::string ExperimentMarker;
};

struct BaselineValues {
    std::string AuditValue;
    std::string AuthorizedUser;

    double Kp;
    double Ki;
    double Kd;

    std::string ControllerStatus;
    std::string AuxStatus;
};

struct ComparisonData {
    bool any_change;
    bool unauthorized_change;
    bool authorized_change;

    std::vector<std::string> changed_fields;

    bool chg_AuditValue;
    bool chg_AuthorizedUser;
    bool chg_Kp;
    bool chg_Ki;
    bool chg_Kd;
    bool chg_ControllerStatus;
    bool chg_AuxStatus;

    double delta_Kp;
    double delta_Ki;
    double delta_Kd;
};

struct CommData {
    std::string comm_status;
    bool read_ok;
    int retry_count;
};

struct GroundTruthData {
    std::string t_change_groundtruth_iso;
    std::string t_change_marker_seen;
};

struct Metadata {
    int poll_period_ms;
    std::string esp_firmware_version;
    std::string plc_firmware_version;
};

struct LogEntry {
    // Scenario context
    std::string scenario_id;
    std::string scenario_variant;
    std::string trial_id;
    bool change_expected;
    std::string change_type;

    // Timing
    long poll_seq;
    std::string esp32_timestamp_iso;
    long esp32_timestamp_ms;

    // PLC timestamps
    PLCDateTime plc_time;

    // Nested data
    CurrentValues current;
    BaselineValues baseline;
    ComparisonData comparison;
    CommData comm;
    GroundTruthData groundtruth;
    Metadata metadata;
};