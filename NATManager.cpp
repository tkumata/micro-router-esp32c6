/*
 * NATManager.cpp - NAT/NAPT 機能管理の実装
 *
 * ESP-IDF の esp_netif API を使った NAT 有効化を実装します。
 * ESP32C6 では Arduino Core の lwIP NAT API が使えないため、
 * ESP-IDF API を直接使用します。
 */

#include "NATManager.h"
#include "Config.h"
#include <Arduino.h>
#include <esp_netif.h>

/**
 * NAT 有効化リクエストを処理する
 */
void processNATEnableRequest() {
  if (needEnableNAT) {
    needEnableNAT = false;
    Serial.println();
    Serial.println("loop() から NAT を有効化します...");
    delay(NAT_ENABLE_DELAY);  // ネットワークスタックの安定化を待つ
    enableNAT();
    natEnabled = true;
  }
}

/**
 * NAT/NAPT 機能を有効化
 *
 * - ESP-IDF の esp_netif API を使用
 * - IP フォワーディングを開始
 * - DNS フォワーディングは底層のネットワークスタックが自動処理
 */
void enableNAT() {
  Serial.println("--- NAT 有効化開始（ESP-IDF API使用） ---");

  // AP の netif ハンドルを取得
  // 複数のキー名を試す
  const char* ap_keys[] = {
    "WIFI_AP_DEF",
    "ESP_NETIF_WIFI_AP",
    "ap",
    "AP"
  };

  esp_netif_t* ap_netif = NULL;

  for (int i = 0; i < 4; i++) {
    ap_netif = esp_netif_get_handle_from_ifkey(ap_keys[i]);
    if (ap_netif != NULL) {
      Serial.print("AP netif をキー '");
      Serial.print(ap_keys[i]);
      Serial.println("' で取得しました");
      break;
    }
  }

  if (ap_netif == NULL) {
    Serial.println("エラー: AP netif ハンドルが見つかりません");
    Serial.println("利用可能なすべてのキーを試しましたが失敗しました");
    Serial.println("NAT 有効化を中止します");
    Serial.println();
    Serial.println("注意: ESP32C6 + Arduino Core 3.0 では NAPT 機能が");
    Serial.println("正常に動作しない可能性があります。");
    return;
  }

  Serial.println("AP netif ハンドル取得成功");

  // ESP-IDF の高レベル API で NAPT を有効化
  esp_err_t err = esp_netif_napt_enable(ap_netif);

  if (err == ESP_OK) {
    Serial.println("✓ NAT/NAPT 有効化成功！");
    Serial.println("AP に接続したデバイスはインターネットにアクセスできます");
  } else {
    Serial.print("エラー: NAT 有効化に失敗（エラーコード: 0x");
    Serial.print(err, HEX);
    Serial.println("）");

    // エラーコードの説明
    if (err == ESP_ERR_NOT_SUPPORTED) {
      Serial.println("理由: この機能はサポートされていません");
      Serial.println("ESP32C6 の現在の Arduino Core ではNAPTが無効化されている可能性があります");
    } else if (err == ESP_ERR_INVALID_ARG) {
      Serial.println("理由: 無効な引数");
    } else {
      Serial.println("理由: 不明なエラー");
    }
  }

  Serial.println("--- NAT 有効化処理完了 ---");
  Serial.println();
}
