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
#include "DNSBlocklistStore.h"

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

  void setCaptivePortal(bool enabled);
  bool isCaptivePortal() const;

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
  bool captivePortalEnabled;        // キャプティブポータルモード（全クエリに自分のIPを返す）

  DNSBlocklistStore blocklistStore;
  DNSStats stats;                   // 統計情報
  IPAddress upstreamDNS;            // 上流 DNS サーバー

  // ===== DNS パケット処理 =====
  void sendCustomIPResponse(uint8_t* query, size_t len, IPAddress clientIP, uint16_t clientPort, IPAddress responseIP);
  void forwardToUpstream(uint8_t* query, size_t len, IPAddress clientIP, uint16_t clientPort);
};

#endif // DNS_FILTER_MANAGER_H
