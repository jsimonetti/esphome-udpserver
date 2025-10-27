# UDP Server Filtering Guide

This guide explains the IP and text filtering capabilities of the ESPHome UDP Server component.

## Table of Contents
1. [IP Filtering](#ip-filtering)
2. [Text Filtering](#text-filtering)
3. [Combining Filters](#combining-filters)
4. [Use Cases](#use-cases)
5. [Best Practices](#best-practices)

---

## IP Filtering

### Overview
IP filtering allows you to restrict which clients can send UDP messages to your device. This is useful for security and to prevent unwanted traffic.

### Configuration

#### Allow All IPs (Default)
```yaml
udpserver:
  listen_port: 8888
  # No allowed_ips specified = all IPs accepted
  on_string_data:
    - then:
        - lambda: |-
            udp.send_response("Hello from anyone!");
```

#### Whitelist Specific IPs
```yaml
udpserver:
  listen_port: 8888
  allowed_ips:
    - "192.168.1.100"
    - "192.168.1.101"
    - "10.0.0.50"
  on_string_data:
    - then:
        - lambda: |-
            udp.send_response("Hello from authorized IP!");
```

### How It Works
1. When a UDP packet arrives, the component extracts the sender's IP address
2. If `allowed_ips` is configured, the IP is checked against the whitelist
3. If the IP is not in the list, the packet is rejected (logged as warning)
4. If the IP is allowed (or no filter exists), processing continues

### Logging
```
[D][udpserver:053] Received UDP packet: 10 bytes from 192.168.1.100:54321
[D][udpserver:055] IP 192.168.1.100 is allowed
[D][udpserver:028] Processing data, length=10, data=test
```

Or for blocked IPs:
```
[D][udpserver:053] Received UDP packet: 10 bytes from 192.168.1.200:54321
[W][udpserver:059] Packet from 192.168.1.200 rejected by IP filter
```

---

## Text Filtering

### Overview
Text filtering allows you to create different handlers for different types of messages on the same UDP port. This is perfect for command routing and protocol parsing.

### Filter Modes

#### 1. CONTAINS (Default)
Matches if the message **contains** the filter text anywhere.

```yaml
udpserver:
  listen_port: 8888
  on_string_data:
    - text_filter: "ERROR"
      filter_mode: CONTAINS
      then:
        - lambda: |-
            ESP_LOGE("udp", "Error received: %s", data.c_str());
            udp.send_response("Error logged");
```

**Examples:**
- ✅ "ERROR: Something failed" → Matches
- ✅ "This is an ERROR message" → Matches
- ✅ "ERRORS everywhere" → Matches
- ❌ "Warning: issue detected" → Does not match

#### 2. STARTS_WITH
Matches if the message **begins with** the filter text.

```yaml
udpserver:
  listen_port: 8888
  on_string_data:
    - text_filter: "CMD:"
      filter_mode: STARTS_WITH
      then:
        - lambda: |-
            std::string cmd = data.substr(4); // Remove "CMD:" prefix
            ESP_LOGI("udp", "Command: %s", cmd.c_str());
            udp.send_response("Command received");
```

**Examples:**
- ✅ "CMD:turn_on" → Matches
- ✅ "CMD:" → Matches
- ❌ "SENDCMD:test" → Does not match
- ❌ "turn_on CMD:" → Does not match

#### 3. ENDS_WITH
Matches if the message **ends with** the filter text.

```yaml
udpserver:
  listen_port: 8888
  on_string_data:
    - text_filter: "?"
      filter_mode: ENDS_WITH
      then:
        - lambda: |-
            ESP_LOGI("udp", "Query: %s", data.c_str());
            udp.send_response("Answer: 42");
```

**Examples:**
- ✅ "What is the status?" → Matches
- ✅ "?" → Matches
- ❌ "? What now" → Does not match
- ❌ "status" → Does not match

#### 4. EQUALS
Matches **only** if the message exactly equals the filter text.

```yaml
udpserver:
  listen_port: 8888
  on_string_data:
    - text_filter: "ping"
      filter_mode: EQUALS
      then:
        - lambda: |-
            udp.send_response("pong");
```

**Examples:**
- ✅ "ping" → Matches
- ❌ "ping " → Does not match (extra space)
- ❌ "PING" → Does not match (case sensitive)
- ❌ "pinging" → Does not match

### Multiple Text Filters

You can have multiple triggers with different text filters on the same port:

```yaml
udpserver:
  listen_port: 8888
  on_string_data:
    # Catch commands
    - text_filter: "CMD:"
      filter_mode: STARTS_WITH
      then:
        - lambda: |-
            udp.send_response("Command processed");
    
    # Catch queries
    - text_filter: "?"
      filter_mode: ENDS_WITH
      then:
        - lambda: |-
            udp.send_response("Query response");
    
    # Catch specific command
    - text_filter: "ping"
      filter_mode: EQUALS
      then:
        - lambda: |-
            udp.send_response("pong");
    
    # Catch-all for everything else
    - then:
        - lambda: |-
            udp.send_response("Message received");
```

**Important:** Triggers are evaluated in order. The first matching filter triggers, but subsequent triggers can also fire if they match.

---

## Combining Filters

### IP + Text Filtering
Combine both IP and text filters for maximum security and routing:

```yaml
udpserver:
  listen_port: 9999
  allowed_ips:
    - "192.168.1.10"  # Admin workstation
  on_string_data:
    # Admin commands (IP already filtered at component level)
    - text_filter: "ADMIN:"
      filter_mode: STARTS_WITH
      then:
        - lambda: |-
            std::string cmd = data.substr(6);
            if (cmd == "restart") {
              udp.send_response("Restarting...");
              ESP.restart();
            } else if (cmd == "factory_reset") {
              udp.send_response("Resetting...");
              // Factory reset logic
            }
```

### Multiple Ports with Different Rules

```yaml
# Public API - No IP filter, text routing only
udpserver:
  id: public_api
  listen_port: 8888
  on_string_data:
    - text_filter: "ping"
      filter_mode: EQUALS
      then:
        - lambda: udp.send_response("pong");
    
    - text_filter: "status"
      filter_mode: EQUALS
      then:
        - lambda: udp.send_response("Online");

# Admin API - Strict IP filtering
udpserver:
  id: admin_api
  listen_port: 9999
  allowed_ips:
    - "192.168.1.10"
  on_string_data:
    - text_filter: "ADMIN:"
      filter_mode: STARTS_WITH
      then:
        - lambda: |-
            // Admin operations
            udp.send_response("Admin command processed");
```

---

## Use Cases

### 1. Command Routing System
```yaml
udpserver:
  listen_port: 8080
  on_string_data:
    - text_filter: "relay:"
      filter_mode: STARTS_WITH
      then:
        - lambda: |-
            // Handle relay commands
    
    - text_filter: "sensor:"
      filter_mode: STARTS_WITH
      then:
        - lambda: |-
            // Handle sensor queries
    
    - text_filter: "config:"
      filter_mode: STARTS_WITH
      then:
        - lambda: |-
            // Handle configuration
```

### 2. Security Zones
```yaml
# Zone 1: LAN only - Full access
udpserver:
  listen_port: 8888
  allowed_ips:
    - "192.168.1.0/24"  # Note: CIDR not yet supported, list individual IPs
  on_string_data:
    - then:
        - lambda: |-
            // Full API access

# Zone 2: Monitoring - Read-only
udpserver:
  listen_port: 8889
  allowed_ips:
    - "10.0.0.50"  # Monitoring server
  on_string_data:
    - text_filter: "status"
      filter_mode: CONTAINS
      then:
        - lambda: |-
            // Return status only, no control
```

### 3. Protocol Parsing
```yaml
udpserver:
  listen_port: 9000
  on_string_data:
    # JSON messages
    - text_filter: "{"
      filter_mode: STARTS_WITH
      then:
        - lambda: |-
            // Parse JSON
    
    # XML messages
    - text_filter: "<?xml"
      filter_mode: STARTS_WITH
      then:
        - lambda: |-
            // Parse XML
    
    # Plain text
    - then:
        - lambda: |-
            // Handle as plain text
```

---

## Best Practices

### Security

1. **Always use IP filtering for admin functions:**
   ```yaml
   allowed_ips:
     - "192.168.1.10"  # Your admin machine only
   ```

2. **Use text filters to separate public and private APIs:**
   ```yaml
   # Public commands don't need prefix
   - text_filter: "ping"
     filter_mode: EQUALS
   
   # Admin commands require prefix
   - text_filter: "ADMIN:"
     filter_mode: STARTS_WITH
   ```

3. **Log rejected packets for monitoring:**
   The component automatically logs rejected IPs at WARNING level.

### Performance

1. **Use EQUALS for exact matches** (fastest):
   ```yaml
   text_filter: "ping"
   filter_mode: EQUALS
   ```

2. **Put most common filters first:**
   ```yaml
   on_string_data:
     - text_filter: "ping"  # Most common
     - text_filter: "status"
     - text_filter: "ADMIN:"  # Least common
   ```

3. **Don't overuse filters on high-traffic ports:**
   Text filtering has minimal overhead, but evaluate your needs.

### Maintainability

1. **Use consistent naming:**
   ```yaml
   # Good: Clear prefix pattern
   - text_filter: "CMD:"
   - text_filter: "QUERY:"
   - text_filter: "ADMIN:"
   
   # Bad: Inconsistent
   - text_filter: "command_"
   - text_filter: "??"
   - text_filter: "ADMINPASS"
   ```

2. **Document your filters:**
   ```yaml
   on_string_data:
     # Handle device control commands from authorized IPs
     - text_filter: "CTRL:"
       filter_mode: STARTS_WITH
       then:
         - lambda: |-
             // Implementation
   ```

3. **Test your filters:**
   Use netcat to verify your configuration:
   ```bash
   # Test IP filtering
   echo "test" | nc -p 54321 -u 192.168.1.10 8888
   
   # Test text filtering
   echo "CMD:status" | nc -u -w1 192.168.1.10 8888
   ```

---

## Troubleshooting

### Packets Not Triggering

1. **Check IP filter:**
   ```
   [W][udpserver:059] Packet from 192.168.1.200 rejected by IP filter
   ```
   Solution: Add the IP to `allowed_ips` or remove IP filtering.

2. **Check text filter:**
   ```
   [D][udpserver:045] Text filter 'contains' not matched: 'CMD:' not in 'command'
   ```
   Solution: Adjust your filter text or mode.

3. **Enable debug logging:**
   ```yaml
   logger:
     level: DEBUG
   ```

### Multiple Triggers Firing

This is normal behavior - all matching triggers will fire. To prevent this, use more specific filters or handle it in lambda:

```yaml
on_string_data:
  - text_filter: "ping"
    filter_mode: EQUALS
    then:
      - lambda: |-
          udp.send_response("pong");
          return;  // Stop here
```

---

## Examples Summary

See the complete example files:
- `examples/filtering.yaml` - Comprehensive filtering examples
- `examples/usage.yaml` - Basic usage examples
- `README.md` - Full API documentation
