/*
 * WebUIManager.cpp - Web UI 管理の実装
 *
 * HTTP サーバーのエンドポイント処理と HTML 生成を実装します。
 */

#include "WebUIManager.h"
#include "Config.h"
#include "ConfigManager.h"
#include "DNSFilterManager.h"  // Phase 8
#include "WebUIRenderer.h"
#include <Arduino.h>
#include <ESP.h>
#include <LittleFS.h>  // Phase 8

namespace {
const char* BLOCKLIST_PATH = "/blocklist.txt";
const char* BLOCKLIST_UPLOAD_PATH = "/blocklist.txt.tmp";
const char* BLOCKLIST_BACKUP_PATH = "/blocklist.txt.bak";

struct BlocklistUploadState {
  bool failed;
  String message;
};

BlocklistUploadState gBlocklistUploadState = {false, ""};

void markBlocklistUploadFailure(const String& message) {
  gBlocklistUploadState.failed = true;
  gBlocklistUploadState.message = message;
}

void restartDeviceWithLog(const char* logTitle) {
  Serial.println();
  Serial.println("========================================");
  Serial.println(logTitle);
  Serial.println("========================================");
  Serial.print(CONFIG_SAVE_DELAY / MILLISECONDS_TO_SECONDS_DIVISOR);
  Serial.println("秒後に再起動します");
  Serial.println("========================================");

  delay(CONFIG_SAVE_DELAY);
  ESP.restart();
}

bool replaceBlocklistFile() {
  // 古いバックアップを削除
  LittleFS.remove(BLOCKLIST_BACKUP_PATH);

  bool backupCreated = false;

  // 既存のブロックリストがあればバックアップを作成
  if (LittleFS.exists(BLOCKLIST_PATH)) {
    if (!LittleFS.rename(BLOCKLIST_PATH, BLOCKLIST_BACKUP_PATH)) {
      // バックアップ作成に失敗したら、安全のため置換処理自体を中止
      return false;
    }
    backupCreated = true;
  }

  // アップロードされた一時ファイルを本体パスへリネーム
  if (LittleFS.rename(BLOCKLIST_UPLOAD_PATH, BLOCKLIST_PATH)) {
    return true;
  }

  LittleFS.remove(BLOCKLIST_UPLOAD_PATH);

  // 失敗時は、可能であればバックアップからロールバック
  if (backupCreated && LittleFS.exists(BLOCKLIST_BACKUP_PATH)) {
    if (!LittleFS.rename(BLOCKLIST_BACKUP_PATH, BLOCKLIST_PATH)) {
      return false;
    }
  }

  return false;
}
}

// グローバル変数の extern 宣言
extern DNSFilterManager dnsFilter;  // Phase 8

/**
 * Web サーバーのセットアップ
 */
void setupWebServer() {
  server.on("/", handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/save_ap_password", HTTP_POST, handleSaveAPPassword);

  // DNS フィルタエンドポイント（Phase 8）
  server.on("/dns-filter", handleDNSFilter);
  server.on("/dns-filter-toggle", HTTP_POST, handleDNSFilterToggle);
  server.on("/download-blocklist", HTTP_GET, handleDownloadBlocklist);
  server.on("/upload-blocklist", HTTP_POST,
    []() {
      if (gBlocklistUploadState.failed) {
        server.send(HTTP_STATUS_INTERNAL_ERROR, "text/html",
                    buildSimpleMessagePage("アップロード失敗", gBlocklistUploadState.message, "/dns-filter"));
      } else {
        server.send(HTTP_STATUS_OK, "text/html", buildUploadSuccessPage());
      }
    },
    handleUploadBlocklist
  );

  // キャプティブポータル対策: 未知のURLはすべてルートへリダイレクト
  server.onNotFound([]() {
    String host = server.hostHeader();
    Serial.printf("Unknown request: %s (Host: %s)\n", server.uri().c_str(), host.c_str());

    // キャプティブポータル検出用URLの場合、またはIP直打ちでない場合
    if (!host.equals(AP_IP.toString())) {
      server.sendHeader("Location", String("http://") + AP_IP.toString() + "/", true);
      server.send(302, "text/plain", "");
    } else {
      // 自分のIP宛だが存在しないパスの場合は404
      server.send(HTTP_STATUS_NOT_FOUND, "text/plain", "Not Found");
    }
  });

  server.begin();

  Serial.println();
  Serial.println("Web サーバー起動: http://192.168.4.1");
}

/**
 * Web UI のルートページ（ステータス表示 + 設定フォーム）
 * PROGMEM テンプレートを使用してメモリを最適化
 */
void handleRoot() {
  server.send(HTTP_STATUS_OK, "text/html", renderRootPage());
}

/**
 * 設定保存エンドポイント（POST /save）
 */
void handleSave() {
  // フォームデータ取得
  String ssid = server.arg("ssid");
  String password = server.arg("password");

  // 入力検証
  if (ssid.length() == 0 || ssid.length() > WIFI_SSID_MAX_LENGTH) {
    server.send(HTTP_STATUS_BAD_REQUEST, "text/html",
                buildSimpleMessagePage("エラー", "SSID は 1〜" + String(WIFI_SSID_MAX_LENGTH) + " 文字で入力してください"));
    return;
  }

  if (password.length() < WIFI_PASSWORD_MIN_LENGTH || password.length() > WIFI_PASSWORD_MAX_LENGTH) {
    server.send(HTTP_STATUS_BAD_REQUEST, "text/html",
                buildSimpleMessagePage("エラー", "パスワードは " + String(WIFI_PASSWORD_MIN_LENGTH) + "〜" + String(WIFI_PASSWORD_MAX_LENGTH) + " 文字で入力してください"));
    return;
  }

  // 設定保存
  if (!saveConfig(ssid.c_str(), password.c_str())) {
    server.send(HTTP_STATUS_INTERNAL_ERROR, "text/html",
                buildSimpleMessagePage("エラー", "設定の暗号化保存に失敗しました。"));
    return;
  }

  // 成功ページ
  server.send(HTTP_STATUS_OK, "text/html",
              buildRestartNoticePage("✓ 設定を保存しました", "再起動後、設定した Wi-Fi に接続します。"));
  restartDeviceWithLog("設定保存完了");
}

/**
 * AP パスワード保存エンドポイント（POST /save_ap_password）
 */
void handleSaveAPPassword() {
  // フォームデータ取得
  String apPassword = server.arg("ap_password");

  // 入力検証
  if (apPassword.length() < WIFI_PASSWORD_MIN_LENGTH || apPassword.length() > WIFI_PASSWORD_MAX_LENGTH) {
    server.send(HTTP_STATUS_BAD_REQUEST, "text/html",
                buildSimpleMessagePage("エラー", "AP パスワードは " + String(WIFI_PASSWORD_MIN_LENGTH) + "〜" + String(WIFI_PASSWORD_MAX_LENGTH) + " 文字で入力してください"));
    return;
  }

  // AP パスワード保存
  if (!saveAPPassword(apPassword.c_str())) {
    server.send(HTTP_STATUS_INTERNAL_ERROR, "text/html",
                buildSimpleMessagePage("エラー", "AP パスワードの暗号化保存に失敗しました。"));
    return;
  }

  // 成功ページ
  server.send(HTTP_STATUS_OK, "text/html",
              buildRestartNoticePage("✓ AP パスワードを保存しました", "再起動後、新しいパスワードで AP に接続してください。"));
  restartDeviceWithLog("AP パスワード保存完了");
}

// ========================================
// Phase 8: DNS フィルタ関連ハンドラ
// ========================================

/**
 * DNS フィルタページ（GET /dns-filter）
 * PROGMEM テンプレートを使用してメモリを最適化
 */
void handleDNSFilter() {
  DNSStats stats = dnsFilter.getStats();
  int blocklistCount = dnsFilter.getBlocklistCount();
  bool enabled = dnsFilter.isEnabled();
  server.send(HTTP_STATUS_OK, "text/html", renderDNSFilterPage(stats, blocklistCount, enabled));
}

/**
 * DNS フィルタ ON/OFF 切り替え（POST /dns-filter-toggle）
 */
void handleDNSFilterToggle() {
  bool enabled = server.hasArg("enabled");

  // 設定を保存
  saveDNSFilterEnabled(enabled);
  dnsFilter.setEnabled(enabled);

  Serial.printf("DNS フィルタ設定変更: %s\n", enabled ? "有効" : "無効");

  // リダイレクト
  server.sendHeader("Location", "/dns-filter");
  server.send(HTTP_STATUS_SEE_OTHER);
}

/**
 * ブロックリストアップロード処理（POST /upload-blocklist）
 */
void handleUploadBlocklist() {
  static File uploadFile;
  HTTPUpload& upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    gBlocklistUploadState.failed = false;
    gBlocklistUploadState.message = "";
    uploadFile = File();
    Serial.printf("アップロード開始: %s\n", upload.filename.c_str());
    LittleFS.remove(BLOCKLIST_UPLOAD_PATH);
    uploadFile = LittleFS.open(BLOCKLIST_UPLOAD_PATH, "w");
    if (!uploadFile) {
      Serial.println("一時ファイルのオープンに失敗");
      markBlocklistUploadFailure("一時ファイルを開けませんでした。空き容量とファイルシステムの状態を確認してください。");
    }
  }
  else if (upload.status == UPLOAD_FILE_WRITE) {
    if (uploadFile) {
      size_t written = uploadFile.write(upload.buf, upload.currentSize);
      if (written != upload.currentSize) {
        Serial.println("一時ファイルへの書き込みに失敗");
        uploadFile.close();
        uploadFile = File();
        LittleFS.remove(BLOCKLIST_UPLOAD_PATH);
        markBlocklistUploadFailure("ブロックリストの書き込みに失敗しました。");
      }
    }
  }
  else if (upload.status == UPLOAD_FILE_END) {
    if (!uploadFile) {
      if (!gBlocklistUploadState.failed) {
        markBlocklistUploadFailure("アップロードファイルを処理できませんでした。");
      }
      return;
    }

    uploadFile.close();
    uploadFile = File();
    Serial.printf("アップロード完了: %d バイト\n", upload.totalSize);

    if (gBlocklistUploadState.failed) {
      LittleFS.remove(BLOCKLIST_UPLOAD_PATH);
      return;
    }

    // バックアップして置換
    if (!replaceBlocklistFile()) {
      Serial.println("ブロックリストの置換に失敗しました");
      markBlocklistUploadFailure("ブロックリストの置換に失敗しました。");
      return;
    }

    // リロード
    if (!dnsFilter.reloadBlocklist()) {
      Serial.println("ブロックリストの再読み込みに失敗しました");
      markBlocklistUploadFailure("ブロックリストの再読み込みに失敗しました。");
      return;
    }

    Serial.println("ブロックリストを更新しました");
  }
  else if (upload.status == UPLOAD_FILE_ABORTED) {
    if (uploadFile) {
      uploadFile.close();
      uploadFile = File();
    }
    LittleFS.remove(BLOCKLIST_UPLOAD_PATH);
    Serial.println("アップロードが中断されました");
    markBlocklistUploadFailure("アップロードが中断されました。");
  }
}

/**
 * ブロックリストダウンロード（GET /download-blocklist）
 */
void handleDownloadBlocklist() {
  if (!LittleFS.exists(BLOCKLIST_PATH)) {
    server.send(HTTP_STATUS_NOT_FOUND, "text/plain", "ブロックリストが見つかりません");
    return;
  }

  File file = LittleFS.open(BLOCKLIST_PATH, "r");
  if (file) {
    server.sendHeader("Content-Disposition", "attachment; filename=blocklist.txt");
    server.streamFile(file, "text/plain");
    file.close();
  } else {
    server.send(HTTP_STATUS_INTERNAL_ERROR, "text/plain", "ブロックリストを開けませんでした");
  }
}
