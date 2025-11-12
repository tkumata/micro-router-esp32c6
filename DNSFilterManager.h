/*
 * DNSFilterManager.h - DNS フィルタリング機能
 *
 * このモジュールは DNS クエリをインターセプトし、
 * ブロックリストに基づいてドメインレベルのフィルタリングを行います。
 */

#ifndef DNS_FILTER_MANAGER_H
#define DNS_FILTER_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <LittleFS.h>
#include <vector>

// ===== DNS パケット定数 =====
#define DNS_PORT 53
#define DNS_MAX_PACKET_SIZE 512
#define DNS_HEADER_SIZE 12
#define MAX_BLOCKLIST_SIZE 5000

// ===== 統計情報構造体 =====
struct DNSStats {
  uint32_t totalQueries;     // 総クエリ数
  uint32_t blockedQueries;   // ブロック数
  uint32_t allowedQueries;   // 許可数
  uint32_t errorQueries;     // エラー数
};

/**
 * DNSFilterManager クラス
 *
 * DNS Proxy サーバーとして動作し、ブロックリストに基づいて
 * ドメインをフィルタリングします。
 */
class DNSFilterManager {
public:
  DNSFilterManager();
  ~DNSFilterManager();

  // ===== 初期化・終了 =====
  bool begin();
  void end();

  // ===== メインループ処理 =====
  void handleClient();

  // ===== 設定 =====
  void setEnabled(bool enabled);
  bool isEnabled() const;

  // ===== ブロックリスト管理 =====
  bool loadBlocklistFromFile(const char* filepath = "/blocklist.txt");
  bool reloadBlocklist();
  void clearBlocklist();
  int getBlocklistCount() const;

  // ===== 統計情報 =====
  DNSStats getStats() const;
  void resetStats();

private:
  WiFiUDP udp;                      // DNS サーバー用 UDP
  bool enabled;                     // フィルタ有効フラグ
  std::vector<String> blocklist;    // ブロックリスト
  DNSStats stats;                   // 統計情報
  IPAddress upstreamDNS;            // 上流 DNS サーバー

  // ===== DNS パケット処理 =====
  String extractDomainFromDNSQuery(uint8_t* packet, size_t len);
  bool isBlocked(const String& domain);
  void sendBlockedResponse(uint8_t* query, size_t len, IPAddress clientIP, uint16_t clientPort);
  void forwardToUpstream(uint8_t* query, size_t len, IPAddress clientIP, uint16_t clientPort);

  // ===== ユーティリティ =====
  bool isValidDomain(const String& domain);
};

#endif // DNS_FILTER_MANAGER_H
