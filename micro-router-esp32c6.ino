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
#include <ESPmDNS.h>    // mDNS 機能

#include "Config.h"
#include "Types.h"
#include "ConfigManager.h"
#include "WiFiManager.h"
#include "NATManager.h"
#include "WebUIManager.h"
#include "DNSFilterManager.h"  // Phase 8
#include "Utils.h"

// ===== グローバル変数 =====
WebServer server(WEB_SERVER_PORT);
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
void initializeFilesystem();
void startMDNSResponder();
void initializeDNSFilter();
void updateCaptivePortalState();

void initializeFilesystem() {
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS のマウントに失敗しました");
    return;
  }

  Serial.println("LittleFS を正常にマウントしました");
}

void startMDNSResponder() {
  if (MDNS.begin("micro-router")) {
    Serial.println("mDNS レスポンダーを開始しました: http://micro-router.local");
  } else {
    Serial.println("mDNS のセットアップに失敗しました");
  }
}

void initializeDNSFilter() {
  if (!dnsFilter.begin()) {
    Serial.println("DNS フィルタの起動に失敗しました");
    return;
  }

  Serial.println("DNS フィルタを正常に起動しました");

  bool dnsFilterEnabled = loadDNSFilterEnabled();
  dnsFilter.setEnabled(dnsFilterEnabled);
  Serial.printf("DNS フィルタ設定: %s\n", dnsFilterEnabled ? "有効" : "無効");

  if (!config.configured || WiFi.status() != WL_CONNECTED) {
    dnsFilter.setCaptivePortal(true);
  }
}

void updateCaptivePortalState() {
  if (!config.configured) {
    if (!dnsFilter.isCaptivePortal()) {
      dnsFilter.setCaptivePortal(true);
    }
    return;
  }

  bool currentlyConnected = (WiFi.status() == WL_CONNECTED);
  if (currentlyConnected && dnsFilter.isCaptivePortal()) {
    dnsFilter.setCaptivePortal(false);
    Serial.println("Captive Portal: 無効化 (インターネット接続復帰)");
  } else if (!currentlyConnected && !dnsFilter.isCaptivePortal()) {
    dnsFilter.setCaptivePortal(true);
    Serial.println("Captive Portal: 有効化 (インターネット接続ロスト)");
  }
}

void setup() {
  // シリアル通信の初期化
  Serial.begin(115200);
  delay(SERIAL_STABILIZE_DELAY);  // シリアルの安定化を待つ

  Serial.println();
  printSeparator("XIAO ESP32C6 マイクロ Wi-Fi ルーター");
  Serial.println();

  // LittleFS の初期化（Phase 8）
  initializeFilesystem();

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

  // mDNS の開始
  startMDNSResponder();

  // DNS フィルタの初期化（Phase 8）
  initializeDNSFilter();

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

  // キャプティブポータルの状態管理
  // ネット接続がない場合は強制的に設定画面へ誘導する
  updateCaptivePortalState();

  // 定期的なステータス表示
  printPeriodicStatus();
}
