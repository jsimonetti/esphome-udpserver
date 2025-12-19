#ifdef USE_ESP_IDF

#include "udp_socket_idf.h"
#include <cstring>
#include <cstdlib>

namespace esphome {
namespace udpserver {

static const char* TAG = "udpserver.idf";

UDPSocketIDF::UDPSocketIDF() {
    remote_ip_str_[0] = '\0';
}

UDPSocketIDF::~UDPSocketIDF() {
    stop();
}

bool UDPSocketIDF::begin(uint16_t port) {
    ESP_LOGD(TAG, "Starting ESP-IDF UDP socket on port %d", port);
    
    // Create UDP socket
    sock_fd_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock_fd_ < 0) {
        ESP_LOGE(TAG, "Failed to create socket: errno %d", errno);
        return false;
    }
    
    // Set socket to non-blocking mode
    int flags = fcntl(sock_fd_, F_GETFL, 0);
    if (flags < 0 || fcntl(sock_fd_, F_SETFL, flags | O_NONBLOCK) < 0) {
        ESP_LOGW(TAG, "Failed to set socket non-blocking: errno %d", errno);
    }
    
    // Enable address reuse
    int reuse = 1;
    if (setsockopt(sock_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        ESP_LOGW(TAG, "Failed to set SO_REUSEADDR: errno %d", errno);
    }
    
    // Bind to port
    memset(&local_addr_, 0, sizeof(local_addr_));
    local_addr_.sin_family = AF_INET;
    local_addr_.sin_addr.s_addr = INADDR_ANY;
    local_addr_.sin_port = htons(port);
    
    if (bind(sock_fd_, (struct sockaddr*)&local_addr_, sizeof(local_addr_)) < 0) {
        ESP_LOGE(TAG, "Failed to bind socket to port %d: errno %d", port, errno);
        close(sock_fd_);
        sock_fd_ = -1;
        return false;
    }
    
    ESP_LOGD(TAG, "ESP-IDF UDP socket started successfully on port %d", port);
    return true;
}

void UDPSocketIDF::stop() {
    if (sock_fd_ >= 0) {
        ESP_LOGD(TAG, "Stopping ESP-IDF UDP socket");
        close(sock_fd_);
        sock_fd_ = -1;
    }
    
    if (packet_buffer_) {
        free(packet_buffer_);
        packet_buffer_ = nullptr;
    }
    
    if (send_buffer_) {
        free(send_buffer_);
        send_buffer_ = nullptr;
    }
    
    packet_size_ = 0;
    packet_read_ = 0;
    send_buffer_size_ = 0;
    send_buffer_used_ = 0;
    has_remote_info_ = false;
}

int UDPSocketIDF::parsePacket() {
    if (sock_fd_ < 0) {
        return 0;
    }
    
    // Free previous packet buffer if any
    if (packet_buffer_) {
        free(packet_buffer_);
        packet_buffer_ = nullptr;
        packet_size_ = 0;
        packet_read_ = 0;
    }
    
    // Allocate buffer for incoming packet
    packet_buffer_ = (uint8_t*)malloc(MAX_PACKET_SIZE);
    if (!packet_buffer_) {
        ESP_LOGE(TAG, "Failed to allocate packet buffer");
        return 0;
    }
    
    // Receive packet
    socklen_t addr_len = sizeof(remote_addr_);
    ssize_t len = recvfrom(sock_fd_, packet_buffer_, MAX_PACKET_SIZE, 0,
                           (struct sockaddr*)&remote_addr_, &addr_len);
    
    if (len < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            ESP_LOGW(TAG, "recvfrom error: errno %d", errno);
        }
        free(packet_buffer_);
        packet_buffer_ = nullptr;
        return 0;
    }
    
    if (len == 0) {
        free(packet_buffer_);
        packet_buffer_ = nullptr;
        return 0;
    }
    
    packet_size_ = len;
    packet_read_ = 0;
    
    // Store remote IP as string
    inet_ntop(AF_INET, &remote_addr_.sin_addr, remote_ip_str_, sizeof(remote_ip_str_));
    has_remote_info_ = true;
    
    ESP_LOGV(TAG, "Parsed packet: %d bytes from %s:%d", 
             len, remote_ip_str_, ntohs(remote_addr_.sin_port));
    
    return len;
}

int UDPSocketIDF::read(uint8_t* buffer, size_t len) {
    if (!packet_buffer_ || packet_read_ >= packet_size_) {
        return 0;
    }
    
    size_t available = packet_size_ - packet_read_;
    size_t to_read = (len < available) ? len : available;
    
    memcpy(buffer, packet_buffer_ + packet_read_, to_read);
    packet_read_ += to_read;
    
    ESP_LOGV(TAG, "Read %d bytes from UDP packet", to_read);
    return to_read;
}

bool UDPSocketIDF::beginPacket(const char* ip, uint16_t port) {
    if (sock_fd_ < 0) {
        ESP_LOGE(TAG, "Socket not initialized");
        return false;
    }
    
    ESP_LOGV(TAG, "Begin packet to %s:%d", ip, port);
    
    // Reset send buffer
    if (send_buffer_) {
        free(send_buffer_);
        send_buffer_ = nullptr;
    }
    
    send_buffer_size_ = MAX_PACKET_SIZE;
    send_buffer_ = (uint8_t*)malloc(send_buffer_size_);
    if (!send_buffer_) {
        ESP_LOGE(TAG, "Failed to allocate send buffer");
        return false;
    }
    
    send_buffer_used_ = 0;
    
    // Setup destination address
    memset(&send_addr_, 0, sizeof(send_addr_));
    send_addr_.sin_family = AF_INET;
    send_addr_.sin_port = htons(port);
    
    if (inet_pton(AF_INET, ip, &send_addr_.sin_addr) <= 0) {
        ESP_LOGE(TAG, "Invalid IP address: %s", ip);
        free(send_buffer_);
        send_buffer_ = nullptr;
        return false;
    }
    
    return true;
}

size_t UDPSocketIDF::write(const uint8_t* buffer, size_t len) {
    if (!send_buffer_ || send_buffer_used_ + len > send_buffer_size_) {
        ESP_LOGW(TAG, "Send buffer full or not initialized");
        return 0;
    }
    
    memcpy(send_buffer_ + send_buffer_used_, buffer, len);
    send_buffer_used_ += len;
    
    ESP_LOGV(TAG, "Wrote %d bytes to send buffer (total: %d)", len, send_buffer_used_);
    return len;
}

bool UDPSocketIDF::endPacket() {
    if (sock_fd_ < 0 || !send_buffer_ || send_buffer_used_ == 0) {
        ESP_LOGE(TAG, "Cannot send packet: socket not ready or no data");
        return false;
    }
    
    ssize_t sent = sendto(sock_fd_, send_buffer_, send_buffer_used_, 0,
                          (struct sockaddr*)&send_addr_, sizeof(send_addr_));
    
    bool success = (sent == (ssize_t)send_buffer_used_);
    
    if (success) {
        ESP_LOGV(TAG, "Sent %d bytes successfully", sent);
    } else {
        ESP_LOGE(TAG, "sendto failed: errno %d, sent %d of %d bytes", 
                 errno, sent, send_buffer_used_);
    }
    
    // Clean up send buffer
    free(send_buffer_);
    send_buffer_ = nullptr;
    send_buffer_size_ = 0;
    send_buffer_used_ = 0;
    
    return success;
}

const char* UDPSocketIDF::remoteIP() {
    if (!has_remote_info_) {
        ESP_LOGW(TAG, "Remote IP requested but no packet parsed");
        return "0.0.0.0";
    }
    return remote_ip_str_;
}

uint16_t UDPSocketIDF::remotePort() {
    if (!has_remote_info_) {
        ESP_LOGW(TAG, "Remote port requested but no packet parsed");
        return 0;
    }
    return ntohs(remote_addr_.sin_port);
}

} // namespace udpserver
} // namespace esphome

#endif // USE_ESP_IDF
