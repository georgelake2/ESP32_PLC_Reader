// AuditMonitor.hpp
// George Lake
// Fall 2025
//
// Purpose: 
//      Periodically poll the PLC's AuditValue and AuthorizedUser. 
//      If AuditValue changes and AuthorizedUser == 0, the change is unauthorized.
//
// Usage:
//      1) Provide read functions that return true on success:
//          bool readAuditValue
//          bool readAuthorizedUser
//      2) Call start_audit_monitor after WIFI and ENIP session is up
//
// Notes:
//      AuthorizedUser == 0, not authorized
//      AuthorizedUser == 1, authorized
//      Threading: Create FreeRTOS task and logs with ESP_LOG

#pragma once
#include <cstdint>
class EnipClient;

void start_audit_monitor(EnipClient* enip, const char* audit_tag, const char* authorized_tag, uint32_t poll_ms);

bool readAuditValue(int64_t& out_lint);
bool readAuthorizedUser(int32_t& out_dint);