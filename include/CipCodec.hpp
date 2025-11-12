// CipCodec.hpp
// George Lake
// Fall 2025
//
// CIP helpers to read a symbol
//      scalar
//      fixed DINT[7]


#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace Cip {
    enum class Type : uint8_t { BOOL, SINT, INT, DINT, LINT, REAL, UNSUPPORTED };

    struct Value {
        Type type{Type::UNSUPPORTED};
        union {
        bool     b;
        int8_t   i8;
        int16_t  i16;
        int32_t  i32;
        int64_t  i64;
        float    f32;
        } v{};
    };

    // Build a CIP "Read Tag Service" for a symbol path with 'elements' count.
    std::vector<uint8_t> build_read_request(const std::string& tag_name, uint16_t elements);

    // Wrap a CIP payload into a SendRRData (UCMM) data item.
    std::vector<uint8_t> wrap_sendrr(const std::vector<uint8_t>& cip);

    // Pull the Unconnected Data (0x00B2) item payload back out of a SendRRData reply.
    bool extract_cip_from_rr(const std::vector<uint8_t>& rr, std::vector<uint8_t>& out);

    // Parse a simple read-reply payload (scalar types).
    bool parse_read_reply(const std::vector<uint8_t>& c, Value& out);
}
