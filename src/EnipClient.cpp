// EnipClient.cpp
// George Lake
// Fall 2025
// 
// Minimal EIP encapsulation (RegisterSession / SendRRData)


#include "lwip/inet.h"
#include "lwip/sockets.h"
#include "esp_log.h"

#include <cstring>
#include <vector>
#include <errno.h>
#include <inttypes.h>

#include "EnipClient.hpp"

namespace {
    static const char* TAG = "ENIP";
    #pragma pack(push, 1)
    
    struct EncapsulationHeader {
        uint16_t command;
        uint16_t length;
        uint32_t session;
        uint32_t status;
        uint8_t context[8];
        uint32_t options;
    };
    #pragma pack(pop)
}

EnipClient::EnipClient(const std::string& ip, uint16_t port) : ip_(ip), port_(port) {}
EnipClient::~EnipClient() {close(); }

bool EnipClient::connect_tcp() {
    //
    //
    //
    sock_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sock_ < 0) { ESP_LOGE(TAG, "socket() failed"); return false; }

    sockaddr_in a{}; a.sin_family = AF_INET;
    a.sin_port = htons(port_);
    a.sin_addr.s_addr = inet_addr(ip_.c_str());

    if (::connect(sock_, (sockaddr*)&a, sizeof(a)) != 0) {
        ESP_LOGE(TAG, "connect() errno=%d", errno);
        close();
        return false;
    }
    return true;
}

bool EnipClient::register_session() {
    //
    //
    //
    if (sock_ < 0) return false;

    EncapsulationHeader hdr{};
    hdr.command = 0x0065;
    hdr.length = 4;
    uint8_t body[4] = {0x01, 0x00, 0x00, 0x00};

    std::vector<uint8_t> pkt(sizeof(hdr)+sizeof(body));
    std::memcpy(pkt.data(), &hdr, sizeof(hdr));
    std::memcpy(pkt.data()+sizeof(hdr), body, sizeof(body));
    if (!send_all(pkt.data(), pkt.size())) return false;

    EncapsulationHeader rh{};
    if (!recv_all(&rh, sizeof(rh))) return false;

    uint16_t len = rh.length;
    std::vector<uint8_t> rbody(len);
    if (len && !recv_all(rbody.data(), len)) return false;

    if (rh.command != 0x0065 || rh.status != 0) {
        ESP_LOGE(TAG, "RegisterSession failed: status=0x%08" PRIX32, (uint32_t)rh.status);
        return false;
    }

    session_ = rh.session;
    ESP_LOGI(TAG, "Session=0x%08" PRIX32, (uint32_t)session_);
    return true;
}

bool EnipClient::send_rr_data(const std::vector<uint8_t>& rr, std::vector<uint8_t>& rr_resp) {
    //
    //
    //
    if (sock_ < 0 || session_ == 0) return false;

    EncapsulationHeader hdr{};
    hdr.command = 0x006F;
    hdr.length = (uint16_t)rr.size();
    hdr.session = session_;

    std::vector<uint8_t> pkt(sizeof(hdr)+rr.size());
    std::memcpy(pkt.data(), &hdr, sizeof(hdr));
    std::memcpy(pkt.data()+sizeof(hdr), rr.data(), rr.size());
    if (!send_all(pkt.data(), pkt.size())) return false;

    EncapsulationHeader rh{};
    if (!recv_all(&rh, sizeof(rh))) return false;
    uint16_t len = rh.length;
    rr_resp.resize(len);
    if (len && !recv_all(rr_resp.data(), len)) return false;

    if (rh.command != 0x006F || rh.status != 0) {
        ESP_LOGE(TAG, "SendRRData failed: status=0x%08" PRIX32, (uint32_t)rh.status);
        return false;
    }
    return true;
}

void EnipClient::close() {
    //
    //
    //
    if (sock_ >= 0) {
        ::close(sock_);
        sock_ = -1;
        session_ = 0;
    }
}

bool EnipClient::send_all(const void* data, size_t len) {
    //
    //
    //
    const uint8_t* p = static_cast<const uint8_t*>(data);
    while (len) {
        int n = ::send(sock_, p, len, 0);
        if (n <= 0) return false;
        p   += n;
        len -= (size_t)n;
    }
    return true;
}

bool EnipClient::recv_all(void* data, size_t len) {
    //
    //
    //
    uint8_t* p = static_cast<uint8_t*>(data);
    while (len) {
        int n = ::recv(sock_, p, len, 0);
        if (n <= 0) return false;
        p   += n;
        len -= (size_t)n;
    }
    return true;
}
