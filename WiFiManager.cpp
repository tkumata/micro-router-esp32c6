/*
 * WiFiManager.cpp - Wi-Fi 管理の実装
 *
 * AP モードと STA モードのセットアップ、イベント処理、再接続ロジックを実装します。
 */

#include "WiFiManager.h"
#include "Config.h"
#include "ConfigManager.h"
#include <Arduino.h>
#include <esp_netif.h>

// NATManager からの extern 宣言
extern bool natEnabled;
extern bool needEnableNAT;

/**
 * AP モードのセットアップ
 *
 * - 固定 IP アドレスの設定
 * - AP モードの起動
 * - DHCP サーバーの自動起動
 */
void setupAP() {
  Serial.println("--- AP モード設定開始 ---");

  // AP の固定 IP アドレスを設定
  if (!WiFi.softAPConfig(AP_IP, AP_GATEWAY, AP_SUBNET)) {
    Serial.println("エラー: AP IP 設定に失敗しました");
    return;
  }
  Serial.print("AP IP アドレス: ");
  Serial.println(AP_IP);

  // AP モードを起動（config.ap_password を使用）
  // WiFi.softAP(ssid, password, channel, ssid_hidden, max_connection)
  if (!WiFi.softAP(AP_SSID, config.ap_password, AP_CHANNEL, 0, AP_MAX_CONNECTIONS)) {
    Serial.println("エラー: AP 起動に失敗しました");
    return;
  }

  Serial.println("AP モード起動成功");
  Serial.print("SSID: ");
  Serial.println(AP_SSID);
  Serial.print("パスワード: ");
  Serial.println(config.ap_password_set ? "カスタム設定済み" : "デフォルト");
  Serial.print("チャンネル: ");
  Serial.println(AP_CHANNEL);
  Serial.print("最大接続数: ");
  Serial.println(AP_MAX_CONNECTIONS);

  // DHCP サーバーで DNS サーバーとして ESP32C6 自身を広告（Phase 8）
  esp_netif_t* ap_netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
  if (ap_netif != NULL) {
    // DNS サーバーとして自分自身（192.168.4.1）を設定
    esp_netif_dns_info_t dns_info;
    dns_info.ip.u_addr.ip4.addr = static_cast<uint32_t>(AP_IP);
    dns_info.ip.type = ESP_IPADDR_TYPE_V4;

    esp_netif_set_dns_info(ap_netif, ESP_NETIF_DNS_MAIN, &dns_info);

    Serial.println("DHCP: ESP32C6 を DNS サーバーとして設定（192.168.4.1）");
  } else {
    Serial.println("警告: AP netif の取得に失敗しました");
  }

  Serial.println("--- AP モード設定完了 ---");
  Serial.println();
}

/**
 * STA モードのセットアップ
 *
 * - 既存 Wi-Fi への接続
 * - IP アドレスの取得（DHCP）
 */
void setupSTA() {
  Serial.println("--- STA モード設定開始 ---");

  Serial.print("接続先: ");
  Serial.println(config.sta_ssid);
  Serial.print("接続中");

  WiFi.begin(config.sta_ssid, config.sta_password);

  // 接続待機（タイムアウト設定に従う）
  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED &&
         millis() - startAttemptTime < STA_CONNECTION_TIMEOUT) {
    Serial.print(".");
    delay(500);
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("STA 接続成功");
    Serial.print("STA IP アドレス: ");
    Serial.println(WiFi.localIP());
    Serial.print("サブネットマスク: ");
    Serial.println(WiFi.subnetMask());
    Serial.print("ゲートウェイ: ");
    Serial.println(WiFi.gatewayIP());
    Serial.print("DNS サーバー: ");
    Serial.println(WiFi.dnsIP());

    // NAT 機能は WiFi イベントハンドラで自動的に有効化されます
    Serial.println();
    Serial.println("WiFi イベント ARDUINO_EVENT_WIFI_STA_GOT_IP で NAT を有効化します");
  } else {
    Serial.print("エラー: STA 接続に失敗しました（");
    Serial.print(STA_CONNECTION_TIMEOUT / 1000);
    Serial.println("秒タイムアウト）");
    Serial.println("AP モードは引き続き動作します");
  }

  Serial.println("--- STA モード設定完了 ---");
  Serial.println();
}

/**
 * WiFi イベントハンドラ
 *
 * WiFi の各種イベントを処理します
 * - STA が IP アドレスを取得した時に NAT を有効化
 */
void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.println();
      Serial.println("=== WiFi イベント: STA が IP アドレスを取得 ===");
      Serial.print("IP アドレス: ");
      Serial.println(WiFi.localIP());

      // NAT をまだ有効化していない場合、フラグを立てる
      // 実際の有効化は loop() から行う
      if (!natEnabled) {
        Serial.println("NAT 有効化リクエストをセット（loop() で実行されます）");
        needEnableNAT = true;
      }
      break;

    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      Serial.println();
      Serial.println("=== WiFi イベント: STA 切断 ===");
      natEnabled = false;  // 切断時に NAT フラグをリセット
      break;

    case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
      Serial.println();
      Serial.println("=== WiFi イベント: AP にクライアント接続 ===");
      break;

    case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
      Serial.println();
      Serial.println("=== WiFi イベント: AP からクライアント切断 ===");
      break;

    default:
      break;
  }
}

/**
 * STA 接続を監視し、切断時に自動再接続を試みる
 */
void checkAndReconnectSTA() {
  if (!config.configured) {
    return;  // 未設定の場合は何もしない
  }

  bool staConnected = WiFi.status() == WL_CONNECTED;
  unsigned long now = millis();

  // STA が切断されている場合、再接続を試みる
  if (!staConnected && (now - lastReconnectAttempt > STA_RECONNECT_INTERVAL)) {
    Serial.println();
    Serial.println("STA 切断検知 - 再接続中...");
    WiFi.disconnect();
    WiFi.begin(config.sta_ssid, config.sta_password);
    lastReconnectAttempt = now;
  }

  // 切断から接続に変わった場合
  // WiFiイベントハンドラで自動的にNATが再有効化されます
  if (staConnected && !lastSTAConnected) {
    Serial.println();
    Serial.println("STA 再接続成功");
    Serial.println("（WiFi イベントハンドラが NAT を再有効化します）");
  }

  lastSTAConnected = staConnected;
}
