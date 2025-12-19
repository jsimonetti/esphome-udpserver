#pragma once

#ifdef USE_ARDUINO

#include "udp_socket_base.h"
#include "esphome/core/log.h"
#include <WiFiUdp.h>

namespace esphome {
namespace udpserver {

/**
 * Arduino WiFiUDP implementation of UDPSocketBase
 */
class UDPSocketArduino : public UDPSocketBase {
public:
    UDPSocketArduino();
    ~UDPSocketArduino() override;
    
    bool begin(uint16_t port) override;
    void stop() override;
    int parsePacket() override;
    int read(uint8_t* buffer, size_t len) override;
    bool beginPacket(const char* ip, uint16_t port) override;
    size_t write(const uint8_t* buffer, size_t len) override;
    bool endPacket() override;
    const char* remoteIP() override;
    uint16_t remotePort() override;

private:
    WiFiUDP udp_;
    char remote_ip_str_[16];
    uint16_t remote_port_{0};
    bool has_remote_info_{false};
};

} // namespace udpserver
} // namespace esphome

#endif // USE_ARDUINO
