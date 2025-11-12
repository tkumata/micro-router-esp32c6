/*
 * Config.h - 定数定義とグローバル設定
 *
 * このファイルはアプリケーション全体で使用される定数を定義します。
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <IPAddress.h>

// ===== AP モード設定 =====
extern const char* AP_SSID;
extern const uint8_t AP_CHANNEL;
extern const uint8_t AP_MAX_CONNECTIONS;

// ===== IP アドレス設定 =====
extern const IPAddress AP_IP;
extern const IPAddress AP_GATEWAY;
extern const IPAddress AP_SUBNET;

// ===== タイミング設定 =====
extern const unsigned long STA_RECONNECT_INTERVAL;   // STA再接続間隔（ミリ秒）
extern const unsigned long STATUS_PRINT_INTERVAL;    // ステータス表示間隔（ミリ秒）
extern const unsigned long STA_CONNECTION_TIMEOUT;   // STA接続タイムアウト（ミリ秒）
extern const unsigned long CONFIG_SAVE_DELAY;        // 設定保存後の再起動遅延（ミリ秒）
extern const unsigned long NAT_ENABLE_DELAY;         // NAT有効化前の遅延（ミリ秒）

// ===== メモリ管理設定 =====
extern const uint32_t MIN_FREE_HEAP_WARNING;  // メモリ不足警告閾値（バイト）

// ===== Preferences 設定 =====
extern const char* PREF_NAMESPACE;
extern const char* PREF_KEY_STA_SSID;
extern const char* PREF_KEY_STA_PASSWORD;
extern const char* PREF_KEY_AP_PASSWORD;
extern const char* PREF_KEY_CONFIGURED;
extern const char* PREF_KEY_AP_PASSWORD_SET;

// ===== デフォルト値 =====
extern const char* DEFAULT_AP_PASSWORD;

// ===== DNS フィルタリング設定 (Phase 8) =====
extern const char* PREF_KEY_DNS_FILTER_ENABLED;

#endif // CONFIG_H
