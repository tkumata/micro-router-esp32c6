/*
 * DNSBlocklistStore.h - DNS ブロックリスト保持
 */

#ifndef DNS_BLOCKLIST_STORE_H
#define DNS_BLOCKLIST_STORE_H

#include <Arduino.h>
#include <LittleFS.h>
#include <vector>

class DNSBlocklistStore {
public:
  DNSBlocklistStore();
  ~DNSBlocklistStore();

  bool loadFromFile(const char* filepath);
  void clear();
  int count() const;
  bool contains(const String& domain) const;

private:
  char* buffer;
  size_t bufferSize;
  size_t bufferUsed;
  std::vector<const char*> domains;

  bool ensureBuffer();
  bool isValidDomain(const String& domain) const;
  bool normalizeLine(String& line) const;
  bool appendDomain(const String& domain);
};

#endif // DNS_BLOCKLIST_STORE_H
