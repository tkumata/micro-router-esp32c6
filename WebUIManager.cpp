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
#include <inttypes.h>
#include <esp_system.h>
#include <LittleFS.h>  // Phase 8

namespace {
const char* BLOCKLIST_PATH = "/blocklist.txt";
const char* BLOCKLIST_UPLOAD_PATH = "/blocklist.txt.tmp";
const char* BLOCKLIST_BACKUP_PATH = "/blocklist.txt.bak";
const char* CSRF_TOKEN_ARG = "csrf_token";
const char* SESSION_COOKIE_NAME = "mr_session";
const char* WEB_UI_HEADER_NAMES[] = {"Origin", "Referer", "Cookie"};
const unsigned long SESSION_TIMEOUT = 15UL * 60UL * 1000UL;

struct BlocklistUploadState {
  bool failed;
  String message;
};

BlocklistUploadState gBlocklistUploadState = {false, ""};
String gCsrfToken;
String gSessionToken;
unsigned long gSessionExpiresAt = 0;

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

String generateCsrfToken() {
  char token[33];
  for (size_t index = 0; index < 4; ++index) {
    uint32_t value = esp_random();
    snprintf(&token[index * 8], 9, "%08" PRIx32, value);
  }
  token[32] = '\0';
  return String(token);
}

String generateSessionToken() {
  return generateCsrfToken();
}

bool isDefaultAPPasswordActive() {
  return strcmp(config.ap_password, DEFAULT_AP_PASSWORD) == 0;
}

String extractCookieValue(const String& cookieHeader, const String& cookieName) {
  String pattern = cookieName + "=";
  int start = cookieHeader.indexOf(pattern);
  if (start < 0) {
    return "";
  }

  start += pattern.length();
  int end = cookieHeader.indexOf(';', start);
  if (end < 0) {
    end = cookieHeader.length();
  }

  String value = cookieHeader.substring(start, end);
  value.trim();
  return value;
}

bool isSafeRedirectPath(const String& path) {
  return path.length() > 0 && path.startsWith("/") && !path.startsWith("//");
}

void clearSession() {
  gSessionToken = "";
  gSessionExpiresAt = 0;
}

void issueSession() {
  gSessionToken = generateSessionToken();
  gSessionExpiresAt = millis() + SESSION_TIMEOUT;
  server.sendHeader("Set-Cookie",
                    String(SESSION_COOKIE_NAME) + "=" + gSessionToken +
                    "; Path=/; HttpOnly; SameSite=Strict");
}

void expireSessionCookie() {
  server.sendHeader("Set-Cookie",
                    String(SESSION_COOKIE_NAME) + "=; Path=/; Max-Age=0; HttpOnly; SameSite=Strict");
}

bool hasValidSession() {
  if (gSessionToken.length() == 0) {
    return false;
  }

  if (static_cast<long>(millis() - gSessionExpiresAt) >= 0) {
    clearSession();
    return false;
  }

  String cookieHeader = server.header("Cookie");
  if (cookieHeader.length() == 0) {
    return false;
  }

  String sessionValue = extractCookieValue(cookieHeader, SESSION_COOKIE_NAME);
  if (sessionValue != gSessionToken) {
    return false;
  }

  gSessionExpiresAt = millis() + SESSION_TIMEOUT;
  return true;
}

bool isTrustedRequestSource() {
  const String origin = server.header("Origin");
  const String referer = server.header("Referer");
  const String trustedBaseUrl = String("http://") + AP_IP.toString();

  if (origin.length() > 0) {
    return origin.equals(trustedBaseUrl);
  }

  if (referer.length() > 0) {
    return referer.startsWith(trustedBaseUrl + "/") || referer.equals(trustedBaseUrl);
  }

  return true;
}

bool verifyCsrfRequest(const String& backPath = "/") {
  if (!isTrustedRequestSource()) {
    server.send(HTTP_STATUS_FORBIDDEN, "text/html",
                buildSimpleMessagePage("エラー", "不正な送信元からのリクエストを拒否しました。", backPath));
    return false;
  }

  if (!server.hasArg(CSRF_TOKEN_ARG) || server.arg(CSRF_TOKEN_ARG) != gCsrfToken) {
    server.send(HTTP_STATUS_FORBIDDEN, "text/html",
                buildSimpleMessagePage("エラー", "CSRF トークンの検証に失敗しました。ページを再読み込みして再度お試しください。", backPath));
    return false;
  }

  return true;
}

bool ensureSession(const String& redirectPath = "/") {
  if (isDefaultAPPasswordActive()) {
    server.sendHeader("Location", "/", true);
    server.send(HTTP_STATUS_SEE_OTHER);
    return false;
  }

  if (hasValidSession()) {
    return true;
  }

  server.send(HTTP_STATUS_FORBIDDEN, "text/html",
              renderLoginPage(gCsrfToken, redirectPath, "Web UI を利用するにはログインが必要です。"));
  return false;
}

bool isRequestFromAPSubnet() {
  IPAddress remoteIP = server.client().remoteIP();
  for (uint8_t index = 0; index < 4; ++index) {
    if ((remoteIP[index] & AP_SUBNET[index]) != (AP_IP[index] & AP_SUBNET[index])) {
      return false;
    }
  }

  return true;
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
  gCsrfToken = generateCsrfToken();
  server.collectHeaders(WEB_UI_HEADER_NAMES, sizeof(WEB_UI_HEADER_NAMES) / sizeof(WEB_UI_HEADER_NAMES[0]));
  server.on("/", handleRoot);
  server.on("/login", HTTP_GET, []() {
    if (isDefaultAPPasswordActive()) {
      server.send(HTTP_STATUS_OK, "text/html", renderForcePasswordChangePage(gCsrfToken));
      return;
    }

    if (hasValidSession()) {
      server.sendHeader("Location", "/", true);
      server.send(HTTP_STATUS_SEE_OTHER);
      return;
    }

    String redirectPath = server.hasArg("redirect_to") && isSafeRedirectPath(server.arg("redirect_to"))
      ? server.arg("redirect_to")
      : "/";
    server.send(HTTP_STATUS_OK, "text/html", renderLoginPage(gCsrfToken, redirectPath));
  });
  server.on("/login", HTTP_POST, []() {
    if (isDefaultAPPasswordActive()) {
      server.send(HTTP_STATUS_OK, "text/html", renderForcePasswordChangePage(gCsrfToken));
      return;
    }

    if (!verifyCsrfRequest("/login")) {
      return;
    }

    String redirectPath = server.hasArg("redirect_to") && isSafeRedirectPath(server.arg("redirect_to"))
      ? server.arg("redirect_to")
      : "/";
    String apPassword = server.arg("ap_password");

    if (apPassword != String(config.ap_password)) {
      server.send(HTTP_STATUS_FORBIDDEN, "text/html",
                  renderLoginPage(gCsrfToken, redirectPath, "AP パスワードが一致しません。"));
      return;
    }

    issueSession();
    server.sendHeader("Location", redirectPath, true);
    server.send(HTTP_STATUS_SEE_OTHER);
  });
  server.on("/logout", HTTP_POST, []() {
    if (!verifyCsrfRequest("/")) {
      return;
    }

    clearSession();
    expireSessionCookie();
    server.sendHeader("Location", "/login", true);
    server.send(HTTP_STATUS_SEE_OTHER);
  });
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
  if (isDefaultAPPasswordActive()) {
    server.send(HTTP_STATUS_OK, "text/html", renderForcePasswordChangePage(gCsrfToken));
    return;
  }

  if (!ensureSession("/")) {
    return;
  }

  server.send(HTTP_STATUS_OK, "text/html", renderRootPage(gCsrfToken));
}

/**
 * 設定保存エンドポイント（POST /save）
 */
void handleSave() {
  if (!ensureSession("/")) {
    return;
  }

  if (!verifyCsrfRequest("/")) {
    return;
  }

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
  if (!isDefaultAPPasswordActive() && !ensureSession("/")) {
    return;
  }

  if (!verifyCsrfRequest("/")) {
    return;
  }

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
  clearSession();
  expireSessionCookie();
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
  if (!ensureSession("/dns-filter")) {
    return;
  }

  DNSStats stats = dnsFilter.getStats();
  int blocklistCount = dnsFilter.getBlocklistCount();
  bool enabled = dnsFilter.isEnabled();
  server.send(HTTP_STATUS_OK, "text/html", renderDNSFilterPage(stats, blocklistCount, enabled, gCsrfToken));
}

/**
 * DNS フィルタ ON/OFF 切り替え（POST /dns-filter-toggle）
 */
void handleDNSFilterToggle() {
  if (!ensureSession("/dns-filter")) {
    return;
  }

  if (!verifyCsrfRequest("/dns-filter")) {
    return;
  }

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

    if (!isRequestFromAPSubnet()) {
      markBlocklistUploadFailure("ブロックリスト更新は AP に接続した端末からのみ実行できます。");
      return;
    }

    if (!ensureSession("/dns-filter")) {
      markBlocklistUploadFailure("ログイン状態を確認できませんでした。再ログインして再度お試しください。");
      return;
    }

    if (!verifyCsrfRequest("/dns-filter")) {
      markBlocklistUploadFailure("CSRF トークンの検証に失敗しました。ページを再読み込みして再度お試しください。");
      return;
    }
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
      if (upload.totalSize + upload.currentSize > BLOCKLIST_UPLOAD_MAX_SIZE) {
        Serial.println("ブロックリストサイズ上限を超過");
        uploadFile.close();
        uploadFile = File();
        LittleFS.remove(BLOCKLIST_UPLOAD_PATH);
        markBlocklistUploadFailure("ブロックリストのサイズが上限を超えています。");
        return;
      }

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

    if (upload.totalSize > BLOCKLIST_UPLOAD_MAX_SIZE) {
      uploadFile.close();
      uploadFile = File();
      LittleFS.remove(BLOCKLIST_UPLOAD_PATH);
      markBlocklistUploadFailure("ブロックリストのサイズが上限を超えています。");
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
  if (!ensureSession("/dns-filter")) {
    return;
  }

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
