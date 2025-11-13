// TagWrites.cpp
// George Lake
// Fall 2025
//
// Helpers to write simple tags via EIP



#include "esp_log.h"

#include "TagWrites.hpp"
#include "TagReads.hpp"

static const char* TAG = "TAG_WRITES";

static bool send_write_request(EnipClient& enip, const std::vector<uint8_t>& cip_req) {
    //
    //
    //
    std::vector<uint8_t> rr = Cip::wrap_sendrr(cip_req);
    std::vector<uint8_t> rr_resp;

    if (!enip.send_rr_data(rr, rr_resp)) {
        ESP_LOGW(TAG, "send_rr_data failed");
        return false;
    }

    // Extract CIP reply
    std::vector<uint8_t> cip_resp;
    if (!Cip::extract_cip_from_rr(rr_resp, cip_resp)) {
        ESP_LOGW(TAG, "extract_cip_from_rr failed");
        return false;
    }

    // Check general status
    if (cip_resp.size() < 4) {
        ESP_LOGW(TAG, "CIP reply too short");
        return false;
    }

    // bit 7 set = reply
    if ((cip_resp[0] & 0x80) == 0) {
        ESP_LOGW(TAG, "Not a reply (service=0x%02X)", cip_resp[0]);
        return false;
    }

    uint8_t gen_status = cip_resp[2];
    if (gen_status != 0) {
        ESP_LOGW(TAG, "CIP write error, general status=0x%02X", gen_status);
        return false;
    }
    return true;
}

bool write_bool_tag(EnipClient& enip, const std::string& tag_name, bool value) {
    //
    //
    //
    auto cip = Cip::build_write_bool(tag_name, value);
    return send_write_request(enip, cip);
}

bool write_dint_tag(EnipClient& enip, const std::string& tag_name, int32_t value) {
    //
    //
    //
    auto cip = Cip::build_write_dint(tag_name, value);
    return send_write_request(enip, cip);
}

bool increment_dint_tag(EnipClient& enip, const std::string& tag_name) {
    //
    //
    //
    int32_t current = 0;
    if (!read_dint(enip, tag_name.c_str(), current)) {
        ESP_LOGW(TAG, "increment_dint_tag: read_dint failed for %s", tag_name.c_str());
        return false;
    }

    int32_t next = current + 1;

    if (!write_dint_tag(enip, tag_name, next)) {
        ESP_LOGW(TAG, "increment_dint_tag: write_dint_tag failed for %s", tag_name.c_str());
        return false;
    }

    ESP_LOGI(TAG, "increment_dint_tag: %s %d -> %d",
             tag_name.c_str(), current, next);
    return true;
}

