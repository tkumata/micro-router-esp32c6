/*
 * Types.h - 共通データ構造の定義
 *
 * このファイルはアプリケーション全体で使用されるデータ構造を定義します。
 */

#ifndef TYPES_H
#define TYPES_H

// 設定データ構造体
struct WifiConfig {
  char sta_ssid[33];      // SSID 最大 32 文字 + NULL
  char sta_password[65];  // パスワード最大 64 文字 + NULL
  char ap_password[65];   // AP パスワード最大 64 文字 + NULL
  bool configured;        // 設定済みフラグ
  bool ap_password_set;   // AP パスワード設定済みフラグ
};

#endif // TYPES_H
