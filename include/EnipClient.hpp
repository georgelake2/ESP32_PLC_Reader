// EnipClient.hpp
// George Lake
// Fall2025
//
// Small EitherNet/IP UCMM Client (RegisterSession + SendRRData)


#pragma once
#include <cstdint>
#include <string>
#include <vector>

class EnipClient {
public:
    EnipClient(const std::string& ip, uint16_t port);
    ~EnipClient();

    bool connect_tcp();
    bool register_session();
    bool send_rr_data(const std::vector<uint8_t>& rr, std::vector<uint8_t>& rr_resp);
    void close();

private:
    bool send_all(const void* data, size_t len);
    bool recv_all(void* data, size_t len);

    std::string ip_;
    uint16_t port_{0};
    int sock_{-1};
    uint32_t session_{0};
};