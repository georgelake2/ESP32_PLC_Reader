// CipCodec.cpp
// George Lake
// Fall 2025
//
// CIP path encoder and read-rely parser


#include "CipCodec.hpp"
#include <cstring>

namespace {
    static void emit_one_symbol(std::vector<uint8_t>& buf, const std::string& s) {
        //
        //
        //
        buf.push_back(0x91);                  // ANSI extended symbol segment
        buf.push_back((uint8_t)s.size());     // length
        buf.insert(buf.end(), s.begin(), s.end());
        if (s.size() & 0x01) buf.push_back(0x00); // pad to even
    }
    static void emit_symbol_path(std::vector<uint8_t>& buf, const std::string& full) {
        //
        //
        //
        size_t start = 0;
        while (start <= full.size()) {
        size_t dot = full.find('.', start);
        std::string token = (dot == std::string::npos) ? full.substr(start)
                                                        : full.substr(start, dot - start);
        if (!token.empty()) emit_one_symbol(buf, token);
        if (dot == std::string::npos) break;
        start = dot + 1;
        }
    }
}

namespace Cip {
    std::vector<uint8_t> build_read_request(const std::string& tag_name, uint16_t elements) {
        //
        //
        //
        std::vector<uint8_t> c;
        c.push_back(0x4C);            // Service: Read Tag
        c.push_back(0x00);            // Path size (words) — to be filled
        size_t path_start = c.size();
        emit_symbol_path(c, tag_name);
        c[1] = (uint8_t)((c.size() - path_start)/2);
        c.push_back((uint8_t)(elements & 0xFF));
        c.push_back((uint8_t)(elements >> 8));
        return c;
    }

    std::vector<uint8_t> wrap_sendrr(const std::vector<uint8_t>& cip) {
        //
        //
        //
        std::vector<uint8_t> rr;
        uint32_t if_handle = 0;  // UCMM
        uint16_t timeout   = 0;
        // interface handle
        rr.push_back((uint8_t)(if_handle      ));
        rr.push_back((uint8_t)(if_handle >> 8 ));
        rr.push_back((uint8_t)(if_handle >>16 ));
        rr.push_back((uint8_t)(if_handle >>24 ));
        // timeout
        rr.push_back((uint8_t)(timeout));
        rr.push_back((uint8_t)(timeout >> 8));
        // item count
        rr.push_back(0x02); rr.push_back(0x00);

        // Address item: Null (0x0000, len=0)
        rr.push_back(0x00); rr.push_back(0x00);
        rr.push_back(0x00); rr.push_back(0x00);

        // Data item: Unconnected Data (0x00B2)
        rr.push_back(0xB2); rr.push_back(0x00);
        uint16_t dlen = (uint16_t)cip.size();
        rr.push_back((uint8_t)(dlen));
        rr.push_back((uint8_t)(dlen >> 8));
        rr.insert(rr.end(), cip.begin(), cip.end());
        return rr;
    }

    bool extract_cip_from_rr(const std::vector<uint8_t>& rr, std::vector<uint8_t>& out) {
        //
        //
        //
        if (rr.size() < 8) return false;
        size_t off = 0;
        off += 4; // if_handle
        off += 2; // timeout
        uint16_t item_count = rr[6] | (rr[7] << 8);
        off += 2;
        for (uint16_t i=0; i<item_count; ++i) {
        if (rr.size() < off + 4) return false;
        uint16_t type = rr[off] | (rr[off+1] << 8);
        uint16_t len  = rr[off+2] | (rr[off+3] << 8);
        off += 4;
        if (rr.size() < off + len) return false;
        if (type == 0x00B2) {
            out.assign(rr.begin()+off, rr.begin()+off+len);
            return true;
        }
        off += len;
        }
        return false;
    }

    bool parse_read_reply(const std::vector<uint8_t>& c, Value& out) {
        //
        //
        //
        if (c.size() < 4) return false;
        if ((c[0] & 0x80) == 0) return false; // must be a reply
        uint8_t gen = c[2], add = c[3];
        if (gen != 0) return false;           // CIP error
        size_t data_off = 4 + add * 2;
        if (c.size() < data_off + 2) return false;

        uint16_t type_id = c[data_off] | (c[data_off+1] << 8);
        size_t val_off   = data_off + 2;

        switch (type_id) {
        case 0x00C1: // BOOL
            if (c.size() < val_off + 1) return false;
            out.type = Type::BOOL; out.v.b = (c[val_off] & 1) != 0; return true;
        case 0x00C2: // SINT
            if (c.size() < val_off + 1) return false;
            out.type = Type::SINT; out.v.i8 = (int8_t)c[val_off];   return true;
        case 0x00C3: // INT
            if (c.size() < val_off + 2) return false;
            out.type = Type::INT;
            out.v.i16 = (int16_t)((uint16_t)c[val_off] | ((uint16_t)c[val_off+1] << 8));
            return true;
        case 0x00C4: // DINT
            if (c.size() < val_off + 4) return false;
            out.type = Type::DINT;
            out.v.i32 = (int32_t)((uint32_t)c[val_off] |
                                ((uint32_t)c[val_off+1]<<8) |
                                ((uint32_t)c[val_off+2]<<16) |
                                ((uint32_t)c[val_off+3]<<24));
            return true;
        case 0x00C5: // LINT
            if (c.size() < val_off + 8) return false;
            out.type = Type::LINT;
            {
            uint64_t u64 = 0;
            for (int i=0;i<8;++i) u64 |= (uint64_t)c[val_off+i] << (8*i);
            out.v.i64 = (int64_t)u64;
            }
            return true;
        case 0x00CA: // REAL
            if (c.size() < val_off + 4) return false;
            out.type = Type::REAL;
            {
            uint32_t u = (uint32_t)c[val_off] |
                        ((uint32_t)c[val_off+1]<<8) |
                        ((uint32_t)c[val_off+2]<<16)|
                        ((uint32_t)c[val_off+3]<<24);
            std::memcpy(&out.v.f32, &u, sizeof(float));
            }
            return true;
        default: break;
        }
        out.type = Type::UNSUPPORTED;
        return false;
    }
}
