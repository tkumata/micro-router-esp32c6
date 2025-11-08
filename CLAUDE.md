# CLAUDE.md

AI が思考中の文言も含め、全て日本語でやりとりしてください。
This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

XIAO ESP32C6 Micro Wi-Fi Router - A minimal Wi-Fi router implementation that connects to an existing Wi-Fi network (STA mode) while providing its own access point (AP mode), enabling NAT routing for up to 3 connected devices.

**Hardware**: Seeed Studio XIAO ESP32C6 (ESP32-C6 RISC-V, 512KB SRAM, 4MB Flash)

## Development Environment

### Required Setup

**Arduino IDE**: 2.0 or later

**Board Package**: ESP32 by Espressif Systems **version 3.0.0 or later** (CRITICAL for ESP32-C6 RISC-V support)

- Board Manager URL: `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
- Board Selection: `XIAO_ESP32C6`
- Serial Baud Rate: `115200`

**Required Libraries** (all included in ESP32 Arduino Core 3.0.0+):

- WiFi.h
- WebServer.h
- Preferences.h
- lwIP NAT (for NAT functionality)

### Build and Upload

```bash
# Standard Arduino IDE workflow:
# 1. Tools > Board > esp32 > XIAO_ESP32C6
# 2. Tools > Port > [Select appropriate port]
# 3. Sketch > Upload (or Ctrl+U)

# Serial Monitor
# Tools > Serial Monitor (or Ctrl+Shift+M)
# Set baud rate to 115200
```

## Architecture

### Dual Wi-Fi Mode Operation

The device runs in `WIFI_AP_STA` mode, operating two interfaces simultaneously:

**STA (Station) Mode**: Connects to existing home Wi-Fi as a client

- Obtains IP via DHCP (e.g., 192.168.1.100)
- Gets DNS server information from upstream router
- Serves as the WAN interface

**AP (Access Point) Mode**: Provides its own Wi-Fi network

- Fixed IP: 192.168.4.1/24
- SSID: `micro-router-esp32c6`
- Security: WPA2-PSK
- Max clients: 3
- DHCP range: 192.168.4.2 - 192.168.4.4
- Serves as the LAN interface

### NAT Routing

Packet flow: `Client (192.168.4.x) → AP Interface → NAT Engine (lwIP) → STA Interface → Internet`

The lwIP stack handles:

- NAT/NAPT (Port Address Translation)
- IP forwarding between AP and STA interfaces
- Dynamic port mapping (max 512 entries)
- DNS forwarding

NAT is enabled after successful STA connection using: `ip_napt_enable_no(SOFTAP_IF, 1)`

### Configuration Persistence

**Storage**: NVS (Non-Volatile Storage) via Preferences library

- Namespace: `"wifi-config"`
- Keys: `sta_ssid` (String), `sta_password` (String), `configured` (Bool)

**Startup Logic**:

- If `configured == false`: Start in AP-only mode, show Web UI for initial setup
- If `configured == true`: Load credentials, connect STA mode, start AP mode, enable NAT

### Web UI

**Server**: WebServer library on port 80, accessible at http://192.168.4.1

**Endpoints**:

- `GET /` - Status display + configuration form
- `POST /save` - Save WiFi credentials to NVS, then restart

**Status Information**:

- STA connection state and IP
- AP client count (current/max)
- Free heap memory

## Key Implementation Details

### Memory Constraints

The ESP32-C6 has limited RAM (~512KB total). Monitor heap usage:

```cpp
uint32_t freeHeap = ESP.getFreeHeap();
uint32_t minFreeHeap = ESP.getMinFreeHeap();
```

**Target**: Maintain >50KB free heap during operation to prevent crashes.

**Major memory consumers**:

- WiFi stack: ~50KB
- lwIP stack: ~30KB
- NAT table: ~25KB
- Web server: ~8KB

### STA Reconnection

Implement connection monitoring in `loop()`:

- Check `WiFi.status() != WL_CONNECTED`
- Auto-reconnect with backoff (e.g., 5-second intervals)
- Continue AP operation during STA disconnection

### Configuration Workflow

1. **First boot**: Device starts AP-only, user connects, configures via Web UI
2. **Save**: Credentials written to NVS with `Preferences.putString()`
3. **Restart**: `ESP.restart()` triggered after 3-second delay
4. **Subsequent boots**: Load config, connect STA, start AP, enable NAT

### lwIP NAT Configuration

NAT must be enabled AFTER successful STA connection:

```cpp
#include <lwip/lwip_napt.h>

if (WiFi.status() == WL_CONNECTED) {
  ip_napt_enable_no(SOFTAP_IF, 1);
}
```

**Note**: lwIP NAT is available in ESP32 Arduino Core 3.0.0+. Older versions lack this feature.

### Event Handling

Use WiFi events for connection management:

```cpp
WiFi.onEvent(WiFiStationConnected, ARDUINO_EVENT_WIFI_AP_STACONNECTED);
WiFi.onEvent(WiFiStationDisconnected, ARDUINO_EVENT_WIFI_AP_STADISCONNECTED);
```

## Common Development Tasks

### Testing NAT Functionality

From a device connected to the AP:

```bash
# Test basic connectivity
ping 8.8.8.8

# Test DNS resolution
ping google.com
nslookup google.com

# Test HTTP
curl http://google.com
```

### Debugging Connection Issues

Check serial output for:

- STA connection status and IP assignment
- NAT enablement confirmation
- DHCP server startup
- Web server binding to 192.168.4.1:80

### Measuring Performance

**Throughput**: Use iperf3

```bash
# On PC: iperf3 -s
# On AP client: iperf3 -c <PC_IP>
```

**Latency**: Compare ping times to external host with/without router

- Target: <10ms additional latency

### Clearing Stored Configuration

To reset WiFi credentials:

```cpp
preferences.begin("wifi-config", false);
preferences.clear();
preferences.end();
ESP.restart();
```

Or use `esptool.py erase_flash` for complete flash erase.

## File Structure

- `/micro-router-esp32c6.ino` - Main Arduino sketch (currently empty - implementation pending)
- `/docs/requirements.md` - Detailed requirements specification (in Japanese)
- `/docs/data_structure.md` - Network architecture and data structures (in Japanese)
- `/docs/implement_tasks.md` - Phase-by-phase implementation tasks (in Japanese)

## Implementation Status

The project is in the planning phase. The `.ino` file is currently empty. Implementation should follow the phased approach in `docs/implement_tasks.md`:

- **Phase 1**: Basic STA+AP setup
- **Phase 2**: Web UI and Preferences (NVS) integration
- **Phase 3**: NAT/routing enablement
- **Phase 4**: Error handling and stability
- **Phase 5**: Optimization and testing

## Critical Notes

1. **ESP32 Arduino Core 3.0.0+ is MANDATORY** - Earlier versions don't support ESP32-C6 or lack lwIP NAT
2. **NAT timing**: Enable NAT only after STA successfully connects and obtains an IP
3. **Memory management**: Continuously monitor free heap; <50KB indicates potential instability
4. **Concurrent modes**: Use `WiFi.mode(WIFI_AP_STA)` for simultaneous AP and STA operation
5. **Preferences namespace**: Always use `"wifi-config"` for consistency across boots
6. **Web server loop**: `server.handleClient()` must be called in `loop()` for request processing
7. **Password security**: NVS provides automatic encryption for stored credentials

## Known Limitations

- Max 3 simultaneous AP clients (hardware constraint)
- Target throughput: >5 Mbps (varies with RF environment)
- No WPA3 support (using WPA2-PSK for compatibility)
- Web UI has no authentication (protected by AP connection requirement)
- NAT table limited to 512 concurrent mappings
