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

// ===== Web サーバー設定 =====
extern const uint16_t WEB_SERVER_PORT;               // Web サーバーポート番号

// ===== シリアル通信設定 =====
extern const unsigned long SERIAL_STABILIZE_DELAY;   // シリアル通信安定化待機時間（ミリ秒）

// ===== 文字列長制限 =====
extern const size_t WIFI_SSID_MAX_LENGTH;            // WiFi SSID 最大長
extern const size_t WIFI_PASSWORD_MIN_LENGTH;        // WiFi パスワード最小長
extern const size_t WIFI_PASSWORD_MAX_LENGTH;        // WiFi パスワード最大長
extern const size_t WIFI_SSID_BUFFER_SIZE;           // WiFi SSID バッファサイズ（null終端含む）
extern const size_t WIFI_PASSWORD_BUFFER_SIZE;       // WiFi パスワードバッファサイズ（null終端含む）

// ===== WiFi 接続設定 =====
extern const uint8_t WIFI_SSID_HIDDEN;               // SSID 非表示フラグ（0=表示、1=非表示）
extern const unsigned long WIFI_CONNECTION_CHECK_DELAY; // WiFi 接続待機時のチェック間隔（ミリ秒）

// ===== HTTP ステータスコード =====
extern const uint16_t HTTP_STATUS_OK;                // 200 OK
extern const uint16_t HTTP_STATUS_SEE_OTHER;         // 303 See Other
extern const uint16_t HTTP_STATUS_BAD_REQUEST;       // 400 Bad Request
extern const uint16_t HTTP_STATUS_NOT_FOUND;         // 404 Not Found
extern const uint16_t HTTP_STATUS_INTERNAL_ERROR;    // 500 Internal Server Error

// ===== 単位換算定数 =====
extern const uint32_t BYTES_TO_KB_DIVISOR;           // バイトから KB への変換（1024）
extern const uint32_t MILLISECONDS_TO_SECONDS_DIVISOR; // ミリ秒から秒への変換（1000）
extern const uint8_t PERCENTAGE_MULTIPLIER;          // パーセント計算用乗数（100）

// ===== DNS 設定 =====
extern const IPAddress DEFAULT_UPSTREAM_DNS;         // デフォルト上流DNSサーバー（Google DNS 8.8.8.8）
extern const uint16_t DNS_FORWARD_TIMEOUT;           // DNS 転送タイムアウト（ミリ秒）
extern const unsigned long DNS_POLLING_INTERVAL;     // DNS ポーリング間隔（ミリ秒）

// ===== DNS パケット定数 =====
extern const uint8_t DNS_COMPRESSION_POINTER_MASK;   // DNS 圧縮ポインタマスク（0xC0）
extern const uint8_t DNS_LABEL_MAX_LENGTH;           // DNS ラベル最大長（63）
extern const uint8_t DNS_RESPONSE_FLAGS_BYTE2;       // DNS 応答フラグ byte 2（0x81）
extern const uint8_t DNS_RESPONSE_FLAGS_BYTE3;       // DNS 応答フラグ byte 3（0x80）
extern const uint16_t DNS_COMPRESSION_POINTER_QUERY; // DNS 圧縮ポインタ Query セクション先頭（0xC00C）
extern const uint16_t DNS_TYPE_A;                    // DNS Type A レコード（0x0001）
extern const uint16_t DNS_CLASS_IN;                  // DNS Class IN（0x0001）
extern const uint32_t DNS_TTL_SECONDS;               // DNS TTL（秒）
extern const uint8_t DNS_IPV4_ADDRESS_LENGTH;        // IPv4 アドレス長（4バイト）
extern const IPAddress DNS_BLOCKED_IP;               // ブロック時の返信IPアドレス（0.0.0.0）

// ===== ドメイン名検証定数 =====
extern const size_t DOMAIN_NAME_MIN_LENGTH;          // ドメイン名最小長
extern const size_t DOMAIN_NAME_MAX_LENGTH;          // ドメイン名最大長（253）

// ===== ブロックリストメモリ設定 =====
extern const size_t BLOCKLIST_BUFFER_SIZE;           // ブロックリスト文字列プールサイズ（バイト）

#endif // CONFIG_H
