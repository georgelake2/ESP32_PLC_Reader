// TagReads.cpp
// George Lake
// Fall 2025
//
//

#include <vector>
#include <array>
#include "CipCodec.hpp"
#include "EnipClient.hpp"
#include "TagReads.hpp"

bool read_tag_scalar(EnipClient& enip, const char* tag, Cip::Value& out) {
    //
    //
    //
    auto cip = Cip::build_read_request(tag, 1);
    auto rr  = Cip::wrap_sendrr(cip);
    std::vector<uint8_t> rr_body, cip_reply;
    if (!enip.send_rr_data(rr, rr_body)) return false;
    if (!Cip::extract_cip_from_rr(rr_body, cip_reply)) return false;
    return Cip::parse_read_reply(cip_reply, out);
}

bool read_dint(EnipClient& enip, const char* tag, int32_t& out) {
    //
    //
    //
    Cip::Value v{};
    if (!read_tag_scalar(enip, tag, v)) return false;
    if (v.type != Cip::Type::DINT) return false;
    out = v.v.i32;
    return true;
}

bool read_lint(EnipClient& enip, const char* tag, int64_t& out) {
    //
    //
    //
    Cip::Value v{};
    if (!read_tag_scalar(enip, tag, v)) return false;
    if (v.type != Cip::Type::LINT) return false;
    out = v.v.i64;
    return true;
}

bool read_real(EnipClient& enip, const char* tag, float& out) {
    //
    //
    //
    Cip::Value v{};
    if (!read_tag_scalar(enip, tag, v)) return false;
    if (v.type != Cip::Type::REAL) return false;
    out = v.v.f32;
    return true;
}

bool read_dint_array7(EnipClient& enip, const char* base, std::array<int32_t,7>& out) {
    //
    //
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

    uint16_t type_id = c[data_off] | (c[data_off+1] << 8);
    if (type_id != 0x00C4) return false; // DINT
    size_t val_off = data_off + 2;
    size_t need = 7 * 4;
    if (c.size() < val_off + need) return false;

    for (int i = 0; i < 7; ++i) {
        size_t p = val_off + i*4;
        uint32_t u = (uint32_t)c[p] | ((uint32_t)c[p+1] << 8) | ((uint32_t)c[p+2] << 16) | ((uint32_t)c[p+3] << 24);
        out[i] = (int32_t)u;
    }
    return true;
}
