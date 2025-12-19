#pragma once

#include <string>
#include <cstdint>

namespace esphome {
namespace udpserver {

/**
 * Abstract base class for UDP socket operations.
 * Provides a common interface for both Arduino WiFiUDP and ESP-IDF lwIP implementations.
 */
class UDPSocketBase {
public:
    virtual ~UDPSocketBase() = default;
    
    /**
     * Initialize and bind UDP socket to a port
     * @param port Port number to bind to (0-65535)
     * @return true if successful, false otherwise
     */
    virtual bool begin(uint16_t port) = 0;
    
    /**
     * Close the UDP socket
     */
    virtual void stop() = 0;
    
    /**
     * Check for incoming packets and prepare for reading
     * @return Size of the packet in bytes, or 0 if no packet available
     */
    virtual int parsePacket() = 0;
    
    /**
     * Read data from the current packet
     * @param buffer Buffer to store received data
     * @param len Maximum number of bytes to read
     * @return Number of bytes actually read
     */
    virtual int read(uint8_t* buffer, size_t len) = 0;
    
    /**
     * Begin preparing a packet to send
     * @param ip Destination IP address (as string)
     * @param port Destination port
     * @return true if successful, false otherwise
     */
    virtual bool beginPacket(const char* ip, uint16_t port) = 0;
    
    /**
     * Write data to the packet being prepared
     * @param buffer Data to write
     * @param len Number of bytes to write
     * @return Number of bytes written
     */
    virtual size_t write(const uint8_t* buffer, size_t len) = 0;
    
    /**
     * Finish and send the packet
     * @return true if packet was sent successfully, false otherwise
     */
    virtual bool endPacket() = 0;
    
    /**
     * Get the IP address of the sender of the current packet
     * @return IP address as C string (remains valid until next parsePacket)
     */
    virtual const char* remoteIP() = 0;
    
    /**
     * Get the port number of the sender of the current packet
     * @return Port number
     */
    virtual uint16_t remotePort() = 0;
};

} // namespace udpserver
} // namespace esphome
