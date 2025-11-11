/*
 * WiFiManager.h - Wi-Fi 管理
 *
 * AP モードと STA モードの管理、イベント処理を行います。
 */

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <WiFi.h>

// 状態管理変数の extern 宣言
extern unsigned long lastReconnectAttempt;
extern bool lastSTAConnected;

// Wi-Fi セットアップ関数
void setupAP();
void setupSTA();

// Wi-Fi イベントハンドラ
void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info);

// 再接続処理
void checkAndReconnectSTA();

#endif // WIFI_MANAGER_H
