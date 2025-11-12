/*
 * XIAO ESP32C6 Micro Wi-Fi Router - Phase 8 DNS フィルタリング版
 *
 * このスケッチは ESP32C6 を Wi-Fi ルーターとして動作させます。
 *
 * 機能:
 * - Wi-Fi AP モード（自分の Wi-Fi を提供）
 * - Wi-Fi STA モード（既存 Wi-Fi に接続）
 * - Web UI による設定（http://192.168.4.1）
 * - Preferences (NVS) による設定の不揮発性保存
 * - 初回起動と設定済み起動の自動判定
 * - NAT/NAPT によるパケット転送
 * - DNS フォワーディング
 * - DNS フィルタリング（広告ブロック）
 * - 自動再接続機能
 * - メモリ監視
 *
 * ハードウェア: Seeed Studio XIAO ESP32C6
 * ボード設定: XIAO_ESP32C6
 * シリアルボーレート: 115200
 */

#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <LittleFS.h>   // ファイルシステム（Phase 8）
#include <esp_netif.h>  // ESP-IDF netif API（NAT機能用）

#include "Config.h"
#include "Types.h"
#include "ConfigManager.h"
#include "WiFiManager.h"
#include "NATManager.h"
#include "WebUIManager.h"
#include "DNSFilterManager.h"  // Phase 8
#include "Utils.h"

// ===== 定数の実体定義 =====
const char* AP_SSID = "micro-router-esp32c6";
const uint8_t AP_CHANNEL = 6;
const uint8_t AP_MAX_CONNECTIONS = 3;

const IPAddress AP_IP(192, 168, 4, 1);
const IPAddress AP_GATEWAY(192, 168, 4, 1);
const IPAddress AP_SUBNET(255, 255, 255, 0);

const unsigned long STA_RECONNECT_INTERVAL = 5000;
const unsigned long STATUS_PRINT_INTERVAL = 10000;
const unsigned long STA_CONNECTION_TIMEOUT = 30000;
const unsigned long CONFIG_SAVE_DELAY = 5000;
const unsigned long NAT_ENABLE_DELAY = 1000;

const uint32_t MIN_FREE_HEAP_WARNING = 50000;

const char* PREF_NAMESPACE = "wifi-config";
const char* PREF_KEY_STA_SSID = "sta_ssid";
const char* PREF_KEY_STA_PASSWORD = "sta_password";
const char* PREF_KEY_AP_PASSWORD = "ap_password";
const char* PREF_KEY_CONFIGURED = "configured";
const char* PREF_KEY_AP_PASSWORD_SET = "ap_pw_set";
const char* PREF_KEY_DNS_FILTER_ENABLED = "dns_filter_en";  // Phase 8

const char* DEFAULT_AP_PASSWORD = "esp32c6router";

// ===== グローバル変数 =====
WebServer server(80);
Preferences preferences;
WifiConfig config;
DNSFilterManager dnsFilter;  // Phase 8

// ===== 状態管理変数 =====
unsigned long lastReconnectAttempt = 0;  // 最後の再接続試行時刻
unsigned long lastStatusPrint = 0;       // 最後のステータス表示時刻
bool lastSTAConnected = false;           // 前回の STA 接続状態
bool natEnabled = false;                 // NAT 有効化済みフラグ
bool needEnableNAT = false;              // NAT 有効化リクエストフラグ

// ===== 関数プロトタイプ =====
// (すべてヘッダーファイルに移動)

void setup() {
  // シリアル通信の初期化
  Serial.begin(115200);
  delay(1000);  // シリアルの安定化を待つ

  Serial.println();
  printSeparator("XIAO ESP32C6 マイクロ Wi-Fi ルーター");
  Serial.println();

  // LittleFS の初期化（Phase 8）
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS のマウントに失敗しました");
  } else {
    Serial.println("LittleFS を正常にマウントしました");
  }

  // WiFi イベントハンドラの登録
  WiFi.onEvent(onWiFiEvent);
  Serial.println("WiFi イベントハンドラを登録しました");

  // 設定の読み込み
  loadConfig();

  // 設定状態による起動モード分岐
  if (config.configured) {
    // 設定済み: STA + AP モード
    Serial.println("=== 設定済みモード ===");
    Serial.print("接続先 SSID: ");
    Serial.println(config.sta_ssid);

    WiFi.mode(WIFI_AP_STA);
    setupSTA();
    setupAP();

    // STA 接続状態を記録
    lastSTAConnected = (WiFi.status() == WL_CONNECTED);
  } else {
    // 未設定: AP モードのみ
    Serial.println("=== 初回起動モード ===");
    Serial.println("Web UI で Wi-Fi 設定を行ってください");
    Serial.println("http://192.168.4.1 にアクセスしてください");

    WiFi.mode(WIFI_AP);
    setupAP();
  }

  // Web サーバーの起動
  setupWebServer();

  // DNS フィルタの初期化（Phase 8）
  if (dnsFilter.begin()) {
    Serial.println("DNS フィルタを正常に起動しました");

    // DNS フィルタ設定の読み込み
    preferences.begin(PREF_NAMESPACE, true);
    bool dnsFilterEnabled = preferences.getBool(PREF_KEY_DNS_FILTER_ENABLED, false);
    preferences.end();

    dnsFilter.setEnabled(dnsFilterEnabled);
    Serial.printf("DNS フィルタ設定: %s\n", dnsFilterEnabled ? "有効" : "無効");
  } else {
    Serial.println("DNS フィルタの起動に失敗しました");
  }

  Serial.println();
  printSeparator("セットアップ完了");
  Serial.println();
}

// ===== ループ処理 =====

void loop() {
  // NAT 有効化リクエストの処理
  processNATEnableRequest();

  // DNS フィルタの処理（Phase 8）
  dnsFilter.handleClient();

  // Web サーバーのリクエスト処理
  server.handleClient();

  // STA 再接続処理
  checkAndReconnectSTA();

  // 定期的なステータス表示
  printPeriodicStatus();
}

