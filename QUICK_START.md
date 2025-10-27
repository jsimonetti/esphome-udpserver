# Quick Start - UDP Server v2.1 Filtering

## IP Filtering Cheatsheet

```yaml
# No filter - Allow all IPs (default)
udpserver:
  listen_port: 8888

# Whitelist specific IPs
udpserver:
  listen_port: 8888
  allowed_ips:
    - "192.168.1.100"
    - "192.168.1.101"
```

**Result:** Only packets from listed IPs are processed. Others are rejected and logged.

---

## Text Filtering Cheatsheet

### CONTAINS (default)
```yaml
text_filter: "CMD"
filter_mode: CONTAINS
```
| Message | Match? |
|---------|--------|
| "CMD:on" | ✅ Yes |
| "SEND CMD" | ✅ Yes |
| "command" | ❌ No |

### STARTS_WITH
```yaml
text_filter: "CMD:"
filter_mode: STARTS_WITH
```
| Message | Match? |
|---------|--------|
| "CMD:on" | ✅ Yes |
| "CMD:" | ✅ Yes |
| "SEND CMD:" | ❌ No |

### ENDS_WITH
```yaml
text_filter: "?"
filter_mode: ENDS_WITH
```
| Message | Match? |
|---------|--------|
| "status?" | ✅ Yes |
| "?" | ✅ Yes |
| "? what" | ❌ No |

### EQUALS
```yaml
text_filter: "ping"
filter_mode: EQUALS
```
| Message | Match? |
|---------|--------|
| "ping" | ✅ Yes |
| "ping " | ❌ No (space) |
| "pinging" | ❌ No |

---

## Common Patterns

### 1. Public + Admin Ports
```yaml
# Public API - No restrictions
udpserver:
  id: public
  listen_port: 8888
  on_string_data:
    - text_filter: "ping"
      filter_mode: EQUALS
      then:
        - lambda: udp.send_response("pong");

# Admin API - IP restricted
udpserver:
  id: admin
  listen_port: 9999
  allowed_ips:
    - "192.168.1.10"
  on_string_data:
    - text_filter: "ADMIN:"
      filter_mode: STARTS_WITH
      then:
        - lambda: |-
            // Admin commands only from 192.168.1.10
```

### 2. Command Router
```yaml
udpserver:
  listen_port: 8080
  on_string_data:
    # Commands
    - text_filter: "cmd:"
      filter_mode: STARTS_WITH
      then:
        - lambda: |-
            // Handle commands

    # Queries
    - text_filter: "?"
      filter_mode: ENDS_WITH
      then:
        - lambda: |-
            // Handle queries

    # Exact matches
    - text_filter: "ping"
      filter_mode: EQUALS
      then:
        - lambda: |-
            udp.send_response("pong");

    # Catch-all
    - then:
        - lambda: |-
            udp.send_response("Unknown");
```

### 3. Protocol Parser
```yaml
udpserver:
  listen_port: 9000
  on_string_data:
    # JSON
    - text_filter: "{"
      filter_mode: STARTS_WITH
      then:
        - lambda: |-
            // Parse JSON

    # XML
    - text_filter: "<?xml"
      filter_mode: STARTS_WITH
      then:
        - lambda: |-
            // Parse XML

    # Plain text
    - then:
        - lambda: |-
            // Handle plain text
```

---

## Testing Commands

```bash
# Basic test (wait 1 second for response)
echo "ping" | nc -u -w1 192.168.1.10 8888

# Test with specific source port
echo "hello" | nc -p 54321 -u 192.168.1.10 8888

# Test different filters
echo "CMD:turn_on" | nc -u -w1 192.168.1.10 8888    # STARTS_WITH
echo "status?" | nc -u -w1 192.168.1.10 8888        # ENDS_WITH
echo "ping" | nc -u -w1 192.168.1.10 8888           # EQUALS
echo "ERROR in system" | nc -u -w1 192.168.1.10 8888 # CONTAINS

# Interactive mode (type and press Enter to send)
nc -u 192.168.1.10 8888

# Send without waiting for response
echo "relay_on" | nc -u -N 192.168.1.10 9999
```
```

---

## Debug Logging

Enable to see filter activity:

```yaml
logger:
  level: DEBUG
```

Output examples:
```
[D][udpserver:053] Received UDP packet: 10 bytes from 192.168.1.100:54321
[D][udpserver:055] IP 192.168.1.100 is allowed
[D][udpserver:028] Processing data, length=10, data=CMD:test
[D][udpserver:045] Text filter 'starts_with' matched
```

Or rejected:
```
[W][udpserver:059] Packet from 192.168.1.200 rejected by IP filter
[D][udpserver:045] Text filter 'contains' not matched: 'CMD:' not in 'test'
```

---

## Need Help?

1. Check logs: `logger: level: DEBUG`
2. Review examples: `examples/filtering.yaml`
3. Read guide: `FILTERING_GUIDE.md`
4. Test with netcat: 
   ```bash
   # Basic test
   echo "test" | nc -u -w1 192.168.1.10 8888
   
   # With source port
   echo "hello" | nc -p 54321 -u 192.168.1.10 8888
   ```
