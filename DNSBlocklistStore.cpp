/*
 * DNSBlocklistStore.cpp - DNS ブロックリスト保持の実装
 */

#include "DNSBlocklistStore.h"
#include "Config.h"

namespace {
constexpr size_t kMaxBlocklistEntries = 5000;
}

DNSBlocklistStore::DNSBlocklistStore()
  : buffer(nullptr),
    bufferSize(0),
    bufferUsed(0) {}

DNSBlocklistStore::~DNSBlocklistStore() {
  if (buffer) {
    free(buffer);
    buffer = nullptr;
  }
}

bool DNSBlocklistStore::loadFromFile(const char* filepath) {
  if (!LittleFS.exists(filepath)) {
    Serial.printf("DNSFilterManager: ブロックリストファイルが見つかりません: %s\n", filepath);
    return false;
  }

  File file = LittleFS.open(filepath, "r");
  if (!file) {
    Serial.println("DNSFilterManager: ブロックリストを開けませんでした");
    return false;
  }

  clear();
  if (!ensureBuffer()) {
    file.close();
    return false;
  }

  int countValue = 0;
  while (file.available() && countValue < static_cast<int>(kMaxBlocklistEntries)) {
    String line = file.readStringUntil('\n');
    if (!normalizeLine(line)) {
      continue;
    }

    if (!appendDomain(line)) {
      break;
    }
    countValue++;
  }

  file.close();
  Serial.printf("DNSFilterManager: %s から %d ドメインを読み込みました\n", filepath, countValue);
  Serial.printf("DNSFilterManager: バッファ使用量: %d / %d バイト (%.1f%%)\n",
                bufferUsed, bufferSize, (float)bufferUsed * 100.0 / bufferSize);
  return true;
}

void DNSBlocklistStore::clear() {
  domains.clear();
  bufferUsed = 0;
}

int DNSBlocklistStore::count() const {
  return static_cast<int>(domains.size());
}

bool DNSBlocklistStore::contains(const String& domain) const {
  for (const char* blocked : domains) {
    size_t blockedLen = strlen(blocked);
    size_t domainLen = domain.length();

    if (domain.equals(blocked)) {
      return true;
    }

    if (domainLen > blockedLen && domain.charAt(domainLen - blockedLen - 1) == '.' &&
        domain.endsWith(blocked)) {
      return true;
    }
  }

  return false;
}

bool DNSBlocklistStore::ensureBuffer() {
  if (buffer) {
    return true;
  }

  buffer = static_cast<char*>(malloc(BLOCKLIST_BUFFER_SIZE));
  if (!buffer) {
    Serial.println("DNSFilterManager: メモリ割り当てに失敗しました");
    return false;
  }

  bufferSize = BLOCKLIST_BUFFER_SIZE;
  bufferUsed = 0;
  return true;
}

bool DNSBlocklistStore::isValidDomain(const String& domain) const {
  if (domain.length() < DOMAIN_NAME_MIN_LENGTH || domain.length() > DOMAIN_NAME_MAX_LENGTH) {
    return false;
  }

  if (domain.indexOf(' ') >= 0 || domain.indexOf("..") >= 0 || domain.indexOf('.') < 0) {
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

bool DNSBlocklistStore::normalizeLine(String& line) const {
  line.trim();
  if (line.length() == 0 || line.startsWith("#")) {
    return false;
  }

  if (line.startsWith("0.0.0.0 ") || line.startsWith("127.0.0.1 ")) {
    int spaceIdx = line.indexOf(' ');
    if (spaceIdx > 0) {
      line = line.substring(spaceIdx + 1);
      line.trim();
    }
  }

  if (!isValidDomain(line)) {
    return false;
  }

  line.toLowerCase();
  return true;
}

bool DNSBlocklistStore::appendDomain(const String& domain) {
  size_t domainLen = domain.length() + 1;
  if (bufferUsed + domainLen > bufferSize) {
    Serial.println("DNSFilterManager: バッファが不足しています");
    return false;
  }

  char* domainPtr = buffer + bufferUsed;
  strcpy(domainPtr, domain.c_str());
  domains.push_back(domainPtr);
  bufferUsed += domainLen;
  return true;
}
