# 開発タスクとロードマップ

## フェーズ概要

| Phase   | Duration | Objective                      | Status  |
| ------- | -------- | ------------------------------ | ------- |
| Phase 1 | 1-2 days | Basic Wi-Fi connectivity       | ✅ Done |
| Phase 2 | 1-2 days | Web UI & EEPROM functionality  | ✅ Done |
| Phase 3 | 2-3 days | NAT & Routing implementation   | ✅ Done |
| Phase 4 | 1-2 days | Error handling & Stabilization | ✅ Done |
| Phase 5 | 1 day    | Optimization & Testing         | ✅ Done |
| Phase 6 | 1 day    | AP Password persistence        | ✅ Done |
| Phase 7 | 2-3 hrs  | Code Refactoring               | ✅ Done |
| Phase 8 | 2-3 days | DNS Filtering implementation   | ✅ Done |

---

## 完了したタスク

### Phase 1: 基本実装

- [x] Environment Setup (Arduino IDE, ESP32 Core v3.0.0+).
- [x] AP Mode implementation (Fixed IP: 192.168.4.1).
- [x] DHCP Server verification.

### Phase 2: Web UI とストレージ

- [x] Preferences (NVS) integration for SSID/Pass.
- [x] Web Server (Port 80) implementation.
- [x] Configuration UI (HTML/CSS).
- [x] Save & Reboot logic.

### Phase 3: NAT とルーティング

- [x] Enable lwIP NAPT using `esp_netif` API.
- [x] DNS Forwarding setup.
- [x] Packet forwarding verification (Ping/HTTP).

### Phase 4: 安定化

- [x] STA auto-reconnect logic (5s interval).
- [x] Memory leak monitoring (Heap check).
- [x] Client connection limits (Max 3).

### Phase 5: 最適化

- [x] Performance tuning (Throughput/Latency checks).
- [x] Long-run stability test.

### Phase 6: セキュリティ

- [x] Custom AP password support.
- [x] Password saved in NVS.

### Phase 8: DNS フィルタリング (広告ブロック)

- [x] **DNSFilterManager**: Custom DNS Proxy on UDP:53.
- [x] **Blocklist Engine**: Memory-optimized buffer for ~5000 domains.
- [x] **File System**: LittleFS integration for blocklist storage.
- [x] **Web UI**:
  - Stats dashboard (Queries/Blocked/Allowed).
  - Blocklist uploader (Multipart form).
  - Toggle switch (Enable/Disable).
- [x] **Tools**: Python script to convert Adblock lists to `domain.txt`.

---

## 将来のロードマップ (Wishlist)

- [ ] **HTTPS / TLS Support**: Secure the Web UI (Self-signed certs).
- [ ] **Wildcard Blocking**: Support `*.example.com` in blocklists (requires Trie implementation).
- [ ] **Traffic Stats**: Visual graphs for bandwidth usage.
- [ ] **OTA Update**: Firmware update via Web UI.
