# ESPHome UDP Server Component

A custom ESPHome component that creates a UDP server to receive messages and send responses.
This component was tested and works on an esp32 with arduino framework.

## Features

- 🔌 Listen for UDP packets on a configurable port
- 📨 Receive string data via UDP
- 🔄 **Send UDP responses back to the sender**
- 🎯 Trigger ESPHome automations on data reception
- 🔍 Access sender's IP address and port in lambda functions
- 🔒 **IP address filtering/whitelist**
- 🔎 **Text content filtering** (contains, starts_with, ends_with, equals)
- ⚡ Non-blocking continuous operation with loop() monitoring
- 🎭 Multiple triggers with different filters per UDP port

## Installation

### Method 1: External Components (Recommended)

Add this to your ESPHome configuration:

```yaml
external_components:
  - source: github://jsimonetti/esphome-udpserver
    components: [ udpserver ]
```

### Method 2: Manual Installation

1. Download or clone this repository
2. Copy the `components/udpserver` folder to your ESPHome configuration directory under `custom_components/`

### Method 3: Git Submodule

```bash
cd /path/to/your/esphome/config
git submodule add https://github.com/jsimonetti/esphome-udpserver.git custom_components/udpserver-repo
ln -s custom_components/udpserver-repo/components/udpserver custom_components/udpserver
```

## Configuration

```yaml
udpserver:
  listen_port: 8888  # Port to listen on (0-65535)
  allowed_ips:       # Optional: IP whitelist (if omitted, all IPs allowed)
    - "192.168.1.100"
    - "192.168.1.101"
  on_string_data:    # Trigger when data is received
    - text_filter: "CMD:"      # Optional: only trigger if text contains/matches
      filter_mode: CONTAINS    # Optional: CONTAINS, STARTS_WITH, ENDS_WITH, EQUALS
      then:
        - lambda: |-
            // Access the received data
            ESP_LOGD("udp", "Received: %s", data.c_str());
            
            // Access sender information
            ESP_LOGD("udp", "From: %s:%d", udp.get_remote_ip(), udp.get_remote_port());
            
            // Send a response back to the sender
            udp.send_response("Message received!");
```

## API Reference

### Configuration Options

- **`listen_port`** (Required, int): Port number to listen on (0-65535)
- **`allowed_ips`** (Optional, list): Whitelist of IP addresses allowed to send messages. If omitted, all IPs are allowed.
- **`on_string_data`** (Optional, automation): Triggered when UDP data is received
  - **`text_filter`** (Optional, string): Text pattern to match
  - **`filter_mode`** (Optional, enum): How to match the text filter
    - `CONTAINS`: Message must contain the filter text (default)
    - `STARTS_WITH`: Message must start with the filter text
    - `ENDS_WITH`: Message must end with the filter text
    - `EQUALS`: Message must exactly match the filter text

### Lambda Variables

When the `on_string_data` trigger fires, the following variables are available:

- **`data`** (std::string): The received UDP message as a string
- **`udp`** (UDPContext): Context object for accessing sender info and sending responses

### UDPContext Methods

- **`udp.send_response(std::string data)`**: Send a UDP response back to the sender
  - Returns: `bool` - true if sent successfully, false otherwise
  
- **`udp.get_remote_ip()`**: Get the sender's IP address
  - Returns: `const char*` - IP address as string
  
- **`udp.get_remote_port()`**: Get the sender's port number
  - Returns: `uint16_t` - Port number

## Usage Examples

### 1. Simple Echo Server

```yaml
udpserver:
  listen_port: 8888
  on_string_data:
    - lambda: |-
        udp.send_response("ECHO: " + data);
```

### 2. IP Filtering - Only Allow Specific Clients

```yaml
udpserver:
  listen_port: 8888
  allowed_ips:
    - "192.168.1.100"  # Admin workstation
    - "192.168.1.101"  # Monitoring server
  on_string_data:
    - lambda: |-
        udp.send_response("Authorized: " + data);
```

### 3. Text Filtering - Command Router

```yaml
udpserver:
  listen_port: 9000
  on_string_data:
    # Handle commands that start with "CMD:"
    - text_filter: "CMD:"
      filter_mode: STARTS_WITH
      then:
        - lambda: |-
            std::string cmd = data.substr(4); // Remove "CMD:" prefix
            ESP_LOGI("udp", "Executing: %s", cmd.c_str());
            udp.send_response("OK");
    
    # Handle queries that end with "?"
    - text_filter: "?"
      filter_mode: ENDS_WITH
      then:
        - lambda: |-
            udp.send_response("Answer: 42");
    
    # Handle exact "ping" command
    - text_filter: "ping"
      filter_mode: EQUALS
      then:
        - lambda: |-
            udp.send_response("pong");
```

### 4. Command Handler

```yaml
udpserver:
  listen_port: 9999
  on_string_data:
    - lambda: |-
        if (data == "ping") {
          udp.send_response("pong");
        } else if (data == "status") {
          udp.send_response("OK");
        } else {
          udp.send_response("ERROR: Unknown command");
```

### 5. Control Switches via UDP

```yaml
switch:
  - platform: gpio
    pin: GPIO2
    id: relay1

udpserver:
  listen_port: 8888
  on_string_data:
    - lambda: |-
        if (data == "on") {
          id(relay1).turn_on();
          udp.send_response("Relay ON");
        } else if (data == "off") {
          id(relay1).turn_off();
          udp.send_response("Relay OFF");
        }
```

### 6. Combining IP and Text Filters - Secure Admin Commands

```yaml
udpserver:
  listen_port: 9999
  allowed_ips:
    - "192.168.1.10"  # Admin workstation only
  on_string_data:
    - text_filter: "ADMIN:"
      filter_mode: STARTS_WITH
      then:
        - lambda: |-
            std::string cmd = data.substr(6);
            ESP_LOGI("admin", "Admin command from %s: %s", 
                     udp.get_remote_ip(), cmd.c_str());
            
            if (cmd == "restart") {
              udp.send_response("Restarting...");
              ESP.restart();
            } else {
              udp.send_response("Unknown admin command");
            }
```

### 7. JSON Response

```yaml
udpserver:
  listen_port: 8888
  on_string_data:
    - lambda: |-
        std::string response = "{\"status\":\"ok\",\"message\":\"" + data + "\"}";
        udp.send_response(response);
```

## Testing

### Using netcat (Linux/Mac)

```bash
# Basic test (wait 1 second for response)
echo "ping" | nc -u -w1 192.168.1.100 8888

# Send with specific source port
echo "hello" | nc -p 54321 -u 192.168.1.100 8888

# Interactive mode (type commands and see responses)
nc -u 192.168.1.100 8888

# Send multiple commands
echo "CMD:turn_on" | nc -u -w1 192.168.1.100 8888
echo "status?" | nc -u -w1 192.168.1.100 8888
echo "relay_status" | nc -u -w1 192.168.1.100 9999

# Send without waiting for response
echo "relay_on" | nc -u -N 192.168.1.100 9999
```

### Using Python

```python
import socket

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.settimeout(2.0)

# Send message
sock.sendto(b"ping", ("192.168.1.100", 8888))

# Receive response
response, addr = sock.recvfrom(1024)
print(f"Response: {response.decode()}")

sock.close()
```

### Using Node.js

```javascript
const dgram = require('dgram');
const client = dgram.createSocket('udp4');

client.on('message', (msg, rinfo) => {
  console.log(`Response: ${msg}`);
  client.close();
});

client.send('ping', 8888, '192.168.1.100', (err) => {
  if (err) console.error(err);
});
```

## Technical Details

### Architecture

- **Component Type**: Component with continuous loop() monitoring
- **Base Classes**: Inherits from ESPHome's `Component`
- **Dependencies**: Requires `network` component (WiFi or Ethernet)
- **Initialization**: UDP socket starts after network connection is established

### Response Mechanism

When a UDP packet is received:
1. The component parses the packet and extracts data
2. A `UDPContext` object is created with the sender's IP/port and UDP socket reference
3. All registered triggers are fired with the data and context
4. Lambda functions can call `udp.send_response()` to send data back
5. The response is sent to the original sender's IP and port

### Limitations

- Maximum packet size: 1400 bytes (configurable in source)
- UDP is connectionless - no delivery guarantee
- Responses are sent to the sender of the most recent packet
- Multiple simultaneous connections are handled sequentially

## Contributing

Contributions are welcome! Please ensure code follows ESPHome conventions and includes proper error handling.
