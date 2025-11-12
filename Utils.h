/*
 * Utils.h - ユーティリティ関数
 *
 * デバッグ、ログ出力、ステータス表示などの汎用機能を提供します。
 */

#ifndef UTILS_H
#define UTILS_H

#include <Arduino.h>
#include <WiFi.h>
#include <ESP.h>
#include "Config.h"

// 状態管理変数の extern 宣言
extern unsigned long lastStatusPrint;

/**
 * メモリ情報を表示する
 */
inline void printMemoryInfo() {
  uint32_t freeHeap = ESP.getFreeHeap();
  uint32_t minFreeHeap = ESP.getMinFreeHeap();

  Serial.println("=== メモリ情報 ===");
  Serial.print("空きヒープ: ");
  Serial.print(freeHeap);
  Serial.println(" バイト");
  Serial.print("最小空きヒープ: ");
  Serial.print(minFreeHeap);
  Serial.println(" バイト");

  if (freeHeap < MIN_FREE_HEAP_WARNING) {
    Serial.println("⚠ 警告: メモリ不足");
  }
}

/**
 * 区切り線を表示する
 *
 * @param title タイトル（省略可）
 */
inline void printSeparator(const char* title = nullptr) {
  Serial.println("========================================");
  if (title != nullptr && strlen(title) > 0) {
    Serial.println(title);
    Serial.println("========================================");
  }
}

/**
 * 定期的にステータス情報を表示する
 */
inline void printPeriodicStatus() {
  unsigned long now = millis();

  if (now - lastStatusPrint >= STATUS_PRINT_INTERVAL) {
    int clientCount = WiFi.softAPgetStationNum();
    bool staConnected = WiFi.status() == WL_CONNECTED;
    uint32_t freeHeap = ESP.getFreeHeap();
    uint32_t minFreeHeap = ESP.getMinFreeHeap();

    Serial.print("[");
    Serial.print(now / MILLISECONDS_TO_SECONDS_DIVISOR);
    Serial.print("s] AP クライアント: ");
    Serial.print(clientCount);
    Serial.print("/");
    Serial.print(AP_MAX_CONNECTIONS);
    Serial.print(" | STA: ");
    Serial.print(staConnected ? "接続" : "未接続");
    if (staConnected) {
      Serial.print(" (");
      Serial.print(WiFi.localIP());
      Serial.print(")");
    }
    Serial.print(" | メモリ: ");
    Serial.print(freeHeap / BYTES_TO_KB_DIVISOR);
    Serial.print(" KB (最小: ");
    Serial.print(minFreeHeap / BYTES_TO_KB_DIVISOR);
    Serial.print(" KB)");

    // メモリ不足の警告
    if (freeHeap < MIN_FREE_HEAP_WARNING) {
      Serial.print(" ⚠ 警告: メモリ不足");
    }

    Serial.println();

    lastStatusPrint = now;
  }
}

#endif // UTILS_H
