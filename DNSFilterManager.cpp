/*
 * DNSFilterManager.cpp - DNS フィルタリング機能の実装
 */

#include "DNSFilterManager.h"

DNSFilterManager::DNSFilterManager()
  : enabled(false), upstreamDNS(8, 8, 8, 8) {
  stats = {0, 0, 0, 0};
}

DNSFilterManager::~DNSFilterManager() {
  end();
}

bool DNSFilterManager::begin() {
  Serial.println("DNSFilterManager: DNS Proxy サーバーを起動中...");

  // UDP ポート 53 でリッスン開始
  if (!udp.begin(DNS_PORT)) {
    Serial.println("DNSFilterManager: UDP ポート 53 の起動に失敗しました");
    return false;
  }

  Serial.printf("DNSFilterManager: ポート %d でリッスン開始\n", DNS_PORT);

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
  uint8_t packet[DNS_MAX_PACKET_SIZE];
  int len = udp.read(packet, DNS_MAX_PACKET_SIZE);

  if (len < DNS_HEADER_SIZE) {
    Serial.println("DNSFilterManager: 不正な DNS パケット（サイズ不足）");
    stats.errorQueries++;
    return;
  }

  stats.totalQueries++;

  // ドメイン名を抽出
  String domain = extractDomainFromDNSQuery(packet, len);

  if (domain.length() == 0) {
    Serial.println("DNSFilterManager: ドメイン抽出に失敗");
    stats.errorQueries++;
    return;
  }

  Serial.printf("DNSFilterManager: %s からのクエリ: %s\n",
                clientIP.toString().c_str(), domain.c_str());

  // ブロックリストと照合
  if (isBlocked(domain)) {
    Serial.printf("DNSFilterManager: ブロック %s\n", domain.c_str());
    stats.blockedQueries++;
    sendBlockedResponse(packet, len, clientIP, clientPort);
  } else {
    Serial.printf("DNSFilterManager: 許可 %s\n", domain.c_str());
    stats.allowedQueries++;
    forwardToUpstream(packet, len, clientIP, clientPort);
  }
}

String DNSFilterManager::extractDomainFromDNSQuery(uint8_t* packet, size_t len) {
  if (len < DNS_HEADER_SIZE) {
    return "";
  }

  String domain = "";
  uint8_t* ptr = packet + DNS_HEADER_SIZE;  // ヘッダーをスキップ
  uint8_t* end = packet + len;

  while (ptr < end && *ptr != 0) {
    uint8_t labelLen = *ptr++;

    // 圧縮ポインタチェック（上位2ビットが11の場合）
    if ((labelLen & 0xC0) == 0xC0) {
      break;  // 圧縮ポインタは未対応（通常のクエリでは不要）
    }

    if (labelLen > 63 || ptr + labelLen > end) {
      return "";  // 不正なラベル長
    }

    if (domain.length() > 0) {
      domain += ".";
    }

    for (int i = 0; i < labelLen; i++) {
      domain += (char)*ptr++;
    }
  }

  domain.toLowerCase();
  return domain;
}

bool DNSFilterManager::isBlocked(const String& domain) {
  for (const String& blocked : blocklist) {
    if (domain == blocked || domain.endsWith("." + blocked)) {
      return true;
    }
  }
  return false;
}

void DNSFilterManager::sendBlockedResponse(uint8_t* query, size_t len,
                                            IPAddress clientIP, uint16_t clientPort) {
  // DNS 応答パケットを作成
  uint8_t response[DNS_MAX_PACKET_SIZE];
  memcpy(response, query, len);

  // フラグを設定（応答、権威あり、エラー無し）
  response[2] = 0x81;  // QR=1, Opcode=0, AA=0, TC=0, RD=1
  response[3] = 0x80;  // RA=1, Z=0, RCODE=0

  // Answer Count = 1
  response[6] = 0x00;
  response[7] = 0x01;

  // Answer Section を追加
  size_t answerOffset = len;

  // Name: 圧縮ポインタ (0xC00C = Query Section の先頭を指す)
  response[answerOffset++] = 0xC0;
  response[answerOffset++] = 0x0C;

  // Type: A (0x0001)
  response[answerOffset++] = 0x00;
  response[answerOffset++] = 0x01;

  // Class: IN (0x0001)
  response[answerOffset++] = 0x00;
  response[answerOffset++] = 0x01;

  // TTL: 300 秒
  response[answerOffset++] = 0x00;
  response[answerOffset++] = 0x00;
  response[answerOffset++] = 0x01;
  response[answerOffset++] = 0x2C;

  // Data Length: 4 bytes
  response[answerOffset++] = 0x00;
  response[answerOffset++] = 0x04;

  // Data: 0.0.0.0
  response[answerOffset++] = 0x00;
  response[answerOffset++] = 0x00;
  response[answerOffset++] = 0x00;
  response[answerOffset++] = 0x00;

  // クライアントに送信
  udp.beginPacket(clientIP, clientPort);
  udp.write(response, answerOffset);
  udp.endPacket();
}

void DNSFilterManager::forwardToUpstream(uint8_t* query, size_t len,
                                          IPAddress clientIP, uint16_t clientPort) {
  // 上流 DNS サーバーに転送
  WiFiUDP upstreamUdp;
  upstreamUdp.beginPacket(upstreamDNS, DNS_PORT);
  upstreamUdp.write(query, len);
  upstreamUdp.endPacket();

  // 応答を待つ（タイムアウト: 2秒）
  unsigned long startTime = millis();
  while (millis() - startTime < 2000) {
    int packetSize = upstreamUdp.parsePacket();
    if (packetSize > 0) {
      uint8_t response[DNS_MAX_PACKET_SIZE];
      int responseLen = upstreamUdp.read(response, DNS_MAX_PACKET_SIZE);

      // クライアントに転送
      udp.beginPacket(clientIP, clientPort);
      udp.write(response, responseLen);
      udp.endPacket();

      upstreamUdp.stop();
      return;
    }
    delay(10);
  }

  // タイムアウト
  Serial.println("DNSFilterManager: 上流 DNS タイムアウト");
  upstreamUdp.stop();
}

bool DNSFilterManager::loadBlocklistFromFile(const char* filepath) {
  if (!LittleFS.exists(filepath)) {
    Serial.printf("DNSFilterManager: ブロックリストファイルが見つかりません: %s\n", filepath);
    return false;
  }

  File file = LittleFS.open(filepath, "r");
  if (!file) {
    Serial.println("DNSFilterManager: ブロックリストを開けませんでした");
    return false;
  }

  clearBlocklist();

  int count = 0;
  while (file.available() && count < MAX_BLOCKLIST_SIZE) {
    String line = file.readStringUntil('\n');
    line.trim();

    // 空行とコメント行をスキップ
    if (line.length() == 0 || line.startsWith("#")) {
      continue;
    }

    // hosts 形式の場合は IP 部分を削除
    if (line.startsWith("0.0.0.0 ") || line.startsWith("127.0.0.1 ")) {
      int spaceIdx = line.indexOf(' ');
      if (spaceIdx > 0) {
        line = line.substring(spaceIdx + 1);
        line.trim();
      }
    }

    // ブロックリストに追加
    if (isValidDomain(line)) {
      line.toLowerCase();
      blocklist.push_back(line);
      count++;
    }
  }

  file.close();
  Serial.printf("DNSFilterManager: %s から %d ドメインを読み込みました\n", filepath, count);
  return true;
}

bool DNSFilterManager::reloadBlocklist() {
  Serial.println("DNSFilterManager: ブロックリストを再読み込み中...");
  return loadBlocklistFromFile();
}

void DNSFilterManager::clearBlocklist() {
  blocklist.clear();
}

int DNSFilterManager::getBlocklistCount() const {
  return blocklist.size();
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

bool DNSFilterManager::isValidDomain(const String& domain) {
  if (domain.length() < 3 || domain.length() > 253) {
    return false;
  }

  if (domain.indexOf(' ') >= 0 || domain.indexOf("..") >= 0) {
    return false;
  }

  if (domain.indexOf('.') < 0) {
    return false;
  }

  for (unsigned int i = 0; i < domain.length(); i++) {
    char c = domain.charAt(i);
    if (!isalnum(c) && c != '.' && c != '-' && c != '_') {
      return false;
    }
  }

  return true;
}
