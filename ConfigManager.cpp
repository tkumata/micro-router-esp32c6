/*
 * ConfigManager.cpp - 設定管理の実装
 *
 * Preferences (NVS) を使った設定の読み書きを実装します。
 */

#include "ConfigManager.h"
#include "Config.h"
#include <Arduino.h>

/**
 * Preferences から設定を読み込む
 */
void loadConfig() {
  Serial.println("--- 設定読み込み開始 ---");

  preferences.begin(PREF_NAMESPACE, true);  // Read-only モード

  String ssid = preferences.getString(PREF_KEY_STA_SSID, "");
  String password = preferences.getString(PREF_KEY_STA_PASSWORD, "");
  String apPassword = preferences.getString(PREF_KEY_AP_PASSWORD, DEFAULT_AP_PASSWORD);
  config.configured = preferences.getBool(PREF_KEY_CONFIGURED, false);
  config.ap_password_set = preferences.getBool(PREF_KEY_AP_PASSWORD_SET, false);

  ssid.toCharArray(config.sta_ssid, WIFI_SSID_BUFFER_SIZE);
  password.toCharArray(config.sta_password, WIFI_PASSWORD_BUFFER_SIZE);
  apPassword.toCharArray(config.ap_password, WIFI_PASSWORD_BUFFER_SIZE);

  preferences.end();

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
void saveConfig(const char* ssid, const char* password) {
  Serial.println("--- 設定保存開始 ---");

  preferences.begin(PREF_NAMESPACE, false);  // Read/Write モード

  preferences.putString(PREF_KEY_STA_SSID, ssid);
  preferences.putString(PREF_KEY_STA_PASSWORD, password);
  preferences.putBool(PREF_KEY_CONFIGURED, true);

  preferences.end();

  Serial.print("保存した SSID: ");
  Serial.println(ssid);
  Serial.println("パスワード: ********");
  Serial.println("設定済みフラグ: YES");

  Serial.println("--- 設定保存完了 ---");
}

/**
 * AP パスワードを Preferences に保存する
 */
void saveAPPassword(const char* password) {
  Serial.println("--- AP パスワード保存開始 ---");

  preferences.begin(PREF_NAMESPACE, false);  // Read/Write モード

  preferences.putString(PREF_KEY_AP_PASSWORD, password);
  preferences.putBool(PREF_KEY_AP_PASSWORD_SET, true);

  preferences.end();

  Serial.println("AP パスワード: ********");
  Serial.println("AP パスワード設定済みフラグ: YES");

  Serial.println("--- AP パスワード保存完了 ---");
}
