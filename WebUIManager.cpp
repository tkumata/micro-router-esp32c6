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

// ===== PROGMEM による HTML テンプレート（メモリ最適化） =====

// 共通 CSS（フラッシュメモリに格納）
const char HTML_COMMON_CSS[] PROGMEM = R"rawliteral(
body{font-family:Arial,sans-serif;max-width:600px;margin:50px auto;padding:20px;background:#f5f5f5;}
h1{color:#333;border-bottom:3px solid #007bff;padding-bottom:10px;}
h2{color:#555;margin-top:30px;}
.status{background:#fff;padding:20px;border-radius:8px;box-shadow:0 2px 4px rgba(0,0,0,0.1);margin:20px 0;}
.status p{margin:10px 0;font-size:14px;}
.status strong{color:#007bff;}
.form-group{margin:15px 0;}
label{display:block;margin-bottom:5px;font-weight:bold;color:#555;}
input[type=text],input[type=password]{width:100%;padding:10px;box-sizing:border-box;border:1px solid #ddd;border-radius:4px;font-size:14px;}
input[type=text]:focus,input[type=password]:focus{outline:none;border-color:#007bff;}
button{background:#007bff;color:white;padding:12px 30px;border:none;border-radius:5px;cursor:pointer;font-size:16px;width:100%;margin-top:10px;}
button:hover{background:#0056b3;}
.connected{color:#28a745;}
.disconnected{color:#dc3545;}
.section{background:#fff;padding:20px;border-radius:8px;box-shadow:0 2px 4px rgba(0,0,0,0.1);margin:20px 0;}
)rawliteral";

// メインページテンプレート
const char HTML_ROOT_TEMPLATE[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head>
<meta charset='UTF-8'>
<meta name='viewport' content='width=device-width,initial-scale=1'>
<title>XIAO ESP32C6 設定</title>
<style>%CSS%</style>
</head><body>
<h1>XIAO ESP32C6 マイクロルーター</h1>
<div style='margin:20px 0;'>
<a href='/dns-filter' style='color:#007bff;text-decoration:none;margin-right:15px;'>DNS フィルタリング設定</a>
<a href='/' style='color:#007bff;text-decoration:none;'>トップページ</a>
</div>
<div class='status'>
<h2>ステータス</h2>
<p>STA 接続: <strong class='%STA_CLASS%'>%STA_STATUS%</strong></p>
<p>STA IP: <strong>%STA_IP%</strong></p>
<p>AP クライアント数: <strong>%AP_CLIENTS% / %AP_MAX%</strong></p>
<p>AP パスワード: <strong>%AP_PASSWORD_STATUS%</strong></p>
<p>空きメモリ: <strong>%FREE_HEAP% KB</strong></p>
</div>
<div class='section'>
<h2>Wi-Fi (STA) 設定</h2>
<p>接続したい既存の Wi-Fi ネットワークの情報を入力してください。</p>
<form method='POST' action='/save'>
<div class='form-group'>
<label>既存 Wi-Fi の SSID:</label>
<input type='text' name='ssid' placeholder='例: MyHomeWiFi' required maxlength='%SSID_MAX%'>
</div>
<div class='form-group'>
<label>パスワード:</label>
<input type='password' name='password' placeholder='8文字以上' required minlength='%PASSWORD_MIN%' maxlength='%PASSWORD_MAX%'>
</div>
<button type='submit'>保存して再起動</button>
</form>
</div>
<div class='section'>
<h2>AP パスワード変更</h2>
<p>このルーターの Wi-Fi アクセスポイント (micro-router-esp32c6) のパスワードを変更できます。</p>
<form method='POST' action='/save_ap_password'>
<div class='form-group'>
<label>新しい AP パスワード:</label>
<input type='password' name='ap_password' placeholder='8文字以上' required minlength='%PASSWORD_MIN%' maxlength='%PASSWORD_MAX%'>
</div>
<button type='submit'>AP パスワードを保存して再起動</button>
</form>
</div>
</body></html>
)rawliteral";

// DNS フィルタページテンプレート
const char HTML_DNS_FILTER_TEMPLATE[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head>
<meta charset='UTF-8'>
<meta name='viewport' content='width=device-width,initial-scale=1'>
<title>DNS フィルタリング</title>
<style>
body{font-family:Arial,sans-serif;max-width:800px;margin:50px auto;padding:20px;background:#f5f5f5;}
h1{color:#333;border-bottom:3px solid #007bff;padding-bottom:10px;}
h2{color:#555;margin-top:30px;}
.status{background:#fff;padding:20px;border-radius:8px;box-shadow:0 2px 4px rgba(0,0,0,0.1);margin:20px 0;}
.form-group{margin:15px 0;}
label{display:block;margin-bottom:5px;font-weight:bold;}
button{background:#007bff;color:white;padding:10px 20px;border:none;border-radius:5px;cursor:pointer;}
button:hover{background:#0056b3;}
</style>
</head><body>
<h1>DNS フィルタリング</h1>
<p><a href='/' style='color:#007bff;'>← トップページに戻る</a></p>
<div class='status'>
<h2>DNS フィルタ設定</h2>
<form method='POST' action='/dns-filter-toggle'>
<label>
<input type='checkbox' name='enabled' %ENABLED_CHECKED%>
DNS フィルタを有効にする
</label><br><br>
<button type='submit'>保存</button>
</form>
</div>
<div class='status'>
<h2>統計情報</h2>
<ul>
<li>総クエリ数: <strong>%TOTAL_QUERIES%</strong></li>
<li>ブロック数: <strong>%BLOCKED_QUERIES%</strong>%BLOCKED_PERCENT%</li>
<li>許可数: <strong>%ALLOWED_QUERIES%</strong>%ALLOWED_PERCENT%</li>
<li>ブロックリスト登録数: <strong>%BLOCKLIST_COUNT% ドメイン</strong></li>
</ul>
</div>
<div class='status'>
<h2>ブロックリスト管理</h2>
<form method='POST' action='/upload-blocklist' enctype='multipart/form-data'>
<label>domain.txt をアップロード:</label><br>
<input type='file' name='blocklist' accept='.txt' required><br><br>
<button type='submit'>アップロード</button>
</form>
<p style='margin-top:15px;'>
<a href='/download-blocklist' style='color:#007bff;'>現在のリストをダウンロード</a>
</p>
</div>
</body></html>
)rawliteral";

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
      server.send(HTTP_STATUS_OK, "text/html",
        "<!DOCTYPE html><html><head><meta charset='UTF-8'></head><body>"
        "<h2>アップロード成功</h2>"
        "<p>ブロックリストが更新されました。</p>"
        "<a href='/dns-filter'>戻る</a>"
        "</body></html>");
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
      server.send(404, "text/plain", "Not Found");
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
  // ステータス情報取得
  bool staConnected = WiFi.status() == WL_CONNECTED;
  String staIP = staConnected ? WiFi.localIP().toString() : "未接続";
  int apClients = WiFi.softAPgetStationNum();
  uint32_t freeHeap = ESP.getFreeHeap();

  // テンプレートを PROGMEM から読み込み
  String html = FPSTR(HTML_ROOT_TEMPLATE);

  // CSS を埋め込み
  html.replace("%CSS%", FPSTR(HTML_COMMON_CSS));

  // 動的な値を置換
  html.replace("%STA_CLASS%", staConnected ? "connected" : "disconnected");
  html.replace("%STA_STATUS%", staConnected ? "接続済み" : "未接続");
  html.replace("%STA_IP%", staIP);
  html.replace("%AP_CLIENTS%", String(apClients));
  html.replace("%AP_MAX%", String(AP_MAX_CONNECTIONS));
  html.replace("%AP_PASSWORD_STATUS%", config.ap_password_set ? "カスタム設定済み" : "デフォルト");
  html.replace("%FREE_HEAP%", String(freeHeap / BYTES_TO_KB_DIVISOR));
  html.replace("%SSID_MAX%", String(WIFI_SSID_MAX_LENGTH));
  html.replace("%PASSWORD_MIN%", String(WIFI_PASSWORD_MIN_LENGTH));
  html.replace("%PASSWORD_MAX%", String(WIFI_PASSWORD_MAX_LENGTH));

  server.send(HTTP_STATUS_OK, "text/html", html);
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
                "<html><body><h1>エラー</h1><p>SSID は 1〜" + String(WIFI_SSID_MAX_LENGTH) + " 文字で入力してください</p>"
                "<a href='/'>戻る</a></body></html>");
    return;
  }

  if (password.length() < WIFI_PASSWORD_MIN_LENGTH || password.length() > WIFI_PASSWORD_MAX_LENGTH) {
    server.send(HTTP_STATUS_BAD_REQUEST, "text/html",
                "<html><body><h1>エラー</h1><p>パスワードは " + String(WIFI_PASSWORD_MIN_LENGTH) + "〜" + String(WIFI_PASSWORD_MAX_LENGTH) + " 文字で入力してください</p>"
                "<a href='/'>戻る</a></body></html>");
    return;
  }

  // 設定保存
  saveConfig(ssid.c_str(), password.c_str());

  // 成功ページ
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta http-equiv='refresh' content='" + String(CONFIG_SAVE_DELAY / MILLISECONDS_TO_SECONDS_DIVISOR) + ";url=/'>";
  html += "<style>";
  html += "body{font-family:Arial,sans-serif;max-width:600px;margin:100px auto;padding:20px;text-align:center;}";
  html += "h1{color:#28a745;}";
  html += "p{font-size:18px;color:#555;}";
  html += "</style>";
  html += "</head><body>";
  html += "<h1>✓ 設定を保存しました</h1>";
  html += "<p>" + String(CONFIG_SAVE_DELAY / MILLISECONDS_TO_SECONDS_DIVISOR) + "秒後に再起動します...</p>";
  html += "<p>再起動後、設定した Wi-Fi に接続します。</p>";
  html += "</body></html>";

  server.send(HTTP_STATUS_OK, "text/html", html);

  Serial.println();
  Serial.println("========================================");
  Serial.println("設定保存完了");
  Serial.println("========================================");
  Serial.print(CONFIG_SAVE_DELAY / MILLISECONDS_TO_SECONDS_DIVISOR);
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
  if (apPassword.length() < WIFI_PASSWORD_MIN_LENGTH || apPassword.length() > WIFI_PASSWORD_MAX_LENGTH) {
    server.send(HTTP_STATUS_BAD_REQUEST, "text/html",
                "<html><body><h1>エラー</h1><p>AP パスワードは " + String(WIFI_PASSWORD_MIN_LENGTH) + "〜" + String(WIFI_PASSWORD_MAX_LENGTH) + " 文字で入力してください</p>"
                "<a href='/'>戻る</a></body></html>");
    return;
  }

  // AP パスワード保存
  saveAPPassword(apPassword.c_str());

  // 成功ページ
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta http-equiv='refresh' content='" + String(CONFIG_SAVE_DELAY / MILLISECONDS_TO_SECONDS_DIVISOR) + ";url=/'>";
  html += "<style>";
  html += "body{font-family:Arial,sans-serif;max-width:600px;margin:100px auto;padding:20px;text-align:center;}";
  html += "h1{color:#28a745;}";
  html += "p{font-size:18px;color:#555;}";
  html += "</style>";
  html += "</head><body>";
  html += "<h1>✓ AP パスワードを保存しました</h1>";
  html += "<p>" + String(CONFIG_SAVE_DELAY / MILLISECONDS_TO_SECONDS_DIVISOR) + "秒後に再起動します...</p>";
  html += "<p>再起動後、新しいパスワードで AP に接続してください。</p>";
  html += "</body></html>";

  server.send(HTTP_STATUS_OK, "text/html", html);

  Serial.println();
  Serial.println("========================================");
  Serial.println("AP パスワード保存完了");
  Serial.println("========================================");
  Serial.print(CONFIG_SAVE_DELAY / MILLISECONDS_TO_SECONDS_DIVISOR);
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
 * PROGMEM テンプレートを使用してメモリを最適化
 */
void handleDNSFilter() {
  DNSStats stats = dnsFilter.getStats();
  int blocklistCount = dnsFilter.getBlocklistCount();
  bool enabled = dnsFilter.isEnabled();

  // テンプレートを PROGMEM から読み込み
  String html = FPSTR(HTML_DNS_FILTER_TEMPLATE);

  // 動的な値を置換
  html.replace("%ENABLED_CHECKED%", enabled ? "checked" : "");
  html.replace("%TOTAL_QUERIES%", String(stats.totalQueries));
  html.replace("%BLOCKED_QUERIES%", String(stats.blockedQueries));
  html.replace("%ALLOWED_QUERIES%", String(stats.allowedQueries));
  html.replace("%BLOCKLIST_COUNT%", String(blocklistCount));

  // パーセント計算
  String blockedPercent = "";
  String allowedPercent = "";
  if (stats.totalQueries > 0) {
    blockedPercent = " (" + String(stats.blockedQueries * PERCENTAGE_MULTIPLIER / stats.totalQueries) + "%)";
    allowedPercent = " (" + String(stats.allowedQueries * PERCENTAGE_MULTIPLIER / stats.totalQueries) + "%)";
  }
  html.replace("%BLOCKED_PERCENT%", blockedPercent);
  html.replace("%ALLOWED_PERCENT%", allowedPercent);

  server.send(HTTP_STATUS_OK, "text/html", html);
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
  server.send(HTTP_STATUS_SEE_OTHER);
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
    server.send(HTTP_STATUS_NOT_FOUND, "text/plain", "ブロックリストが見つかりません");
    return;
  }

  File file = LittleFS.open("/blocklist.txt", "r");
  if (file) {
    server.sendHeader("Content-Disposition", "attachment; filename=blocklist.txt");
    server.streamFile(file, "text/plain");
    file.close();
  } else {
    server.send(HTTP_STATUS_INTERNAL_ERROR, "text/plain", "ブロックリストを開けませんでした");
  }
}
