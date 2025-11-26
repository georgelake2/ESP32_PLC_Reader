// json_encode.hpp
//
//

#pragma once
#include <string>
#include "json_log.hpp"

std::string encode_log_to_json(const LogEntry& log);