#pragma once

#include <string>
#include <queue>
#include <memory>

#include "esphome.h"
#include "esphome/core/component.h"

#if defined(USE_SOCKET_IMPL_BSD_SOCKETS) || defined(USE_SOCKET_IMPL_LWIP_SOCKETS)
#include "esphome/components/socket/socket.h"
#endif

namespace esphome {
  namespace udpserver {
    class OnStringDataTrigger;

    // Maximum UDP packet size without fragmentation (MTU 1500 - IP header 20 - UDP header 8)
    static constexpr size_t UDP_BUFFER_SIZE = 1472;

    // ============================================================================
    // UDP Client Abstraction Layer
    // ============================================================================

    // Structure to hold received packet data
    struct UDPPacket {
      uint8_t* data;
      size_t length;
      std::string remote_ip;
      uint16_t remote_port;
    };

    // Abstract UDP client interface
    class UDPClient {
    public:
      virtual ~UDPClient() = default;
      virtual bool begin(uint16_t port) = 0;
      virtual bool receive_packet(UDPPacket& packet) = 0;
      virtual bool send_packet(const std::string& data, const std::string& ip, uint16_t port) = 0;
    };

#ifdef USE_SOCKET_IMPL_LWIP_TCP
    // Forward declaration for WiFiUDP
    class WiFiUDP;

    // WiFiUDP implementation
    class WiFiUDPClient : public UDPClient {
    public:
      WiFiUDPClient();
      ~WiFiUDPClient() override;
      bool begin(uint16_t port) override;
      bool receive_packet(UDPPacket& packet) override;
      bool send_packet(const std::string& data, const std::string& ip, uint16_t port) override;

    private:
      WiFiUDP* udp_;
      uint8_t buffer_[UDP_BUFFER_SIZE];
    };
#endif

#if defined(USE_SOCKET_IMPL_BSD_SOCKETS) || defined(USE_SOCKET_IMPL_LWIP_SOCKETS)
    // Socket-based implementation
    class SocketUDPClient : public UDPClient {
    public:
      bool begin(uint16_t port) override;
      bool receive_packet(UDPPacket& packet) override;
      bool send_packet(const std::string& data, const std::string& ip, uint16_t port) override;

    private:
      std::unique_ptr<socket::Socket> socket_;
      uint8_t buffer_[UDP_BUFFER_SIZE];
    };
#endif

    // ============================================================================
    // UDP Server Component
    // ============================================================================

    // Helper class to store UDP response context
    class UDPContext {
    public:
      UDPContext(UDPClient* client, const std::string& ip, uint16_t port);
      
      bool send_response(const std::string& data);
      const char* get_remote_ip() const { return remote_ip_.c_str(); }
      uint16_t get_remote_port() const { return remote_port_; }

    private:
      UDPClient* client_;
      std::string remote_ip_;
      uint16_t remote_port_;
    };

    class UdpserverComponent : public Component {
    public:
      UdpserverComponent() = default;
      void setup() override;
      void loop() override;

      void set_listen_port(uint16_t port) { this->port_ = port; }
      void add_string_trigger(OnStringDataTrigger *trigger) { this->string_triggers_.push_back(trigger); }
      
      // IP filtering methods
      void add_allowed_ip(const std::string& ip) { this->allowed_ips_.push_back(ip); }
      void set_allow_all_ips(bool allow) { this->allow_all_ips_ = allow; }

    protected:
        bool is_ip_allowed(const char* ip);

        uint16_t port_{8888};
        std::unique_ptr<UDPClient> udp_client_;
        std::vector<OnStringDataTrigger *> string_triggers_{};
        std::vector<std::string> allowed_ips_{};
        bool allow_all_ips_{true};
        bool udp_initialized_{false};
    };

    class OnStringDataTrigger : public Trigger<std::string, UDPContext>, public Component {
      friend class UdpserverComponent;

    public:
      explicit OnStringDataTrigger(UdpserverComponent *parent) : parent_(parent){};

      void setup() override { this->parent_->add_string_trigger(this); }
      
      // Text filter methods
      void set_text_filter(const std::string& filter) { this->text_filter_ = filter; }
      void set_filter_mode(const std::string& mode) { this->filter_mode_ = mode; }

    protected:
      bool matches_filter(const std::string& data);
      
      UdpserverComponent *parent_;
      std::string text_filter_{};
      std::string filter_mode_{"none"}; // none, contains, starts_with, ends_with, equals
    };

  }
}
