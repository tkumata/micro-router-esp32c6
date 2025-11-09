# XIAO ESP32C6 マイクロ Wi-Fi ルーター - 実装タスク

## 実装フェーズ概要

| フェーズ | 期間目安 | 目的                        |
| -------- | -------- | --------------------------- |
| Phase 1  | 1-2 日   | 基本的な Wi-Fi 接続の確立   |
| Phase 2  | 1-2 日   | Web UI と EEPROM 機能の実装 |
| Phase 3  | 2-3 日   | NAT/ルーティング機能の実装  |
| Phase 4  | 1-2 日   | エラーハンドリングと安定化  |
| Phase 5  | 1 日     | 最適化とテスト              |

---

## Phase 1: 基本実装

### タスク 1.1 - 開発環境のセットアップ

- [x] Arduino IDE 2.0 以降をインストール
- [x] ESP32 ボードマネージャーの追加
  - ファイル → 環境設定 → 追加のボードマネージャーの URL
  - URL: `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
- [x] ESP32 ボードパッケージのインストール
  - ツール → ボードマネージャー → "esp32" で検索
  - **"ESP32 by Espressif Systems" バージョン 3.0.0 以降をインストール**
  - **重要**: XIAO ESP32C6 のサポートは 3.0.0 以降でのみ利用可能
- [x] XIAO ESP32C6 ボードの選択
  - ツール → ボード → esp32 → **XIAO_ESP32C6**
- [x] シリアルポートの確認とボーレート設定 (115200)
- [x] 必要なライブラリの確認（すべて ESP32 Arduino Core に標準で含まれる）
  - WiFi.h (ESP32 標準)
  - WebServer.h (ESP32 標準)
  - Preferences.h (ESP32 標準)
  - lwIP NAT (ESP32 Arduino Core 3.0.0 以降に含まれる)

**検証方法**:

```cpp
void setup() {
  Serial.begin(115200);
  Serial.println("XIAO ESP32C6 テスト");
}
void loop() {}
```

をアップロードし、シリアルモニタで出力を確認。

---

### タスク 1.2 - Wi-Fi AP モードの実装（初回起動用）

- [x] AP モード設定コードの実装
- [x] SSID/パスワード/チャンネルの定義
- [x] 固定 IP アドレスの設定 (192.168.4.1)
- [x] 最大接続数の制限 (3 台)

**実装コード例**:

```cpp
#include <WiFi.h>

#define AP_SSID "micro-router-esp32c6"
#define AP_PASSWORD "your_ap_password"
#define AP_CHANNEL 6
#define AP_MAX_CONNECTIONS 3

void setupAP() {
  // AP モード用 IP 設定
  IPAddress apIP(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);

  WiFi.softAPConfig(apIP, gateway, subnet);
  WiFi.softAP(AP_SSID, AP_PASSWORD, AP_CHANNEL, 0, AP_MAX_CONNECTIONS);

  Serial.println("AP モード起動");
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());
  Serial.print("SSID: ");
  Serial.println(AP_SSID);
}

void setup() {
  Serial.begin(115200);
  setupAP();
}

void loop() {
  // 接続中のクライアント数を表示
  Serial.print("接続中: ");
  Serial.println(WiFi.softAPgetStationNum());
  delay(5000);
}
```

**検証方法**:

- PC/スマホから `micro-router-esp32c6` に接続できることを確認
- 接続後、192.168.4.1 に ping が通ることを確認

---

### タスク 1.3 - DHCP サーバーの実装

- [x] DHCP サーバーの範囲設定 (192.168.4.2 - 192.168.4.4)
- [x] リース時間の設定
- [x] DNS サーバー情報の配布設定

**注意事項**:
ESP32 の `WiFi.softAP()` は自動的に DHCP サーバーを起動するため、基本的には追加のコードは不要です。ただし、IP 範囲を明示的に設定する場合は、`tcpip_adapter` API を使用します。

**検証方法**:

- AP に接続したデバイスが 192.168.4.2-4 の IP を取得する
- `ipconfig` / `ifconfig` で確認

---

## Phase 2: Web UI と EEPROM 実装

### タスク 2.1 - Preferences (EEPROM) の実装

- [x] Preferences ライブラリのインクルード
- [x] EEPROM からの設定読み込み
- [x] EEPROM への設定書き込み
- [x] 初回起動判定

**実装コード例**:

```cpp
#include <Preferences.h>

Preferences preferences;

struct WifiConfig {
  char sta_ssid[33];
  char sta_password[65];
  bool configured;
};

WifiConfig config;

void loadConfig() {
  preferences.begin("wifi-config", true);  // Read-only モード

  String ssid = preferences.getString("sta_ssid", "");
  String password = preferences.getString("sta_password", "");
  config.configured = preferences.getBool("configured", false);

  ssid.toCharArray(config.sta_ssid, 33);
  password.toCharArray(config.sta_password, 65);

  preferences.end();

  Serial.println("=== 設定読み込み ===");
  Serial.print("設定済み: ");
  Serial.println(config.configured ? "YES" : "NO");
  if (config.configured) {
    Serial.print("STA SSID: ");
    Serial.println(config.sta_ssid);
  }
}

void saveConfig(const char* ssid, const char* password) {
  preferences.begin("wifi-config", false);  // Read/Write モード

  preferences.putString("sta_ssid", ssid);
  preferences.putString("sta_password", password);
  preferences.putBool("configured", true);

  preferences.end();

  Serial.println("設定を保存しました");
}

void setup() {
  Serial.begin(115200);
  loadConfig();
}
```

**検証方法**:

- 初回起動時は `configured = false` となることを確認
- `saveConfig()` で保存後、再起動して設定が保持されることを確認

---

### タスク 2.2 - Web サーバーの基本実装

- [x] WebServer ライブラリのインクルード
- [x] Web サーバーの起動 (ポート 80)
- [x] ルートパス (/) のハンドラー実装
- [x] 基本的な HTML ページの返送

**実装コード例**:

```cpp
#include <WiFi.h>
#include <WebServer.h>

WebServer server(80);

void handleRoot() {
  String html = "<html><body>";
  html += "<h1>XIAO ESP32C6 Micro Router</h1>";
  html += "<p>Web server is running!</p>";
  html += "</body></html>";

  server.send(200, "text/html", html);
}

void setup() {
  Serial.begin(115200);

  // AP モード起動
  WiFi.softAP("micro-router-esp32c6", "password");

  // Web サーバー設定
  server.on("/", handleRoot);
  server.begin();

  Serial.println("Web サーバー起動: http://192.168.4.1");
}

void loop() {
  server.handleClient();  // クライアントリクエストを処理
}
```

**検証方法**:

- AP に接続後、ブラウザで http://192.168.4.1 にアクセス
- Web ページが表示されることを確認

---

### タスク 2.3 - Web UI の設定画面実装

- [x] ステータス表示機能
- [x] SSID/パスワード入力フォーム
- [x] 保存ボタン
- [x] HTML/CSS の実装

**実装コード例**:

```cpp
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
  html += "body{font-family:Arial;max-width:600px;margin:50px auto;padding:20px;}";
  html += "h1{color:#333;}";
  html += ".status{background:#f0f0f0;padding:15px;border-radius:5px;margin:20px 0;}";
  html += ".form-group{margin:15px 0;}";
  html += "label{display:block;margin-bottom:5px;font-weight:bold;}";
  html += "input[type=text],input[type=password]{width:100%;padding:8px;box-sizing:border-box;}";
  html += "button{background:#007bff;color:white;padding:10px 20px;border:none;border-radius:5px;cursor:pointer;}";
  html += "button:hover{background:#0056b3;}";
  html += "</style>";
  html += "</head><body>";

  html += "<h1>XIAO ESP32C6 マイクロルーター</h1>";

  // ステータス表示
  html += "<div class='status'>";
  html += "<h2>ステータス</h2>";
  html += "<p>STA 接続: <strong>" + String(staConnected ? "接続済み" : "未接続") + "</strong></p>";
  html += "<p>STA IP: <strong>" + staIP + "</strong></p>";
  html += "<p>AP クライアント数: <strong>" + String(apClients) + " / 3</strong></p>";
  html += "<p>空きメモリ: <strong>" + String(freeHeap / 1024) + " KB</strong></p>";
  html += "</div>";

  // 設定フォーム
  html += "<h2>Wi-Fi 設定</h2>";
  html += "<form method='POST' action='/save'>";
  html += "<div class='form-group'>";
  html += "<label>既存 Wi-Fi SSID:</label>";
  html += "<input type='text' name='ssid' required>";
  html += "</div>";
  html += "<div class='form-group'>";
  html += "<label>パスワード:</label>";
  html += "<input type='password' name='password' required>";
  html += "</div>";
  html += "<button type='submit'>保存して再起動</button>";
  html += "</form>";

  html += "</body></html>";

  server.send(200, "text/html", html);
}

void setup() {
  Serial.begin(115200);
  WiFi.softAP("micro-router-esp32c6", "password");

  server.on("/", handleRoot);
  server.begin();
}

void loop() {
  server.handleClient();
}
```

**検証方法**:

- ステータス情報が正しく表示される
- フォームが表示される
- レスポンシブデザインがスマホでも動作する

---

### タスク 2.4 - 設定保存機能の実装

- [x] POST /save エンドポイントの実装
- [x] フォームデータの解析
- [x] 入力検証 (長さチェック)
- [x] EEPROM への保存
- [x] 再起動処理

**実装コード例**:

```cpp
#include <Preferences.h>

Preferences preferences;

void handleSave() {
  // フォームデータ取得
  String ssid = server.arg("ssid");
  String password = server.arg("password");

  // 入力検証
  if (ssid.length() == 0 || ssid.length() > 32) {
    server.send(400, "text/html", "<h1>エラー: SSID は 1~32 文字で入力してください</h1>");
    return;
  }

  if (password.length() < 8 || password.length() > 64) {
    server.send(400, "text/html", "<h1>エラー: パスワードは 8~64 文字で入力してください</h1>");
    return;
  }

  // EEPROM に保存
  preferences.begin("wifi-config", false);
  preferences.putString("sta_ssid", ssid);
  preferences.putString("sta_password", password);
  preferences.putBool("configured", true);
  preferences.end();

  Serial.println("設定を保存: " + ssid);

  // 成功ページ
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta http-equiv='refresh' content='3;url=/'>";
  html += "</head><body>";
  html += "<h1>設定を保存しました</h1>";
  html += "<p>3秒後に再起動します...</p>";
  html += "</body></html>";

  server.send(200, "text/html", html);

  // 再起動
  delay(3000);
  ESP.restart();
}

void setup() {
  Serial.begin(115200);
  WiFi.softAP("micro-router-esp32c6", "password");

  server.on("/", handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.begin();
}

void loop() {
  server.handleClient();
}
```

**検証方法**:

- フォームから SSID/パスワードを入力
- 保存が成功し、再起動すること
- 再起動後、EEPROM から設定が読み込まれること

---

### タスク 2.5 - 起動時の設定判定と分岐

- [x] 起動時に EEPROM を読み込み
- [x] 設定済みの場合は STA モードで接続
- [x] 未設定の場合は AP モードのみで起動

**実装コード例**:

```cpp
#include <WiFi.h>
#include <Preferences.h>
#include <WebServer.h>

Preferences preferences;
WebServer server(80);

struct WifiConfig {
  char sta_ssid[33];
  char sta_password[65];
  bool configured;
} config;

void loadConfig() {
  preferences.begin("wifi-config", true);
  String ssid = preferences.getString("sta_ssid", "");
  String password = preferences.getString("sta_password", "");
  config.configured = preferences.getBool("configured", false);
  ssid.toCharArray(config.sta_ssid, 33);
  password.toCharArray(config.sta_password, 65);
  preferences.end();
}

void setupAP() {
  IPAddress apIP(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.softAPConfig(apIP, gateway, subnet);
  WiFi.softAP("micro-router-esp32c6", "your_password", 6, 0, 3);
  Serial.println("AP モード起動: http://192.168.4.1");
}

void setupSTA() {
  WiFi.begin(config.sta_ssid, config.sta_password);
  Serial.print("STA 接続中");

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 30000) {
    Serial.print(".");
    delay(500);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nSTA 接続成功: " + WiFi.localIP().toString());
  } else {
    Serial.println("\nSTA 接続失敗");
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  // 設定読み込み
  loadConfig();

  if (config.configured) {
    // 設定済み: STA + AP モード
    Serial.println("設定済みモード");
    WiFi.mode(WIFI_AP_STA);
    setupSTA();
    setupAP();
  } else {
    // 未設定: AP モードのみ
    Serial.println("初回起動モード");
    Serial.println("Web UI で設定してください: http://192.168.4.1");
    WiFi.mode(WIFI_AP);
    setupAP();
  }

  // Web サーバー起動
  server.on("/", handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.begin();
}

void loop() {
  server.handleClient();
}
```

**検証方法**:

1. 初回起動時: AP モードのみで起動
2. Web UI で設定保存後: STA + AP モードで起動
3. STA 接続が既存 Wi-Fi に成功すること

---

## Phase 3: NAT/ルーティング実装

### タスク 3.1 - lwIP NAT の有効化

- [x] lwIP の NAT 機能を有効化（ESP-IDF netif API `esp_netif_napt_enable()` を使用）
- [x] メモリ設定の調整
- [x] NAT テーブルサイズの設定

**実装コード例**:

```cpp
#include <lwip/lwip_napt.h>

void enableNAT() {
  // NAT を有効化
  ip_napt_enable_no(SOFTAP_IF, 1);

  Serial.println("NAT 有効化");
  Serial.print("NAT テーブルサイズ: ");
  Serial.println(IP_NAPT_MAX);
}

void setup() {
  Serial.begin(115200);

  // ... STA + AP 起動 ...

  // STA 接続成功後に NAT を有効化
  if (WiFi.status() == WL_CONNECTED) {
    enableNAT();
  }
}
```

**注意事項**:

- ESP32 Arduino Core に lwIP NAT サポートが含まれているか確認が必要
- 最新の ESP32 Arduino Core (2.0.0 以降) では、lwIP NAT 機能がデフォルトで有効化されています
- NAT 機能が利用できない場合は、ESP32 Arduino Core を最新版に更新してください

**検証方法**:

- NAT が有効化されることをログで確認

---

### タスク 3.2 - DNS フォワーディングの実装

- [x] STA から取得した DNS サーバー情報の保存
- [x] AP クライアントへの DNS サーバー情報配布

**実装コード例**:

```cpp
void setup() {
  // ... STA 接続処理 ...

  // STA の DNS サーバーを取得
  IPAddress staDNS = WiFi.dnsIP();
  Serial.print("STA DNS: ");
  Serial.println(staDNS);

  // lwIP の DHCP サーバーが自動的に DNS を配布
  // 追加の設定は基本的に不要
}
```

**検証方法**:

- AP 接続デバイスから `nslookup google.com` でドメイン解決できることを確認

---

### タスク 3.3 - パケット転送の検証

- [x] IP フォワーディングの確認
- [x] NAT 変換の動作確認
- [x] TCP/UDP 両方の転送確認（スマホから https://www.google.com へのアクセス成功）

**検証方法**:

1. AP に PC を接続
2. PC から `ping 8.8.8.8` で Google DNS に到達できることを確認
3. `curl http://google.com` で HTTP 通信ができることを確認
4. ブラウザでインターネットにアクセスできることを確認

---

## Phase 4: 安定化とエラーハンドリング

### タスク 4.1 - STA 再接続処理の実装

- [x] STA 切断検知
- [x] 自動再接続ロジック（5 秒間隔）
- [x] リトライ回数制限（無制限で継続再試行）
- [x] バックオフ戦略（固定 5 秒間隔）

**実装コード例**:

```cpp
unsigned long lastReconnectAttempt = 0;
const unsigned long reconnectInterval = 5000;  // 5秒

void checkSTAConnection() {
  if (config.configured && WiFi.status() != WL_CONNECTED) {
    unsigned long now = millis();
    if (now - lastReconnectAttempt > reconnectInterval) {
      Serial.println("STA 切断検知 - 再接続中...");
      WiFi.disconnect();
      WiFi.begin(config.sta_ssid, config.sta_password);
      lastReconnectAttempt = now;
    }
  }
}

void loop() {
  server.handleClient();
  checkSTAConnection();
}
```

**検証方法**:

- 既存 Wi-Fi を一時的に停止し、自動再接続を確認
- 再接続後もルーティングが正常に動作することを確認

---

### タスク 4.2 - メモリ管理の実装

- [x] ヒープメモリの監視（10 秒ごとに表示）
- [x] メモリリーク検出（最小空きヒープの記録）
- [x] 最小空きヒープの記録

**実装コード例**:

```cpp
void printMemoryInfo() {
  uint32_t freeHeap = ESP.getFreeHeap();
  uint32_t minFreeHeap = ESP.getMinFreeHeap();

  Serial.println("=== メモリ情報 ===");
  Serial.print("空きヒープ: ");
  Serial.print(freeHeap);
  Serial.println(" bytes");
  Serial.print("最小空きヒープ: ");
  Serial.print(minFreeHeap);
  Serial.println(" bytes");

  if (freeHeap < 50000) {
    Serial.println("警告: メモリ不足");
  }
}

// 定期的に呼び出す
void loop() {
  static unsigned long lastMemCheck = 0;
  if (millis() - lastMemCheck > 60000) {  // 1分ごと
    printMemoryInfo();
    lastMemCheck = millis();
  }

  server.handleClient();
}
```

**検証方法**:

- 長時間運転でメモリリークがないことを確認
- 空きヒープが 50KB 以上を維持していることを確認

---

### タスク 4.3 - エラーログとデバッグ機能

- [ ] エラーレベルの定義
- [x] タイムスタンプ付きログ（millis()ベース）
- [x] 接続/切断イベントのログ

**実装コード例**:

```cpp
enum LogLevel { INFO, WARNING, ERROR };

void log(LogLevel level, const char* message) {
  const char* levelStr[] = {"INFO", "WARN", "ERROR"};
  Serial.print("[");
  Serial.print(millis());
  Serial.print("] [");
  Serial.print(levelStr[level]);
  Serial.print("] ");
  Serial.println(message);
}

void setup() {
  Serial.begin(115200);
  log(INFO, "システム起動");
}
```

**検証方法**:

- シリアルモニタで適切なログが出力される

---

### タスク 4.4 - クライアント接続管理

- [x] クライアント接続/切断イベントの検知
- [x] 接続数の制限確認（最大 3 台）

**実装コード例**:

```cpp
#include <esp_wifi.h>

void WiFiStationConnected(WiFiEvent_t event, WiFiEventInfo_t info) {
  Serial.println("クライアント接続");
  Serial.print("接続数: ");
  Serial.println(WiFi.softAPgetStationNum());
}

void WiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info) {
  Serial.println("クライアント切断");
  Serial.print("接続数: ");
  Serial.println(WiFi.softAPgetStationNum());
}

void setup() {
  Serial.begin(115200);

  // イベントハンドラ登録
  WiFi.onEvent(WiFiStationConnected, ARDUINO_EVENT_WIFI_AP_STACONNECTED);
  WiFi.onEvent(WiFiStationDisconnected, ARDUINO_EVENT_WIFI_AP_STADISCONNECTED);

  // ... AP 起動処理 ...
}
```

**検証方法**:

- デバイス接続時にイベントが検知される
- 最大 3 台までしか接続できないことを確認

---

## Phase 5: 最適化とテスト

### タスク 5.1 - パフォーマンス測定

- [ ] スループット測定 (iperf3)
- [ ] レイテンシ測定 (ping)
- [ ] パケットロス率の確認

**測定方法**:

```bash
# PC 側でサーバー起動
iperf3 -s

# AP 接続デバイスからクライアント実行
iperf3 -c <PC_IP>

# ping 測定
ping -c 100 8.8.8.8
```

**目標値**:

- スループット: > 5 Mbps
- レイテンシ: < +10ms
- パケットロス: < 1%

---

### タスク 5.2 - 長時間安定性テスト

- [ ] 24 時間連続動作テスト
- [ ] メモリリークチェック
- [ ] 再接続テスト
- [ ] 負荷テスト (同時接続 3 台)

**テストシナリオ**:

1. システム起動
2. 3 台のデバイスを接続
3. 各デバイスから継続的にトラフィック発生
4. 6 時間ごとに STA 接続を切断/再接続
5. 24 時間後の状態確認

**確認項目**:

- [ ] システムが落ちていない
- [ ] 空きメモリが初期値から大きく減っていない
- [ ] 再接続が正常に動作する
- [ ] ルーティングが正常に機能している

---

### タスク 5.3 - セキュリティ検証

- [ ] WPA2-PSK が正しく動作
- [ ] 不正なパスワードで接続できないことを確認
- [ ] Web UI のアクセス制限確認

**検証方法**:

- 間違ったパスワードで接続を試みる
- AP 未接続の状態で Web UI にアクセスできないことを確認

---

### タスク 5.4 - コードのリファクタリング

- [x] コードの整理とコメント追加
- [x] 設定値の定数化
- [x] 関数の分割と整理
- [x] エラーハンドリングの統一

**リファクタリング項目**:

```cpp
// 定数の定義
const char* AP_SSID = "micro-router-esp32c6";
const char* AP_PASSWORD = "your_password";
const IPAddress AP_IP(192, 168, 4, 1);
const IPAddress AP_GATEWAY(192, 168, 4, 1);
const IPAddress AP_SUBNET(255, 255, 255, 0);

// 関数の分割
void setupWiFi();
void setupWebServer();
void handleWebRequests();
```

---

### タスク 5.5 - ドキュメントの作成

- [x] README.md の作成
- [x] 使用方法の記載
- [ ] 既知の問題と制限事項

**README.md の構成**:

1. プロジェクト概要
2. 必要なハードウェア
3. 使用方法
4. ライセンス

---

## 開発時トラブルシューティング

### 問題 1: NAT が有効化できない

**原因**: ESP32 Arduino Core に lwIP NAT サポートが含まれていない、またはバージョンが古い

**解決策**:

1. Arduino IDE のボードマネージャーから ESP32 Arduino Core を最新版 (2.0.0 以降) に更新
2. Arduino IDE を再起動して変更を反映
3. スケッチを再コンパイルしてアップロード

### 問題 2: AP に接続できるが、インターネットにアクセスできない

**原因**: NAT が正しく動作していない、または STA が既存 Wi-Fi に接続できていない

**確認事項**:

- NAT が有効化されているか
- STA モードで正しく IP を取得しているか
- DNS サーバーが設定されているか

### 問題 3: Web UI にアクセスできない

**原因**: Web サーバーが起動していない、または IP アドレスが違う

**確認事項**:

- `server.begin()` が呼ばれているか
- `server.handleClient()` が `loop()` で呼ばれているか
- AP の IP アドレスが 192.168.4.1 になっているか

### 問題 4: メモリ不足でクラッシュする

**原因**: NAT テーブルや TCP バッファでメモリを使い果たす

**解決策**:

- NAT テーブルサイズを削減
- TCP バッファサイズを削減
- 同時接続数を減らす

### 問題 5: EEPROM に保存できない / 読み込めない

**原因**: Preferences の名前空間が間違っている、または NVS が破損している

**解決策**:

- `preferences.clear()` で設定をクリア
- `esptool.py erase_flash` で Flash を完全消去
- 名前空間名を確認

---

## 完成チェックリスト

### 基本機能

- [x] 初回起動時、AP モードのみで起動する
- [x] Web UI (http://192.168.4.1) にアクセスできる
- [x] Web UI で SSID/パスワードを入力・保存できる
- [x] 再起動後、保存した設定で STA モードに接続できる
- [ ] 3 台のデバイスが AP に接続できる
- [x] DHCP で IP アドレスが自動割り当てされる
- [x] AP 経由でインターネットにアクセスできる
- [x] DNS 解決が正常に動作する

### 安定性

- [ ] 24 時間連続動作できる
- [ ] STA 切断時に自動再接続する
- [ ] メモリリークがない

### パフォーマンス

- [ ] スループット > 5 Mbps
- [ ] レイテンシ < +10ms
- [ ] パケットロス < 1%

### Web UI

- [x] ステータス情報が正しく表示される
- [x] スマホからも操作できる
- [x] 設定変更が正常に動作する

### ドキュメント

- [x] README.md が作成されている
- [x] 設定方法が記載されている
- [ ] トラブルシューティングガイドがある

---

## 参考資料

### ESP32 公式ドキュメント

- ESP32 Arduino Core: https://github.com/espressif/arduino-esp32
- ESP-IDF Programming Guide: https://docs.espressif.com/projects/esp-idf/
- Preferences Library: https://github.com/espressif/arduino-esp32/tree/master/libraries/Preferences

### lwIP ドキュメント

- lwIP Wiki: https://www.nongnu.org/lwip/
- NAT 設定: ESP-IDF の NAT 例を参照

### Web サーバー

- ESP32 WebServer Library: https://github.com/espressif/arduino-esp32/tree/master/libraries/WebServer

### コミュニティリソース

- ESP32 Forum: https://esp32.com/
- Arduino Forum: https://forum.arduino.cc/

---

**最終更新**: 2025-11-08
