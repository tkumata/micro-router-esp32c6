/*
 * ConfigManager.cpp - 設定管理の実装
 *
 * Preferences (NVS) を使った設定の読み書きを実装します。
 */

#include "ConfigManager.h"
#include "Config.h"
#include <ESP.h>
#include <Arduino.h>
#include <esp_system.h>
#include <mbedtls/aes.h>
#include <mbedtls/md.h>

namespace {
constexpr size_t ENCRYPTION_KEY_SIZE = 32;
constexpr size_t ENCRYPTION_IV_SIZE = 16;
constexpr size_t ENCRYPTION_TAG_SIZE = 32;
constexpr size_t ENCRYPTION_STORAGE_MAX_LENGTH = 256;
const char* ENCRYPTED_VALUE_PREFIX = "enc1:";

int hexCharToNibble(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f') {
    return 10 + (c - 'a');
  }
  if (c >= 'A' && c <= 'F') {
    return 10 + (c - 'A');
  }
  return -1;
}

String bytesToHex(const uint8_t* data, size_t len) {
  static const char* hexDigits = "0123456789abcdef";
  String hex;
  hex.reserve(len * 2);

  for (size_t i = 0; i < len; ++i) {
    hex += hexDigits[(data[i] >> 4) & 0x0F];
    hex += hexDigits[data[i] & 0x0F];
  }

  return hex;
}

bool hexToBytes(const String& hex, uint8_t* output, size_t outputLen) {
  if (hex.length() != outputLen * 2) {
    return false;
  }

  for (size_t i = 0; i < outputLen; ++i) {
    int highNibble = hexCharToNibble(hex.charAt(i * 2));
    int lowNibble = hexCharToNibble(hex.charAt(i * 2 + 1));
    if (highNibble < 0 || lowNibble < 0) {
      return false;
    }
    output[i] = static_cast<uint8_t>((highNibble << 4) | lowNibble);
  }

  return true;
}

void fillRandomBytes(uint8_t* buffer, size_t len) {
  size_t offset = 0;
  while (offset < len) {
    uint32_t randomValue = esp_random();
    size_t chunkSize = min(sizeof(randomValue), len - offset);
    memcpy(buffer + offset, &randomValue, chunkSize);
    offset += chunkSize;
  }
}

bool deriveDeviceKey(uint8_t* key, size_t keyLen) {
  if (keyLen != ENCRYPTION_KEY_SIZE) {
    return false;
  }

  uint64_t efuseMac = ESP.getEfuseMac();
  const char* keyContext = "micro-router-nvs-v1";
  const size_t contextLen = strlen(keyContext);
  uint8_t material[32];
  memcpy(material, keyContext, contextLen);
  memcpy(material + contextLen, &efuseMac, sizeof(efuseMac));

  const mbedtls_md_info_t* mdInfo = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  if (mdInfo == nullptr) {
    return false;
  }

  return mbedtls_md(mdInfo, material, contextLen + sizeof(efuseMac), key) == 0;
}

bool computeHmac(const uint8_t* key, size_t keyLen,
                 const uint8_t* iv, size_t ivLen,
                 const uint8_t* payload, size_t payloadLen,
                 uint8_t* output, size_t outputLen) {
  if (outputLen != ENCRYPTION_TAG_SIZE) {
    return false;
  }

  const mbedtls_md_info_t* mdInfo = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  if (mdInfo == nullptr) {
    return false;
  }

  mbedtls_md_context_t ctx;
  mbedtls_md_init(&ctx);

  bool ok = mbedtls_md_setup(&ctx, mdInfo, 1) == 0 &&
            mbedtls_md_hmac_starts(&ctx, key, keyLen) == 0 &&
            mbedtls_md_hmac_update(&ctx, iv, ivLen) == 0 &&
            mbedtls_md_hmac_update(&ctx, payload, payloadLen) == 0 &&
            mbedtls_md_hmac_finish(&ctx, output) == 0;

  mbedtls_md_free(&ctx);
  return ok;
}

bool encryptStringForStorage(const String& plaintext, String& encryptedValue) {
  uint8_t key[ENCRYPTION_KEY_SIZE];
  if (!deriveDeviceKey(key, sizeof(key))) {
    return false;
  }

  uint8_t iv[ENCRYPTION_IV_SIZE];
  fillRandomBytes(iv, sizeof(iv));

  uint8_t ciphertext[WIFI_PASSWORD_BUFFER_SIZE];
  memset(ciphertext, 0, sizeof(ciphertext));
  size_t plaintextLen = plaintext.length();
  memcpy(ciphertext, plaintext.c_str(), plaintextLen);

  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);

  uint8_t nonceCounter[ENCRYPTION_IV_SIZE];
  uint8_t streamBlock[ENCRYPTION_IV_SIZE];
  size_t ncOffset = 0;
  memcpy(nonceCounter, iv, sizeof(iv));
  memset(streamBlock, 0, sizeof(streamBlock));

  bool ok = mbedtls_aes_setkey_enc(&aes, key, ENCRYPTION_KEY_SIZE * 8) == 0 &&
            mbedtls_aes_crypt_ctr(&aes, plaintextLen, &ncOffset, nonceCounter,
                                  streamBlock, ciphertext, ciphertext) == 0;
  mbedtls_aes_free(&aes);

  if (!ok) {
    return false;
  }

  uint8_t tag[ENCRYPTION_TAG_SIZE];
  if (!computeHmac(key, sizeof(key), iv, sizeof(iv), ciphertext, plaintextLen, tag, sizeof(tag))) {
    return false;
  }

  encryptedValue = String(ENCRYPTED_VALUE_PREFIX) +
                   bytesToHex(iv, sizeof(iv)) + ":" +
                   bytesToHex(ciphertext, plaintextLen) + ":" +
                   bytesToHex(tag, sizeof(tag));
  return true;
}

bool decryptStoredString(const String& encryptedValue, String& plaintext) {
  if (!encryptedValue.startsWith(ENCRYPTED_VALUE_PREFIX)) {
    return false;
  }

  int ivSeparator = encryptedValue.indexOf(':', strlen(ENCRYPTED_VALUE_PREFIX));
  int tagSeparator = encryptedValue.lastIndexOf(':');
  if (ivSeparator < 0 || tagSeparator <= ivSeparator) {
    return false;
  }

  String ivHex = encryptedValue.substring(strlen(ENCRYPTED_VALUE_PREFIX), ivSeparator);
  String cipherHex = encryptedValue.substring(ivSeparator + 1, tagSeparator);
  String tagHex = encryptedValue.substring(tagSeparator + 1);

  if (cipherHex.length() == 0 || cipherHex.length() > (WIFI_PASSWORD_MAX_LENGTH * 2)) {
    return false;
  }

  uint8_t key[ENCRYPTION_KEY_SIZE];
  uint8_t iv[ENCRYPTION_IV_SIZE];
  uint8_t tag[ENCRYPTION_TAG_SIZE];
  uint8_t expectedTag[ENCRYPTION_TAG_SIZE];
  uint8_t ciphertext[WIFI_PASSWORD_BUFFER_SIZE];
  uint8_t plaintextBytes[WIFI_PASSWORD_BUFFER_SIZE];

  size_t cipherLen = cipherHex.length() / 2;
  if (!deriveDeviceKey(key, sizeof(key)) ||
      !hexToBytes(ivHex, iv, sizeof(iv)) ||
      !hexToBytes(cipherHex, ciphertext, cipherLen) ||
      !hexToBytes(tagHex, tag, sizeof(tag))) {
    return false;
  }

  if (!computeHmac(key, sizeof(key), iv, sizeof(iv), ciphertext, cipherLen, expectedTag, sizeof(expectedTag)) ||
      memcmp(expectedTag, tag, sizeof(tag)) != 0) {
    return false;
  }

  memcpy(plaintextBytes, ciphertext, cipherLen);

  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);

  uint8_t nonceCounter[ENCRYPTION_IV_SIZE];
  uint8_t streamBlock[ENCRYPTION_IV_SIZE];
  size_t ncOffset = 0;
  memcpy(nonceCounter, iv, sizeof(iv));
  memset(streamBlock, 0, sizeof(streamBlock));

  bool ok = mbedtls_aes_setkey_enc(&aes, key, ENCRYPTION_KEY_SIZE * 8) == 0 &&
            mbedtls_aes_crypt_ctr(&aes, cipherLen, &ncOffset, nonceCounter,
                                  streamBlock, plaintextBytes, plaintextBytes) == 0;
  mbedtls_aes_free(&aes);

  if (!ok) {
    return false;
  }

  plaintextBytes[cipherLen] = '\0';
  plaintext = String(reinterpret_cast<char*>(plaintextBytes));
  return true;
}

bool saveEncryptedPreference(const char* key, const String& value) {
  String encryptedValue;
  if (!encryptStringForStorage(value, encryptedValue) ||
      encryptedValue.length() > ENCRYPTION_STORAGE_MAX_LENGTH) {
    return false;
  }

  return preferences.putString(key, encryptedValue) > 0;
}

String readProtectedPreference(const char* encryptedKey, const char* legacyKey,
                               const char* defaultValue, bool& migratedFromPlaintext) {
  String encryptedValue = preferences.getString(encryptedKey, "");
  if (encryptedValue.length() > 0) {
    String decryptedValue;
    if (decryptStoredString(encryptedValue, decryptedValue)) {
      return decryptedValue;
    }

    Serial.printf("警告: 暗号化設定の復号に失敗しました: %s\n", encryptedKey);
  }

  String legacyValue = preferences.getString(legacyKey, "");
  if (legacyValue.length() > 0) {
    migratedFromPlaintext = true;
    return legacyValue;
  }

  return String(defaultValue);
}

void migrateLegacyCredentials(const String& ssid, const String& staPassword, const String& apPassword,
                              bool migrateSTA, bool migrateAP) {
  if (!migrateSTA && !migrateAP) {
    return;
  }

  preferences.begin(PREF_NAMESPACE, false);

  if (migrateSTA) {
    if (saveEncryptedPreference(PREF_KEY_STA_SSID_ENC, ssid) &&
        saveEncryptedPreference(PREF_KEY_STA_PASSWORD_ENC, staPassword)) {
      preferences.remove(PREF_KEY_STA_SSID);
      preferences.remove(PREF_KEY_STA_PASSWORD);
      Serial.println("平文の STA 設定を暗号化形式へ移行しました");
    } else {
      Serial.println("警告: STA 設定の暗号化移行に失敗しました");
    }
  }

  if (migrateAP) {
    if (saveEncryptedPreference(PREF_KEY_AP_PASSWORD_ENC, apPassword)) {
      preferences.remove(PREF_KEY_AP_PASSWORD);
      Serial.println("平文の AP パスワードを暗号化形式へ移行しました");
    } else {
      Serial.println("警告: AP パスワードの暗号化移行に失敗しました");
    }
  }

  preferences.end();
}
}

/**
 * Preferences から設定を読み込む
 */
void loadConfig() {
  Serial.println("--- 設定読み込み開始 ---");

  preferences.begin(PREF_NAMESPACE, true);  // Read-only モード

  bool migrateSTA = false;
  bool migrateAP = false;
  String ssid = readProtectedPreference(PREF_KEY_STA_SSID_ENC, PREF_KEY_STA_SSID, "", migrateSTA);
  String password = readProtectedPreference(PREF_KEY_STA_PASSWORD_ENC, PREF_KEY_STA_PASSWORD, "", migrateSTA);
  String apPassword = readProtectedPreference(PREF_KEY_AP_PASSWORD_ENC, PREF_KEY_AP_PASSWORD, DEFAULT_AP_PASSWORD, migrateAP);
  config.configured = preferences.getBool(PREF_KEY_CONFIGURED, false);
  config.ap_password_set = preferences.getBool(PREF_KEY_AP_PASSWORD_SET, false);

  ssid.toCharArray(config.sta_ssid, WIFI_SSID_BUFFER_SIZE);
  password.toCharArray(config.sta_password, WIFI_PASSWORD_BUFFER_SIZE);
  apPassword.toCharArray(config.ap_password, WIFI_PASSWORD_BUFFER_SIZE);

  preferences.end();

  migrateLegacyCredentials(ssid, password, apPassword, migrateSTA, migrateAP);

  Serial.print("設定済みフラグ: ");
  Serial.println(config.configured ? "YES" : "NO");
  Serial.print("AP パスワード設定済み: ");
  Serial.println(config.ap_password_set ? "YES" : "NO (デフォルト使用)");
  if (config.configured) {
    Serial.print("保存されている SSID: ");
    Serial.println(config.sta_ssid);
  }

  Serial.println("--- 設定読み込み完了 ---");
  Serial.println();
}

/**
 * Preferences に設定を保存する
 */
bool saveConfig(const char* ssid, const char* password) {
  Serial.println("--- 設定保存開始 ---");

  preferences.begin(PREF_NAMESPACE, false);  // Read/Write モード

  bool encryptedSSIDSaved = saveEncryptedPreference(PREF_KEY_STA_SSID_ENC, String(ssid));
  bool encryptedPasswordSaved = saveEncryptedPreference(PREF_KEY_STA_PASSWORD_ENC, String(password));
  bool saveSucceeded = encryptedSSIDSaved && encryptedPasswordSaved;
  preferences.putBool(PREF_KEY_CONFIGURED, saveSucceeded);
  if (saveSucceeded) {
    preferences.remove(PREF_KEY_STA_SSID);
    preferences.remove(PREF_KEY_STA_PASSWORD);
  } else {
    Serial.println("警告: STA 設定の暗号化保存に失敗しました");
  }

  preferences.end();

  Serial.print("保存した SSID: ");
  Serial.println(ssid);
  Serial.println("パスワード: ********");
  Serial.println("設定済みフラグ: YES");

  Serial.println("--- 設定保存完了 ---");
  return saveSucceeded;
}

/**
 * AP パスワードを Preferences に保存する
 */
bool saveAPPassword(const char* password) {
  Serial.println("--- AP パスワード保存開始 ---");

  preferences.begin(PREF_NAMESPACE, false);  // Read/Write モード

  bool encryptedPasswordSaved = saveEncryptedPreference(PREF_KEY_AP_PASSWORD_ENC, String(password));
  preferences.putBool(PREF_KEY_AP_PASSWORD_SET, encryptedPasswordSaved);
  if (encryptedPasswordSaved) {
    preferences.remove(PREF_KEY_AP_PASSWORD);
  } else {
    Serial.println("警告: AP パスワードの暗号化保存に失敗しました");
  }

  preferences.end();

  Serial.println("AP パスワード: ********");
  Serial.println(encryptedPasswordSaved ? "AP パスワード設定済みフラグ: YES"
                                        : "AP パスワード設定済みフラグ: NO");

  Serial.println("--- AP パスワード保存完了 ---");
  return encryptedPasswordSaved;
}

bool loadDNSFilterEnabled() {
  preferences.begin(PREF_NAMESPACE, true);
  bool enabled = preferences.getBool(PREF_KEY_DNS_FILTER_ENABLED, false);
  preferences.end();
  return enabled;
}

void saveDNSFilterEnabled(bool enabled) {
  preferences.begin(PREF_NAMESPACE, false);
  preferences.putBool(PREF_KEY_DNS_FILTER_ENABLED, enabled);
  preferences.end();
}
