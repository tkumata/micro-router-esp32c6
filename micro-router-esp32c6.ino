/*
 * XIAO ESP32C6 Micro Wi-Fi Router - 完全版
 *
 * このスケッチは ESP32C6 を Wi-Fi ルーターとして動作させます。
 *
 * 機能:
 * - Wi-Fi AP モード（自分の Wi-Fi を提供）
 * - Wi-Fi STA モード（既存 Wi-Fi に接続）
 * - Web UI による設定（http://192.168.4.1）
 * - Preferences (NVS) による設定の不揮発性保存
 * - 初回起動と設定済み起動の自動判定
 * - NAT/NAPT によるパケット転送
 * - DNS フォワーディング
 * - 自動再接続機能
 * - メモリ監視
 *
 * ハードウェア: Seeed Studio XIAO ESP32C6
 * ボード設定: XIAO_ESP32C6
 * シリアルボーレート: 115200
 */

#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <esp_netif.h>  // ESP-IDF netif API（NAT機能用）

// ===== AP モード設定 =====
const char* AP_SSID = "micro-router-esp32c6";
const char* AP_PASSWORD = "esp32c6router";  // WPA2-PSK用パスワード (8文字以上)
const uint8_t AP_CHANNEL = 6;
const uint8_t AP_MAX_CONNECTIONS = 3;

// ===== IP アドレス設定 =====
const IPAddress AP_IP(192, 168, 4, 1);
const IPAddress AP_GATEWAY(192, 168, 4, 1);
const IPAddress AP_SUBNET(255, 255, 255, 0);

// ===== タイミング設定 =====
const unsigned long STA_RECONNECT_INTERVAL = 5000;   // STA再接続間隔（ミリ秒）
const unsigned long STATUS_PRINT_INTERVAL = 10000;   // ステータス表示間隔（ミリ秒）
const unsigned long STA_CONNECTION_TIMEOUT = 30000;  // STA接続タイムアウト（ミリ秒）
const unsigned long CONFIG_SAVE_DELAY = 5000;        // 設定保存後の再起動遅延（ミリ秒）
const unsigned long NAT_ENABLE_DELAY = 1000;         // NAT有効化前の遅延（ミリ秒）

// ===== メモリ管理設定 =====
const uint32_t MIN_FREE_HEAP_WARNING = 50000;  // メモリ不足警告閾値（バイト）

// ===== Preferences 設定 =====
const char* PREF_NAMESPACE = "wifi-config";
const char* PREF_KEY_STA_SSID = "sta_ssid";
const char* PREF_KEY_STA_PASSWORD = "sta_password";
const char* PREF_KEY_CONFIGURED = "configured";

// ===== グローバル変数 =====
WebServer server(80);
Preferences preferences;

// 設定データ構造体
struct WifiConfig {
  char sta_ssid[33];      // SSID 最大 32 文字 + NULL
  char sta_password[65];  // パスワード最大 64 文字 + NULL
  bool configured;        // 設定済みフラグ
} config;

// ===== 状態管理変数 =====
unsigned long lastReconnectAttempt = 0;  // 最後の再接続試行時刻
unsigned long lastStatusPrint = 0;       // 最後のステータス表示時刻
bool lastSTAConnected = false;           // 前回の STA 接続状態
bool natEnabled = false;                 // NAT 有効化済みフラグ
bool needEnableNAT = false;              // NAT 有効化リクエストフラグ

// ===== 関数プロトタイプ =====
// 設定管理
void loadConfig();
void saveConfig(const char* ssid, const char* password);

// Wi-Fi セットアップ
void setupAP();
void setupSTA();

// NAT 機能
void enableNAT();

// イベントハンドラ
void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info);

// Web サーバー
void handleRoot();
void handleSave();

// ループ処理
void processNATEnableRequest();
void checkAndReconnectSTA();
void printPeriodicStatus();

// ユーティリティ
void printMemoryInfo();
void printSeparator(const char* title = nullptr);

void setup() {
  // シリアル通信の初期化
  Serial.begin(115200);
  delay(1000);  // シリアルの安定化を待つ

  Serial.println();
  printSeparator("XIAO ESP32C6 マイクロ Wi-Fi ルーター");
  Serial.println();

  // WiFi イベントハンドラの登録
  WiFi.onEvent(onWiFiEvent);
  Serial.println("WiFi イベントハンドラを登録しました");

  // 設定の読み込み
  loadConfig();

  // 設定状態による起動モード分岐
  if (config.configured) {
    // 設定済み: STA + AP モード
    Serial.println("=== 設定済みモード ===");
    Serial.print("接続先 SSID: ");
    Serial.println(config.sta_ssid);

    WiFi.mode(WIFI_AP_STA);
    setupSTA();
    setupAP();

    // STA 接続状態を記録
    lastSTAConnected = (WiFi.status() == WL_CONNECTED);
  } else {
    // 未設定: AP モードのみ
    Serial.println("=== 初回起動モード ===");
    Serial.println("Web UI で Wi-Fi 設定を行ってください");
    Serial.println("http://192.168.4.1 にアクセスしてください");

    WiFi.mode(WIFI_AP);
    setupAP();
  }

  // Web サーバーの起動
  server.on("/", handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.begin();
  Serial.println();
  Serial.println("Web サーバー起動: http://192.168.4.1");

  Serial.println();
  printSeparator("セットアップ完了");
  Serial.println();
}

// ===== ループ処理 =====

void loop() {
  // NAT 有効化リクエストの処理
  processNATEnableRequest();

  // Web サーバーのリクエスト処理
  server.handleClient();

  // STA 再接続処理
  checkAndReconnectSTA();

  // 定期的なステータス表示
  printPeriodicStatus();
}

/**
 * NAT 有効化リクエストを処理する
 */
void processNATEnableRequest() {
  if (needEnableNAT) {
    needEnableNAT = false;
    Serial.println();
    Serial.println("loop() から NAT を有効化します...");
    delay(NAT_ENABLE_DELAY);  // lwIP スタックの安定化を待つ
    enableNAT();
    natEnabled = true;
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

/**
 * 定期的にステータス情報を表示する
 */
void printPeriodicStatus() {
  unsigned long now = millis();

  if (now - lastStatusPrint >= STATUS_PRINT_INTERVAL) {
    int clientCount = WiFi.softAPgetStationNum();
    bool staConnected = WiFi.status() == WL_CONNECTED;
    uint32_t freeHeap = ESP.getFreeHeap();
    uint32_t minFreeHeap = ESP.getMinFreeHeap();

    Serial.print("[");
    Serial.print(now / 1000);
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
    Serial.print(freeHeap / 1024);
    Serial.print(" KB (最小: ");
    Serial.print(minFreeHeap / 1024);
    Serial.print(" KB)");

    // メモリ不足の警告
    if (freeHeap < MIN_FREE_HEAP_WARNING) {
      Serial.print(" ⚠ 警告: メモリ不足");
    }

    Serial.println();

    lastStatusPrint = now;
  }
}

// ===== 設定管理関数 =====

/**
 * Preferences から設定を読み込む
 */
void loadConfig() {
  Serial.println("--- 設定読み込み開始 ---");

  preferences.begin(PREF_NAMESPACE, true);  // Read-only モード

  String ssid = preferences.getString(PREF_KEY_STA_SSID, "");
  String password = preferences.getString(PREF_KEY_STA_PASSWORD, "");
  config.configured = preferences.getBool(PREF_KEY_CONFIGURED, false);

  ssid.toCharArray(config.sta_ssid, 33);
  password.toCharArray(config.sta_password, 65);

  preferences.end();

  Serial.print("設定済みフラグ: ");
  Serial.println(config.configured ? "YES" : "NO");
  if (config.configured) {
    Serial.print("保存されている SSID: ");
    Serial.println(config.sta_ssid);
  }

  Serial.println("--- 設定読み込み完了 ---");
  Serial.println();
}

/**
 * Preferences に設定を保存する
 */
void saveConfig(const char* ssid, const char* password) {
  Serial.println("--- 設定保存開始 ---");

  preferences.begin(PREF_NAMESPACE, false);  // Read/Write モード

  preferences.putString(PREF_KEY_STA_SSID, ssid);
  preferences.putString(PREF_KEY_STA_PASSWORD, password);
  preferences.putBool(PREF_KEY_CONFIGURED, true);

  preferences.end();

  Serial.print("保存した SSID: ");
  Serial.println(ssid);
  Serial.println("パスワード: ********");
  Serial.println("設定済みフラグ: YES");

  Serial.println("--- 設定保存完了 ---");
}

// ===== Wi-Fi セットアップ関数 =====

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

  // AP モードを起動
  // WiFi.softAP(ssid, password, channel, ssid_hidden, max_connection)
  if (!WiFi.softAP(AP_SSID, AP_PASSWORD, AP_CHANNEL, 0, AP_MAX_CONNECTIONS)) {
    Serial.println("エラー: AP 起動に失敗しました");
    return;
  }

  Serial.println("AP モード起動成功");
  Serial.print("SSID: ");
  Serial.println(AP_SSID);
  Serial.print("パスワード: ");
  Serial.println(AP_PASSWORD);
  Serial.print("チャンネル: ");
  Serial.println(AP_CHANNEL);
  Serial.print("最大接続数: ");
  Serial.println(AP_MAX_CONNECTIONS);

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

// ===== イベントハンドラ =====

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

// ===== NAT 機能 =====

/**
 * NAT/NAPT 機能を有効化
 *
 * - ESP-IDF の esp_netif API を使用
 * - IP フォワーディングを開始
 * - DNS フォワーディングは lwIP が自動処理
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

// ===== Web サーバーハンドラ =====

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
  html += "</style>";
  html += "</head><body>";

  html += "<h1>XIAO ESP32C6 マイクロルーター</h1>";

  // ステータス表示
  html += "<div class='status'>";
  html += "<h2>ステータス</h2>";
  html += "<p>STA 接続: <strong class='" + String(staConnected ? "connected" : "disconnected") + "'>";
  html += staConnected ? "接続済み" : "未接続";
  html += "</strong></p>";
  html += "<p>STA IP: <strong>" + staIP + "</strong></p>";
  html += "<p>AP クライアント数: <strong>" + String(apClients) + " / " + String(AP_MAX_CONNECTIONS) + "</strong></p>";
  html += "<p>空きメモリ: <strong>" + String(freeHeap / 1024) + " KB</strong></p>";
  html += "</div>";

  // 設定フォーム
  html += "<h2>Wi-Fi 設定</h2>";
  html += "<div class='status'>";
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
  printSeparator("設定保存完了");
  Serial.print(CONFIG_SAVE_DELAY / 1000);
  Serial.println("秒後に再起動します");
  printSeparator();

  // 再起動
  delay(CONFIG_SAVE_DELAY);
  ESP.restart();
}

// ===== ユーティリティ関数 =====

/**
 * メモリ情報を表示する
 */
void printMemoryInfo() {
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
void printSeparator(const char* title) {
  Serial.println("========================================");
  if (title != nullptr && strlen(title) > 0) {
    Serial.println(title);
    Serial.println("========================================");
  }
}
