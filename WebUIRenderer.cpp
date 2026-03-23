/*
 * WebUIRenderer.cpp - Web UI 描画補助の実装
 */

#include "WebUIRenderer.h"
#include "Config.h"
#include "ConfigManager.h"
#include <WiFi.h>
#include <ESP.h>

namespace {
String buildBasePage(const String& body, const String& extraHead = "") {
  String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
  html += extraHead;
  html += "</head><body>";
  html += body;
  html += "</body></html>";
  return html;
}

String calculatePercentText(uint32_t value, uint32_t total) {
  if (total == 0) {
    return "";
  }
  return " (" + String(static_cast<uint64_t>(value) * PERCENTAGE_MULTIPLIER / total) + "%)";
}
}

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
.notice{background:#fff3cd;color:#856404;border:1px solid #ffeeba;padding:12px 16px;border-radius:6px;margin:20px 0;}
.actions{display:flex;gap:10px;align-items:center;flex-wrap:wrap;}
.actions form{margin:0;}
.actions a{color:#007bff;text-decoration:none;}
)rawliteral";

const char HTML_LOGIN_TEMPLATE[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head>
<meta charset='UTF-8'>
<meta name='viewport' content='width=device-width,initial-scale=1'>
<title>Web UI ログイン</title>
<style>%CSS%</style>
</head><body>
<h1>XIAO ESP32C6 マイクロルーター</h1>
<div class='section'>
<h2>Web UI ログイン</h2>
%MESSAGE_BLOCK%
<p>現在の AP パスワードを入力してください。</p>
<form method='POST' action='/login'>
<input type='hidden' name='csrf_token' value='%CSRF_TOKEN%'>
<input type='hidden' name='redirect_to' value='%REDIRECT_PATH%'>
<div class='form-group'>
<label>AP パスワード:</label>
<input type='password' name='ap_password' placeholder='8文字以上' required minlength='%PASSWORD_MIN%' maxlength='%PASSWORD_MAX%'>
</div>
<button type='submit'>ログイン</button>
</form>
</div>
</body></html>
)rawliteral";

const char HTML_FORCE_PASSWORD_CHANGE_TEMPLATE[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head>
<meta charset='UTF-8'>
<meta name='viewport' content='width=device-width,initial-scale=1'>
<title>初期 AP パスワード変更</title>
<style>%CSS%</style>
</head><body>
<h1>XIAO ESP32C6 マイクロルーター</h1>
<div class='notice'>
初期 AP パスワードのままでは Web UI を利用できません。先に AP パスワードを変更してください。
</div>
<div class='section'>
<h2>AP パスワード変更</h2>
<p>新しい AP パスワードを設定すると再起動します。再接続後にログイン画面から Web UI を利用できます。</p>
<form method='POST' action='/save_ap_password'>
<input type='hidden' name='csrf_token' value='%CSRF_TOKEN%'>
<div class='form-group'>
<label>新しい AP パスワード:</label>
<input type='password' name='ap_password' placeholder='8文字以上' required minlength='%PASSWORD_MIN%' maxlength='%PASSWORD_MAX%'>
</div>
<button type='submit'>AP パスワードを保存して再起動</button>
</form>
</div>
</body></html>
)rawliteral";

const char HTML_ROOT_TEMPLATE[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head>
<meta charset='UTF-8'>
<meta name='viewport' content='width=device-width,initial-scale=1'>
<title>XIAO ESP32C6 設定</title>
<style>%CSS%</style>
</head><body>
<h1>XIAO ESP32C6 マイクロルーター</h1>
<div class='actions' style='margin:20px 0;'>
<a href='/dns-filter' style='color:#007bff;text-decoration:none;margin-right:15px;'>DNS フィルタリング設定</a>
<a href='/' style='color:#007bff;text-decoration:none;'>トップページ</a>
<form method='POST' action='/logout'>
<input type='hidden' name='csrf_token' value='%CSRF_TOKEN%'>
<button type='submit' style='width:auto;margin-top:0;padding:8px 14px;'>ログアウト</button>
</form>
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
<input type='hidden' name='csrf_token' value='%CSRF_TOKEN%'>
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
<input type='hidden' name='csrf_token' value='%CSRF_TOKEN%'>
<div class='form-group'>
<label>新しい AP パスワード:</label>
<input type='password' name='ap_password' placeholder='8文字以上' required minlength='%PASSWORD_MIN%' maxlength='%PASSWORD_MAX%'>
</div>
<button type='submit'>AP パスワードを保存して再起動</button>
</form>
</div>
</body></html>
)rawliteral";

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
<div class='actions'>
<a href='/' style='color:#007bff;'>← トップページに戻る</a>
<form method='POST' action='/logout'>
<input type='hidden' name='csrf_token' value='%CSRF_TOKEN%'>
<button type='submit' style='width:auto;margin-top:0;padding:8px 14px;'>ログアウト</button>
</form>
</div>
<div class='status'>
<h2>DNS フィルタ設定</h2>
<form method='POST' action='/dns-filter-toggle'>
<input type='hidden' name='csrf_token' value='%CSRF_TOKEN%'>
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
<form method='POST' action='/upload-blocklist?csrf_token=%CSRF_TOKEN%' enctype='multipart/form-data'>
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

String buildSimpleMessagePage(const String& title, const String& message, const String& backPath) {
  String body;
  body += "<h1>";
  body += title;
  body += "</h1><p>";
  body += message;
  body += "</p><p><a href='";
  body += backPath;
  body += "'>戻る</a></p>";
  return buildBasePage(body);
}

String buildRestartNoticePage(const String& title, const String& message) {
  String extraHead = "<meta http-equiv='refresh' content='";
  extraHead += String(CONFIG_SAVE_DELAY / MILLISECONDS_TO_SECONDS_DIVISOR);
  extraHead += ";url=/'>";

  String body = "<style>";
  body += "body{font-family:Arial,sans-serif;max-width:600px;margin:100px auto;padding:20px;text-align:center;}";
  body += "h1{color:#28a745;}p{font-size:18px;color:#555;}";
  body += "</style><h1>";
  body += title;
  body += "</h1><p>";
  body += String(CONFIG_SAVE_DELAY / MILLISECONDS_TO_SECONDS_DIVISOR);
  body += "秒後に再起動します...</p><p>";
  body += message;
  body += "</p>";

  return buildBasePage(body, extraHead);
}

String buildUploadSuccessPage() {
  return buildBasePage("<h2>アップロード成功</h2><p>ブロックリストが更新されました。</p><a href='/dns-filter'>戻る</a>");
}

String renderLoginPage(const String& csrfToken, const String& redirectPath, const String& message) {
  String html = FPSTR(HTML_LOGIN_TEMPLATE);
  String messageBlock = "";
  if (message.length() > 0) {
    messageBlock = "<div class='notice'>" + message + "</div>";
  }

  html.replace("%CSS%", FPSTR(HTML_COMMON_CSS));
  html.replace("%MESSAGE_BLOCK%", messageBlock);
  html.replace("%CSRF_TOKEN%", csrfToken);
  html.replace("%REDIRECT_PATH%", redirectPath);
  html.replace("%PASSWORD_MIN%", String(WIFI_PASSWORD_MIN_LENGTH));
  html.replace("%PASSWORD_MAX%", String(WIFI_PASSWORD_MAX_LENGTH));
  return html;
}

String renderForcePasswordChangePage(const String& csrfToken) {
  String html = FPSTR(HTML_FORCE_PASSWORD_CHANGE_TEMPLATE);
  html.replace("%CSS%", FPSTR(HTML_COMMON_CSS));
  html.replace("%CSRF_TOKEN%", csrfToken);
  html.replace("%PASSWORD_MIN%", String(WIFI_PASSWORD_MIN_LENGTH));
  html.replace("%PASSWORD_MAX%", String(WIFI_PASSWORD_MAX_LENGTH));
  return html;
}

String renderRootPage(const String& csrfToken) {
  bool staConnected = WiFi.status() == WL_CONNECTED;
  String staIP = staConnected ? WiFi.localIP().toString() : "未接続";
  int apClients = WiFi.softAPgetStationNum();
  uint32_t freeHeap = ESP.getFreeHeap();

  String html = FPSTR(HTML_ROOT_TEMPLATE);
  html.replace("%CSS%", FPSTR(HTML_COMMON_CSS));
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
  html.replace("%CSRF_TOKEN%", csrfToken);
  return html;
}

String renderDNSFilterPage(const DNSStats& stats, int blocklistCount, bool enabled, const String& csrfToken) {
  String html = FPSTR(HTML_DNS_FILTER_TEMPLATE);
  html.replace("%ENABLED_CHECKED%", enabled ? "checked" : "");
  html.replace("%TOTAL_QUERIES%", String(stats.totalQueries));
  html.replace("%BLOCKED_QUERIES%", String(stats.blockedQueries));
  html.replace("%ALLOWED_QUERIES%", String(stats.allowedQueries));
  html.replace("%BLOCKLIST_COUNT%", String(blocklistCount));
  html.replace("%BLOCKED_PERCENT%", calculatePercentText(stats.blockedQueries, stats.totalQueries));
  html.replace("%ALLOWED_PERCENT%", calculatePercentText(stats.allowedQueries, stats.totalQueries));
  html.replace("%CSRF_TOKEN%", csrfToken);
  return html;
}
