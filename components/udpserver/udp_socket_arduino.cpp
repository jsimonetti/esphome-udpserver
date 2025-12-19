#ifdef USE_ARDUINO

#include "udp_socket_arduino.h"
#include <cstring>

namespace esphome {
namespace udpserver {

static const char* TAG = "udpserver.arduino";

UDPSocketArduino::UDPSocketArduino() {
    remote_ip_str_[0] = '\0';
}

UDPSocketArduino::~UDPSocketArduino() {
    stop();
}

bool UDPSocketArduino::begin(uint16_t port) {
    ESP_LOGD(TAG, "Starting Arduino UDP socket on port %d", port);
    bool result = udp_.begin(port);
    if (result) {
        ESP_LOGD(TAG, "Arduino UDP socket started successfully");
    } else {
        ESP_LOGE(TAG, "Failed to start Arduino UDP socket");
    }
    return result;
}

void UDPSocketArduino::stop() {
    ESP_LOGD(TAG, "Stopping Arduino UDP socket");
    udp_.stop();
    has_remote_info_ = false;
}

int UDPSocketArduino::parsePacket() {
    int packet_size = udp_.parsePacket();
    
    if (packet_size > 0) {
        // Store remote info when packet arrives
        IPAddress remote_addr = udp_.remoteIP();
        remote_port_ = udp_.remotePort();
        
        snprintf(remote_ip_str_, sizeof(remote_ip_str_), "%d.%d.%d.%d",
                 remote_addr[0], remote_addr[1], remote_addr[2], remote_addr[3]);
        
        has_remote_info_ = true;
        ESP_LOGV(TAG, "Parsed packet: %d bytes from %s:%d", 
                 packet_size, remote_ip_str_, remote_port_);
    }
    
    return packet_size;
}

int UDPSocketArduino::read(uint8_t* buffer, size_t len) {
    int bytes_read = udp_.read(buffer, len);
    ESP_LOGV(TAG, "Read %d bytes from UDP packet", bytes_read);
    return bytes_read;
}

bool UDPSocketArduino::beginPacket(const char* ip, uint16_t port) {
    ESP_LOGV(TAG, "Begin packet to %s:%d", ip, port);
    return udp_.beginPacket(ip, port);
}

size_t UDPSocketArduino::write(const uint8_t* buffer, size_t len) {
    size_t written = udp_.write(buffer, len);
    ESP_LOGV(TAG, "Wrote %d bytes to UDP packet", written);
    return written;
}

bool UDPSocketArduino::endPacket() {
    bool result = udp_.endPacket();
    ESP_LOGV(TAG, "End packet: %s", result ? "success" : "failed");
    return result;
}

const char* UDPSocketArduino::remoteIP() {
    if (!has_remote_info_) {
        ESP_LOGW(TAG, "Remote IP requested but no packet parsed");
        return "0.0.0.0";
    }
    return remote_ip_str_;
}

uint16_t UDPSocketArduino::remotePort() {
    if (!has_remote_info_) {
        ESP_LOGW(TAG, "Remote port requested but no packet parsed");
        return 0;
    }
    return remote_port_;
}

} // namespace udpserver
} // namespace esphome

#endif // USE_ARDUINO
