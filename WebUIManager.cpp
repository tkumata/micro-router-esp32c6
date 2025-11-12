/*
 * WebUIManager.cpp - Web UI 管理の実装
 *
 * HTTP サーバーのエンドポイント処理と HTML 生成を実装します。
 */

#include "WebUIManager.h"
#include "Config.h"
#include "ConfigManager.h"
#include "DNSFilterManager.h"  // Phase 8
#include <WiFi.h>
#include <Arduino.h>
#include <ESP.h>
#include <LittleFS.h>  // Phase 8
#include <Preferences.h>

// グローバル変数の extern 宣言
extern DNSFilterManager dnsFilter;  // Phase 8
extern Preferences preferences;

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
      server.send(200, "text/html",
        "<!DOCTYPE html><html><head><meta charset='UTF-8'></head><body>"
        "<h2>アップロード成功</h2>"
        "<p>ブロックリストが更新されました。</p>"
        "<a href='/dns-filter'>戻る</a>"
        "</body></html>");
    },
    handleUploadBlocklist
  );

  server.begin();

  Serial.println();
  Serial.println("Web サーバー起動: http://192.168.4.1");
}

/**
 * Web UI のルートページ（ステータス表示 + 設定フォーム）
 */
void handleRoot() {
  // ステータス情報取得
  bool staConnected = WiFi.status() == WL_CONNECTED;
  String staIP = staConnected ? WiFi.localIP().toString() : "未接続";
  int apClients = WiFi.softAPgetStationNum();
  uint32_t freeHeap = ESP.getFreeHeap();

  // HTML 生成
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
  html += "<title>XIAO ESP32C6 設定</title>";
  html += "<style>";
  html += "body{font-family:Arial,sans-serif;max-width:600px;margin:50px auto;padding:20px;background:#f5f5f5;}";
  html += "h1{color:#333;border-bottom:3px solid #007bff;padding-bottom:10px;}";
  html += "h2{color:#555;margin-top:30px;}";
  html += ".status{background:#fff;padding:20px;border-radius:8px;box-shadow:0 2px 4px rgba(0,0,0,0.1);margin:20px 0;}";
  html += ".status p{margin:10px 0;font-size:14px;}";
  html += ".status strong{color:#007bff;}";
  html += ".form-group{margin:15px 0;}";
  html += "label{display:block;margin-bottom:5px;font-weight:bold;color:#555;}";
  html += "input[type=text],input[type=password]{width:100%;padding:10px;box-sizing:border-box;border:1px solid #ddd;border-radius:4px;font-size:14px;}";
  html += "input[type=text]:focus,input[type=password]:focus{outline:none;border-color:#007bff;}";
  html += "button{background:#007bff;color:white;padding:12px 30px;border:none;border-radius:5px;cursor:pointer;font-size:16px;width:100%;margin-top:10px;}";
  html += "button:hover{background:#0056b3;}";
  html += ".connected{color:#28a745;}";
  html += ".disconnected{color:#dc3545;}";
  html += ".section{background:#fff;padding:20px;border-radius:8px;box-shadow:0 2px 4px rgba(0,0,0,0.1);margin:20px 0;}";
  html += "</style>";
  html += "</head><body>";

  html += "<h1>XIAO ESP32C6 マイクロルーター</h1>";

  // ナビゲーション（Phase 8）
  html += "<div style='margin:20px 0;'>";
  html += "<a href='/dns-filter' style='color:#007bff;text-decoration:none;margin-right:15px;'>DNS フィルタリング設定</a>";
  html += "<a href='/' style='color:#007bff;text-decoration:none;'>トップページ</a>";
  html += "</div>";

  // ステータス表示
  html += "<div class='status'>";
  html += "<h2>ステータス</h2>";
  html += "<p>STA 接続: <strong class='" + String(staConnected ? "connected" : "disconnected") + "'>";
  html += staConnected ? "接続済み" : "未接続";
  html += "</strong></p>";
  html += "<p>STA IP: <strong>" + staIP + "</strong></p>";
  html += "<p>AP クライアント数: <strong>" + String(apClients) + " / " + String(AP_MAX_CONNECTIONS) + "</strong></p>";
  html += "<p>AP パスワード: <strong>" + String(config.ap_password_set ? "カスタム設定済み" : "デフォルト") + "</strong></p>";
  html += "<p>空きメモリ: <strong>" + String(freeHeap / 1024) + " KB</strong></p>";
  html += "</div>";

  // STA 設定フォーム
  html += "<div class='section'>";
  html += "<h2>Wi-Fi (STA) 設定</h2>";
  html += "<p>接続したい既存の Wi-Fi ネットワークの情報を入力してください。</p>";
  html += "<form method='POST' action='/save'>";
  html += "<div class='form-group'>";
  html += "<label>既存 Wi-Fi の SSID:</label>";
  html += "<input type='text' name='ssid' placeholder='例: MyHomeWiFi' required maxlength='32'>";
  html += "</div>";
  html += "<div class='form-group'>";
  html += "<label>パスワード:</label>";
  html += "<input type='password' name='password' placeholder='8文字以上' required minlength='8' maxlength='64'>";
  html += "</div>";
  html += "<button type='submit'>保存して再起動</button>";
  html += "</form>";
  html += "</div>";

  // AP パスワード設定フォーム
  html += "<div class='section'>";
  html += "<h2>AP パスワード変更</h2>";
  html += "<p>このルーターの Wi-Fi アクセスポイント (micro-router-esp32c6) のパスワードを変更できます。</p>";
  html += "<form method='POST' action='/save_ap_password'>";
  html += "<div class='form-group'>";
  html += "<label>新しい AP パスワード:</label>";
  html += "<input type='password' name='ap_password' placeholder='8文字以上' required minlength='8' maxlength='64'>";
  html += "</div>";
  html += "<button type='submit'>AP パスワードを保存して再起動</button>";
  html += "</form>";
  html += "</div>";

  html += "</body></html>";

  server.send(200, "text/html", html);
}

/**
 * 設定保存エンドポイント（POST /save）
 */
void handleSave() {
  // フォームデータ取得
  String ssid = server.arg("ssid");
  String password = server.arg("password");

  // 入力検証
  if (ssid.length() == 0 || ssid.length() > 32) {
    server.send(400, "text/html",
                "<html><body><h1>エラー</h1><p>SSID は 1〜32 文字で入力してください</p>"
                "<a href='/'>戻る</a></body></html>");
    return;
  }

  if (password.length() < 8 || password.length() > 64) {
    server.send(400, "text/html",
                "<html><body><h1>エラー</h1><p>パスワードは 8〜64 文字で入力してください</p>"
                "<a href='/'>戻る</a></body></html>");
    return;
  }

  // 設定保存
  saveConfig(ssid.c_str(), password.c_str());

  // 成功ページ
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta http-equiv='refresh' content='" + String(CONFIG_SAVE_DELAY / 1000) + ";url=/'>";
  html += "<style>";
  html += "body{font-family:Arial,sans-serif;max-width:600px;margin:100px auto;padding:20px;text-align:center;}";
  html += "h1{color:#28a745;}";
  html += "p{font-size:18px;color:#555;}";
  html += "</style>";
  html += "</head><body>";
  html += "<h1>✓ 設定を保存しました</h1>";
  html += "<p>" + String(CONFIG_SAVE_DELAY / 1000) + "秒後に再起動します...</p>";
  html += "<p>再起動後、設定した Wi-Fi に接続します。</p>";
  html += "</body></html>";

  server.send(200, "text/html", html);

  Serial.println();
  Serial.println("========================================");
  Serial.println("設定保存完了");
  Serial.println("========================================");
  Serial.print(CONFIG_SAVE_DELAY / 1000);
  Serial.println("秒後に再起動します");
  Serial.println("========================================");

  // 再起動
  delay(CONFIG_SAVE_DELAY);
  ESP.restart();
}

/**
 * AP パスワード保存エンドポイント（POST /save_ap_password）
 */
void handleSaveAPPassword() {
  // フォームデータ取得
  String apPassword = server.arg("ap_password");

  // 入力検証
  if (apPassword.length() < 8 || apPassword.length() > 64) {
    server.send(400, "text/html",
                "<html><body><h1>エラー</h1><p>AP パスワードは 8〜64 文字で入力してください</p>"
                "<a href='/'>戻る</a></body></html>");
    return;
  }

  // AP パスワード保存
  saveAPPassword(apPassword.c_str());

  // 成功ページ
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta http-equiv='refresh' content='" + String(CONFIG_SAVE_DELAY / 1000) + ";url=/'>";
  html += "<style>";
  html += "body{font-family:Arial,sans-serif;max-width:600px;margin:100px auto;padding:20px;text-align:center;}";
  html += "h1{color:#28a745;}";
  html += "p{font-size:18px;color:#555;}";
  html += "</style>";
  html += "</head><body>";
  html += "<h1>✓ AP パスワードを保存しました</h1>";
  html += "<p>" + String(CONFIG_SAVE_DELAY / 1000) + "秒後に再起動します...</p>";
  html += "<p>再起動後、新しいパスワードで AP に接続してください。</p>";
  html += "</body></html>";

  server.send(200, "text/html", html);

  Serial.println();
  Serial.println("========================================");
  Serial.println("AP パスワード保存完了");
  Serial.println("========================================");
  Serial.print(CONFIG_SAVE_DELAY / 1000);
  Serial.println("秒後に再起動します");
  Serial.println("========================================");

  // 再起動
  delay(CONFIG_SAVE_DELAY);
  ESP.restart();
}

// ========================================
// Phase 8: DNS フィルタ関連ハンドラ
// ========================================

/**
 * DNS フィルタページ（GET /dns-filter）
 */
void handleDNSFilter() {
  DNSStats stats = dnsFilter.getStats();
  int blocklistCount = dnsFilter.getBlocklistCount();
  bool enabled = dnsFilter.isEnabled();

  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
  html += "<title>DNS フィルタリング</title>";
  html += "<style>";
  html += "body{font-family:Arial,sans-serif;max-width:800px;margin:50px auto;padding:20px;background:#f5f5f5;}";
  html += "h1{color:#333;border-bottom:3px solid #007bff;padding-bottom:10px;}";
  html += "h2{color:#555;margin-top:30px;}";
  html += ".status{background:#fff;padding:20px;border-radius:8px;box-shadow:0 2px 4px rgba(0,0,0,0.1);margin:20px 0;}";
  html += ".form-group{margin:15px 0;}";
  html += "label{display:block;margin-bottom:5px;font-weight:bold;}";
  html += "button{background:#007bff;color:white;padding:10px 20px;border:none;border-radius:5px;cursor:pointer;}";
  html += "button:hover{background:#0056b3;}";
  html += "</style>";
  html += "</head><body>";

  html += "<h1>DNS フィルタリング</h1>";
  html += "<p><a href='/' style='color:#007bff;'>← トップページに戻る</a></p>";

  // 有効/無効切り替え
  html += "<div class='status'>";
  html += "<h2>DNS フィルタ設定</h2>";
  html += "<form method='POST' action='/dns-filter-toggle'>";
  html += "<label>";
  html += "<input type='checkbox' name='enabled' " + String(enabled ? "checked" : "") + "> ";
  html += "DNS フィルタを有効にする";
  html += "</label><br><br>";
  html += "<button type='submit'>保存</button>";
  html += "</form>";
  html += "</div>";

  // 統計情報
  html += "<div class='status'>";
  html += "<h2>統計情報</h2>";
  html += "<ul>";
  html += "<li>総クエリ数: <strong>" + String(stats.totalQueries) + "</strong></li>";
  html += "<li>ブロック数: <strong>" + String(stats.blockedQueries) + "</strong>";
  if (stats.totalQueries > 0) {
    html += " (" + String(stats.blockedQueries * 100 / stats.totalQueries) + "%)";
  }
  html += "</li>";
  html += "<li>許可数: <strong>" + String(stats.allowedQueries) + "</strong>";
  if (stats.totalQueries > 0) {
    html += " (" + String(stats.allowedQueries * 100 / stats.totalQueries) + "%)";
  }
  html += "</li>";
  html += "<li>ブロックリスト登録数: <strong>" + String(blocklistCount) + " ドメイン</strong></li>";
  html += "</ul>";
  html += "</div>";

  // ブロックリストアップロード
  html += "<div class='status'>";
  html += "<h2>ブロックリスト管理</h2>";
  html += "<form method='POST' action='/upload-blocklist' enctype='multipart/form-data'>";
  html += "<label>domain.txt をアップロード:</label><br>";
  html += "<input type='file' name='blocklist' accept='.txt' required><br><br>";
  html += "<button type='submit'>アップロード</button>";
  html += "</form>";
  html += "<p style='margin-top:15px;'>";
  html += "<a href='/download-blocklist' style='color:#007bff;'>現在のリストをダウンロード</a>";
  html += "</p>";
  html += "</div>";

  html += "</body></html>";

  server.send(200, "text/html", html);
}

/**
 * DNS フィルタ ON/OFF 切り替え（POST /dns-filter-toggle）
 */
void handleDNSFilterToggle() {
  bool enabled = server.hasArg("enabled");

  // 設定を保存
  preferences.begin(PREF_NAMESPACE, false);
  preferences.putBool(PREF_KEY_DNS_FILTER_ENABLED, enabled);
  preferences.end();

  dnsFilter.setEnabled(enabled);

  Serial.printf("DNS フィルタ設定変更: %s\n", enabled ? "有効" : "無効");

  // リダイレクト
  server.sendHeader("Location", "/dns-filter");
  server.send(303);
}

/**
 * ブロックリストアップロード処理（POST /upload-blocklist）
 */
void handleUploadBlocklist() {
  static File uploadFile;
  HTTPUpload& upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    Serial.printf("アップロード開始: %s\n", upload.filename.c_str());
    uploadFile = LittleFS.open("/blocklist.txt.tmp", "w");
    if (!uploadFile) {
      Serial.println("一時ファイルのオープンに失敗");
    }
  }
  else if (upload.status == UPLOAD_FILE_WRITE) {
    if (uploadFile) {
      uploadFile.write(upload.buf, upload.currentSize);
    }
  }
  else if (upload.status == UPLOAD_FILE_END) {
    if (uploadFile) {
      uploadFile.close();
      Serial.printf("アップロード完了: %d バイト\n", upload.totalSize);

      // バックアップして置換
      LittleFS.remove("/blocklist.txt.bak");
      if (LittleFS.exists("/blocklist.txt")) {
        LittleFS.rename("/blocklist.txt", "/blocklist.txt.bak");
      }
      LittleFS.rename("/blocklist.txt.tmp", "/blocklist.txt");

      // リロード
      dnsFilter.reloadBlocklist();
      Serial.println("ブロックリストを更新しました");
    }
  }
}

/**
 * ブロックリストダウンロード（GET /download-blocklist）
 */
void handleDownloadBlocklist() {
  if (!LittleFS.exists("/blocklist.txt")) {
    server.send(404, "text/plain", "ブロックリストが見つかりません");
    return;
  }

  File file = LittleFS.open("/blocklist.txt", "r");
  if (file) {
    server.sendHeader("Content-Disposition", "attachment; filename=blocklist.txt");
    server.streamFile(file, "text/plain");
    file.close();
  } else {
    server.send(500, "text/plain", "ブロックリストを開けませんでした");
  }
}
