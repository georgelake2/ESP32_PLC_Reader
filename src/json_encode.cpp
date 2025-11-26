#include "json_encode.hpp"
#include "cJSON.h"

std::string encode_log_to_json(const LogEntry& log) {

    cJSON* root = cJSON_CreateObject();

    // Scenario context
    cJSON_AddStringToObject(root, "scenario_id", log.scenario_id.c_str());
    cJSON_AddStringToObject(root, "scenario_variant", log.scenario_variant.c_str());
    cJSON_AddStringToObject(root, "trial_id", log.trial_id.c_str());
    cJSON_AddBoolToObject(root, "change_expected", log.change_expected);
    cJSON_AddStringToObject(root, "change_type", log.change_type.c_str());

    // Timing
    cJSON_AddNumberToObject(root, "poll_seq", log.poll_seq);
    cJSON_AddStringToObject(root, "esp32_timestamp_iso", log.esp32_timestamp_iso.c_str());
    cJSON_AddNumberToObject(root, "esp32_timestamp_ms", log.esp32_timestamp_ms);

    // PLC timestamps
    cJSON_AddStringToObject(root, "plc_timestamp_iso", log.plc_time.plc_timestamp_iso.c_str());
    cJSON_AddNumberToObject(root, "plc_timestamp_ms", log.plc_time.plc_timestamp_ms);

    // Current
    cJSON* cur = cJSON_AddObjectToObject(root, "current");
    cJSON_AddStringToObject(cur, "AuditValue", log.current.AuditValue.c_str());
    cJSON_AddStringToObject(cur, "AuthorizedUser", log.current.AuthorizedUser.c_str());
    cJSON_AddNumberToObject(cur, "Kp", log.current.Kp);
    cJSON_AddNumberToObject(cur, "Ki", log.current.Ki);
    cJSON_AddNumberToObject(cur, "Kd", log.current.Kd);
    cJSON_AddStringToObject(cur, "ControllerStatus", log.current.ControllerStatus.c_str());
    cJSON_AddStringToObject(cur, "AuxStatus", log.current.AuxStatus.c_str());
    cJSON_AddStringToObject(cur, "ExperimentMarker", log.current.ExperimentMarker.c_str());

    // Baseline
    cJSON* base = cJSON_AddObjectToObject(root, "baseline");
    cJSON_AddStringToObject(base, "AuditValue", log.baseline.AuditValue.c_str());
    cJSON_AddStringToObject(base, "AuthorizedUser", log.baseline.AuthorizedUser.c_str());
    cJSON_AddNumberToObject(base, "Kp", log.baseline.Kp);
    cJSON_AddNumberToObject(base, "Ki", log.baseline.Ki);
    cJSON_AddNumberToObject(base, "Kd", log.baseline.Kd);
    cJSON_AddStringToObject(base, "ControllerStatus", log.baseline.ControllerStatus.c_str());
    cJSON_AddStringToObject(base, "AuxStatus", log.baseline.AuxStatus.c_str());

    // Comparison
    cJSON* cmp = cJSON_AddObjectToObject(root, "comparison");
    cJSON_AddBoolToObject(cmp, "any_change", log.comparison.any_change);
    cJSON_AddBoolToObject(cmp, "unauthorized_change", log.comparison.unauthorized_change);
    cJSON_AddBoolToObject(cmp, "authorized_change", log.comparison.authorized_change);

    cJSON* arr = cJSON_AddArrayToObject(cmp, "changed_fields");
    for (auto& field : log.comparison.changed_fields)
        cJSON_AddItemToArray(arr, cJSON_CreateString(field.c_str()));

    cJSON_AddBoolToObject(cmp, "chg_AuditValue", log.comparison.chg_AuditValue);
    cJSON_AddBoolToObject(cmp, "chg_AuthorizedUser", log.comparison.chg_AuthorizedUser);
    cJSON_AddBoolToObject(cmp, "chg_Kp", log.comparison.chg_Kp);
    cJSON_AddBoolToObject(cmp, "chg_Ki", log.comparison.chg_Ki);
    cJSON_AddBoolToObject(cmp, "chg_Kd", log.comparison.chg_Kd);
    cJSON_AddBoolToObject(cmp, "chg_ControllerStatus", log.comparison.chg_ControllerStatus);
    cJSON_AddBoolToObject(cmp, "chg_AuxStatus", log.comparison.chg_AuxStatus);

    cJSON_AddNumberToObject(cmp, "delta_Kp", log.comparison.delta_Kp);
    cJSON_AddNumberToObject(cmp, "delta_Ki", log.comparison.delta_Ki);
    cJSON_AddNumberToObject(cmp, "delta_Kd", log.comparison.delta_Kd);

    // Comm
    cJSON* comm = cJSON_AddObjectToObject(root, "comm");
    cJSON_AddStringToObject(comm, "comm_status", log.comm.comm_status.c_str());
    cJSON_AddBoolToObject(comm, "read_ok", log.comm.read_ok);
    cJSON_AddNumberToObject(comm, "retry_count", log.comm.retry_count);

    // Ground truth
    cJSON* gt = cJSON_AddObjectToObject(root, "groundtruth");
    cJSON_AddStringToObject(gt, "t_change_groundtruth_iso", log.groundtruth.t_change_groundtruth_iso.c_str());
    cJSON_AddStringToObject(gt, "t_change_marker_seen", log.groundtruth.t_change_marker_seen.c_str());

    // Metadata
    cJSON* meta = cJSON_AddObjectToObject(root, "metadata");
    cJSON_AddNumberToObject(meta, "poll_period_ms", log.metadata.poll_period_ms);
    cJSON_AddStringToObject(meta, "esp_firmware_version", log.metadata.esp_firmware_version.c_str());
    cJSON_AddStringToObject(meta, "plc_firmware_version", log.metadata.plc_firmware_version.c_str());

    // Convert to JSON string
    char* raw = cJSON_PrintUnformatted(root);
    std::string json(raw);

    free(raw);
    cJSON_Delete(root);

    return json;
}
