// TagWrites.hpp
// George Lake
// Fall 2025
// 
// Helpers to write simple tages via EtherNet/IP


#pragma once
#include <string>

#include "EnipClient.hpp"
#include "CipCodec.hpp"

bool write_bool_tag(EnipClient& enip, const std::string& tag_name, bool value);
bool write_dint_tag(EnipClient& enip, const std::string& tag_name, int32_t value);
bool increment_dint_tag(EnipClient& enip, const std::string& tag_name);