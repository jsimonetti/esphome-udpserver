#include "udpserver_component.h"

#ifdef USE_WIFI
#include "esphome/components/wifi/wifi_component.h"
#endif
#ifdef USE_ETHERNET
#include "esphome/components/ethernet/ethernet_component.h"
#endif
#ifdef USE_SOCKET_IMPL_LWIP_TCP
#include <WiFiUdp.h>
#endif

namespace esphome {
    namespace udpserver {

        static const char *TAG = "udpserver";

        // ============================================================================
        // UDP Client Implementations
        // ============================================================================

#ifdef USE_SOCKET_IMPL_LWIP_TCP
        WiFiUDPClient::WiFiUDPClient() : udp_(new WiFiUDP()) {}

        WiFiUDPClient::~WiFiUDPClient() {
            delete udp_;
        }

        bool WiFiUDPClient::begin(uint16_t port) {
            return udp_->begin(port);
        }

        bool WiFiUDPClient::receive_packet(UDPPacket& packet) {
            int packetSize = udp_->parsePacket();
            if (packetSize <= 0) {
                return false;
            }

            if (packetSize > UDP_BUFFER_SIZE) {
                ESP_LOGW(TAG, "Received packet size %d exceeds maximum %d, truncating", 
                         packetSize, (int)UDP_BUFFER_SIZE);
                packetSize = UDP_BUFFER_SIZE;
            }

            int len = udp_->read(buffer_, packetSize);
            if (len <= 0) {
                return false;
            }

            buffer_[len] = '\0';
            packet.data = buffer_;
            packet.length = len;

            // Get remote IP and port
            IPAddress remote_addr = udp_->remoteIP();
            packet.remote_port = udp_->remotePort();

            // Convert IPAddress to string
            char ip_str[16];
            snprintf(ip_str, sizeof(ip_str), "%d.%d.%d.%d", 
                     remote_addr[0], remote_addr[1], remote_addr[2], remote_addr[3]);
            packet.remote_ip = ip_str;

            return true;
        }

        bool WiFiUDPClient::send_packet(const std::string& data, const std::string& ip, uint16_t port) {
            if (!udp_->beginPacket(ip.c_str(), port)) {
                return false;
            }
            udp_->write((const uint8_t*)data.c_str(), data.length());
            return udp_->endPacket();
        }
#endif

#if defined(USE_SOCKET_IMPL_BSD_SOCKETS) || defined(USE_SOCKET_IMPL_LWIP_SOCKETS)
        bool SocketUDPClient::begin(uint16_t port) {
            struct sockaddr_storage bind_addr;
            socklen_t bind_addrlen = socket::set_sockaddr_any((struct sockaddr *)&bind_addr, sizeof(bind_addr), port);

            socket_ = socket::socket_ip(SOCK_DGRAM, PF_INET);
            if (socket_ == nullptr) {
                ESP_LOGE(TAG, "Failed to create socket");
                return false;
            }

            int ret = socket_->bind((struct sockaddr *)&bind_addr, bind_addrlen);
            if (ret != 0) {
                ESP_LOGE(TAG, "Failed to bind socket to port %d: errno %d", port, errno);
                return false;
            }

            // Set socket to non-blocking mode
            socket_->setblocking(false);
            return true;
        }

        bool SocketUDPClient::receive_packet(UDPPacket& packet) {
            if (!socket_) {
                return false;
            }

            struct sockaddr_storage source_addr;
            socklen_t source_addrlen = sizeof(source_addr);
            ssize_t len = socket_->recvfrom(buffer_, UDP_BUFFER_SIZE - 1,
                                           (struct sockaddr *)&source_addr, &source_addrlen);

            if (len < 0) {
                if (errno != EAGAIN && errno != EWOULDBLOCK) {
                    ESP_LOGW(TAG, "recvfrom failed: errno %d", errno);
                }
                return false;
            }

            if (len == 0) {
                return false;
            }

            buffer_[len] = '\0';
            packet.data = buffer_;
            packet.length = len;

            // Convert source address to string (IPv4 only)
            char ip_str[INET_ADDRSTRLEN];
            packet.remote_port = 0;

            if (source_addr.ss_family == AF_INET) {
                struct sockaddr_in *addr_in = (struct sockaddr_in *)&source_addr;
                inet_ntop(AF_INET, &addr_in->sin_addr, ip_str, sizeof(ip_str));
                packet.remote_port = ntohs(addr_in->sin_port);
            } else {
                ESP_LOGW(TAG, "Unsupported address family: %d", source_addr.ss_family);
                return false;
            }

            packet.remote_ip = ip_str;
            return true;
        }

        bool SocketUDPClient::send_packet(const std::string& data, const std::string& ip, uint16_t port) {
            if (!socket_) {
                return false;
            }

            // Parse IP address and port from parameters
            struct sockaddr_in dest_addr;
            memset(&dest_addr, 0, sizeof(dest_addr));
            dest_addr.sin_family = AF_INET;
            dest_addr.sin_port = htons(port);
            
            if (inet_pton(AF_INET, ip.c_str(), &dest_addr.sin_addr) <= 0) {
                ESP_LOGE(TAG, "Invalid IP address: %s", ip.c_str());
                return false;
            }

            ssize_t sent = socket_->sendto(data.c_str(), data.length(), 0,
                                          (struct sockaddr*)&dest_addr, sizeof(dest_addr));
            return sent == (ssize_t)data.length();
        }
#endif

        // ============================================================================
        // UDPContext implementation
        // ============================================================================
        
        UDPContext::UDPContext(UDPClient* client, const std::string& ip, uint16_t port)
            : client_(client), remote_ip_(ip), remote_port_(port) {}

        bool UDPContext::send_response(const std::string& data) {
            if (!client_) {
                return false;
            }
            return client_->send_packet(data, remote_ip_, remote_port_);
        }

        // ============================================================================
        // UdpserverComponent implementation
        // ============================================================================

        void UdpserverComponent::setup() {
            ESP_LOGCONFIG(TAG, "Setting up UDP Server on port %d...", this->port_);
            
            // Create the appropriate UDP client implementation
#ifdef USE_SOCKET_IMPL_LWIP_TCP
            udp_client_ = std::unique_ptr<UDPClient>(new WiFiUDPClient());
            ESP_LOGD(TAG, "Using WiFiUDP implementation");
#elif defined(USE_SOCKET_IMPL_BSD_SOCKETS) || defined(USE_SOCKET_IMPL_LWIP_SOCKETS)
            udp_client_ = std::unique_ptr<UDPClient>(new SocketUDPClient());
            ESP_LOGD(TAG, "Using Socket implementation");
#else
            ESP_LOGE(TAG, "No UDP implementation available");
#endif
        }

        bool UdpserverComponent::is_ip_allowed(const char* ip) {
            // If no IP filter is set, allow all
            if (this->allow_all_ips_ || this->allowed_ips_.empty()) {
                return true;
            }
            
            // Check if IP is in allowed list
            std::string ip_str(ip);
            for (const auto& allowed_ip : this->allowed_ips_) {
                if (ip_str == allowed_ip) {
                    ESP_LOGD(TAG, "IP %s is allowed", ip);
                    return true;
                }
            }
            
            ESP_LOGW(TAG, "IP %s is not in allowed list, rejecting packet", ip);
            return false;
        }

        bool OnStringDataTrigger::matches_filter(const std::string& data) {
            // No filter set, allow all
            if (this->filter_mode_ == "none" || this->text_filter_.empty()) {
                return true;
            }
            
            // Apply filter based on mode
            if (this->filter_mode_ == "contains") {
                bool match = data.find(this->text_filter_) != std::string::npos;
                if (!match) {
                    ESP_LOGD(TAG, "Text filter 'contains' not matched: '%s' not in '%s'", 
                             this->text_filter_.c_str(), data.c_str());
                }
                return match;
            } 
            else if (this->filter_mode_ == "starts_with") {
                bool match = data.rfind(this->text_filter_, 0) == 0;
                if (!match) {
                    ESP_LOGD(TAG, "Text filter 'starts_with' not matched: '%s'", data.c_str());
                }
                return match;
            } 
            else if (this->filter_mode_ == "ends_with") {
                bool match = data.length() >= this->text_filter_.length() && 
                           data.compare(data.length() - this->text_filter_.length(), 
                                      this->text_filter_.length(), this->text_filter_) == 0;
                if (!match) {
                    ESP_LOGD(TAG, "Text filter 'ends_with' not matched: '%s'", data.c_str());
                }
                return match;
            } 
            else if (this->filter_mode_ == "equals") {
                bool match = data == this->text_filter_;
                if (!match) {
                    ESP_LOGD(TAG, "Text filter 'equals' not matched: '%s' != '%s'", 
                             data.c_str(), this->text_filter_.c_str());
                }
                return match;
            }
            
            return true;
        }

        void UdpserverComponent::loop() {
            // Initialize UDP on first loop iteration when network is ready
            if (!udp_initialized_) {
#ifdef USE_WIFI
                if (!wifi::global_wifi_component->is_connected()) {
                    return;  // Wait for WiFi
                }
#endif
#ifdef USE_ETHERNET
                if (!ethernet::global_eth_component->is_connected()) {
                    return;  // Wait for Ethernet
                }
#endif
                
                if (!udp_client_) {
                    ESP_LOGE(TAG, "UDP client not initialized");
                    this->mark_failed();
                    return;
                }

                ESP_LOGI(TAG, "Network ready, starting UDP server on port %d", this->port_);
                
                if (!udp_client_->begin(this->port_)) {
                    ESP_LOGE(TAG, "Failed to start UDP server on port %d", this->port_);
                    this->mark_failed();
                    return;
                }
                
                udp_initialized_ = true;
                ESP_LOGI(TAG, "UDP Server started successfully on port %d", this->port_);
            }

            // Try to receive a packet
            UDPPacket packet;
            if (!udp_client_->receive_packet(packet)) {
                return;  // No packet received
            }

            ESP_LOGD(TAG, "Received UDP packet: %d bytes from %s:%d", 
                     packet.length, packet.remote_ip.c_str(), packet.remote_port);

            // Check IP filtering
            if (!is_ip_allowed(packet.remote_ip.c_str())) {
                ESP_LOGW(TAG, "Packet from %s rejected by IP filter", packet.remote_ip.c_str());
                return;
            }

            // Create UDP context for response capability
            UDPContext udp_ctx(udp_client_.get(), packet.remote_ip, packet.remote_port);
            std::string data_str((char*)packet.data, packet.length);

            // Trigger all registered string data triggers with data and context
            for (auto& trigger : string_triggers_) {
                if (trigger->matches_filter(data_str)) {
                    trigger->trigger(data_str, udp_ctx);
                }
            }
        }
    }
}
