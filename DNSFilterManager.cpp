/*
 * DNSFilterManager.cpp - DNS フィルタリング機能の実装
 */

#include "DNSFilterManager.h"
#include "Config.h"
#include "DNSPacketCodec.h"

namespace {
constexpr uint16_t kDnsPort = 53;
constexpr size_t kDnsMaxPacketSize = 512;
}

DNSFilterManager::DNSFilterManager()
  : enabled(false),
    captivePortalEnabled(false),
    upstreamDNS(DEFAULT_UPSTREAM_DNS) {
  stats = {0, 0, 0, 0};
}

DNSFilterManager::~DNSFilterManager() { end(); }

bool DNSFilterManager::begin() {
  Serial.println("DNSFilterManager: DNS Proxy サーバーを起動中...");

  // UDP ポート 53 でリッスン開始
  if (!udp.begin(kDnsPort)) {
    Serial.println("DNSFilterManager: UDP ポート 53 の起動に失敗しました");
    return false;
  }

  Serial.printf("DNSFilterManager: ポート %d でリッスン開始\n", kDnsPort);

  // ブロックリストを読み込み
  loadBlocklistFromFile();

  enabled = true;
  return true;
}

void DNSFilterManager::end() {
  udp.stop();
  enabled = false;
}

void DNSFilterManager::handleClient() {
  if (!enabled) {
    return;
  }

  int packetSize = udp.parsePacket();
  if (packetSize == 0) {
    return;  // パケット無し
  }

  // クライアント情報
  IPAddress clientIP = udp.remoteIP();
  uint16_t clientPort = udp.remotePort();

  // DNS クエリパケットを読み込み
  uint8_t packet[kDnsMaxPacketSize];
  int len = udp.read(packet, kDnsMaxPacketSize);

  if (len < 12) {
    Serial.println("DNSFilterManager: 不正な DNS パケット（サイズ不足）");
    stats.errorQueries++;
    return;
  }

  stats.totalQueries++;

  // ドメイン名を抽出
  String domain = DNSPacketCodec::extractDomainFromQuery(packet, len);

  if (domain.length() == 0) {
    Serial.println("DNSFilterManager: ドメイン抽出に失敗");
    stats.errorQueries++;
    return;
  }

  Serial.printf("DNSFilterManager: %s からのクエリ: %s\n",
                clientIP.toString().c_str(), domain.c_str());

  // キャプティブポータルモード: すべてのクエリに自分(AP_IP)を返す
  if (captivePortalEnabled) {
    // 自分自身(micro-router.local)へのクエリは常に許可(正しいIPを返す)したいが、
    // mDNSはUDP 5353なのでここには来ない。
    // 通常のDNSクエリとして来た場合も、AP_IPを返せば設定画面が開くのでOK。
    Serial.printf("CaptivePortal: %s -> %s\n", domain.c_str(), AP_IP.toString().c_str());
    sendCustomIPResponse(packet, len, clientIP, clientPort, AP_IP);
    return;
  }

  // ブロックリストと照合
  if (blocklistStore.contains(domain)) {
    Serial.printf("DNSFilterManager: ブロック %s\n", domain.c_str());
    stats.blockedQueries++;
    sendCustomIPResponse(packet, len, clientIP, clientPort, DNS_BLOCKED_IP);
  } else {
    Serial.printf("DNSFilterManager: 許可 %s\n", domain.c_str());
    stats.allowedQueries++;
    forwardToUpstream(packet, len, clientIP, clientPort);
  }
}

void DNSFilterManager::sendCustomIPResponse(uint8_t* query, size_t len,
                                            IPAddress clientIP, uint16_t clientPort, IPAddress responseIP) {
  uint8_t response[kDnsMaxPacketSize];
  size_t responseLen = DNSPacketCodec::buildIPv4Response(response, query, len, responseIP);
  if (responseLen == 0) {
    stats.errorQueries++;
    return;
  }

  udp.beginPacket(clientIP, clientPort);
  udp.write(response, responseLen);
  udp.endPacket();
}

void DNSFilterManager::forwardToUpstream(uint8_t* query, size_t len,
                                          IPAddress clientIP, uint16_t clientPort) {
  // 上流 DNS サーバーに転送
  WiFiUDP upstreamUdp;
  upstreamUdp.beginPacket(upstreamDNS, kDnsPort);
  upstreamUdp.write(query, len);
  upstreamUdp.endPacket();

  // 応答を待つ
  unsigned long startTime = millis();
  while (millis() - startTime < DNS_FORWARD_TIMEOUT) {
    int packetSize = upstreamUdp.parsePacket();
    if (packetSize > 0) {
      uint8_t response[kDnsMaxPacketSize];
      int responseLen = upstreamUdp.read(response, kDnsMaxPacketSize);

      // クライアントに転送
      udp.beginPacket(clientIP, clientPort);
      udp.write(response, responseLen);
      udp.endPacket();

      upstreamUdp.stop();
      return;
    }
    delay(DNS_POLLING_INTERVAL);
  }

  // タイムアウト
  Serial.println("DNSFilterManager: 上流 DNS タイムアウト");
  upstreamUdp.stop();
}

bool DNSFilterManager::loadBlocklistFromFile(const char* filepath) {
  return blocklistStore.loadFromFile(filepath);
}

bool DNSFilterManager::reloadBlocklist() {
  Serial.println("DNSFilterManager: ブロックリストを再読み込み中...");
  return loadBlocklistFromFile();
}

void DNSFilterManager::clearBlocklist() {
  blocklistStore.clear();
}

int DNSFilterManager::getBlocklistCount() const {
  return blocklistStore.count();
}

void DNSFilterManager::setCaptivePortal(bool enable) {
  captivePortalEnabled = enable;
  Serial.printf("DNSFilterManager: キャプティブポータルモード %s\n", captivePortalEnabled ? "有効" : "無効");
}

bool DNSFilterManager::isCaptivePortal() const {
  return captivePortalEnabled;
}

void DNSFilterManager::setEnabled(bool enable) {
  enabled = enable;
  Serial.printf("DNSFilterManager: %s\n", enabled ? "有効化" : "無効化");
}

bool DNSFilterManager::isEnabled() const {
  return enabled;
}

DNSStats DNSFilterManager::getStats() const {
  return stats;
}

void DNSFilterManager::resetStats() {
  stats = {0, 0, 0, 0};
  Serial.println("DNSFilterManager: 統計情報をリセットしました");
}
