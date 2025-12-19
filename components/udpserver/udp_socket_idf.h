#pragma once

#ifdef USE_ESP_IDF

#include "udp_socket_base.h"
#include "esphome/core/log.h"
#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>

namespace esphome {
namespace udpserver {

/**
 * ESP-IDF lwIP sockets implementation of UDPSocketBase
 */
class UDPSocketIDF : public UDPSocketBase {
public:
    UDPSocketIDF();
    ~UDPSocketIDF() override;
    
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
    int sock_fd_{-1};
    struct sockaddr_in local_addr_{};
    struct sockaddr_in remote_addr_{};
    struct sockaddr_in send_addr_{};
    char remote_ip_str_[16];
    uint8_t* packet_buffer_{nullptr};
    size_t packet_size_{0};
    size_t packet_read_{0};
    uint8_t* send_buffer_{nullptr};
    size_t send_buffer_size_{0};
    size_t send_buffer_used_{0};
    bool has_remote_info_{false};
    
    static constexpr size_t MAX_PACKET_SIZE = 1500;
};

} // namespace udpserver
} // namespace esphome

#endif // USE_ESP_IDF
