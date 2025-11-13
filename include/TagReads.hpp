// TagReads.hpp
// George Lake
// Fall 2025
//
//


#pragma once
#include <cstdint>
#include <array>

class EnipClient;
namespace Cip { struct Value; }

bool read_tag_scalar(EnipClient& enip, const char* tag, Cip::Value& out);
bool read_dint(EnipClient& enip, const char* tag, int32_t& out);
bool read_lint(EnipClient& enip, const char* tag, int64_t& out);
bool read_real(EnipClient& enip, const char* tag, float& out);
bool read_dint_array7(EnipClient& enip, const char* base, std::array<int32_t,7>& out);

