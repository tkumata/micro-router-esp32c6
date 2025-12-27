# XIAO ESP32C6 マイクロ Wi-Fi ルーター - データ構造とネットワーク構成

## 1. ネットワーク構成図

```
┌─────────────────────────────────────────────────────┐
│                     Wi-Fi Network                   │
│                  (e,g: 192.168.1.0/24)              │
│                                                     │
│  ┌──────────────┐                                   │
│  │ Home Router  │                                   │
│  │ 192.168.1.1  │                                   │
│  └──────┬───────┘                                   │
│         │                                           │
└─────────┼───────────────────────────────────────────┘
          │ DHCP で IP 取得
          │ (例: 192.168.1.100)
          │
    ┌─────▼───────────────────────────────────────┐
    │        XIAO ESP32C6 Micro Router            │
    │                                             │
    │  ┌──────────────┐      ┌──────────────┐     │
    │  │  STA Mode    │      │   AP Mode    │     │
    │  │ (Client)     │◄────►│  (Server)    │     │
    │  └──────────────┘      └──────────────┘     │
    │         │                      │            │
    │         │   NAT / Routing      │            │
    │         │   lwIP Stack         │            │
    │         │                      │            │
    │  DHCP: 192.168.1.100  IP: 192.168.4.1       │
    │  (Auto fetch)         Static IP             │
    │                                             │
    │  ┌──────────────────────────────┐           │
    │  │ Web Server (Port 80)         │           │
    │  │ http://192.168.4.1           │           │
    │  └──────────────────────────────┘           │
    │                                             │
    │  ┌──────────────────────────────┐           │
    │  │ NVS (Preferences)            │           │
    │  │ - STA SSID                   │           │
    │  │ - STA Password               │           │
    │  └──────────────────────────────┘           │
    └──────────────────────────┬──────────────────┘
                               │
                               │ AP SSID: micro-router-esp32c6
                               │ Channel: 6
                               │ Security: WPA2-PSK
                               │
        ┌──────────────────────┼──────────────────────┐
        │                      │                      │
    ┌───▼────┐            ┌────▼─────┐          ┌─────▼──┐
    │   PC   │            │  Phone   │          │ Tablet │
    │192.168 │            │ 192.168  │          │192.168 │
    │  .4.2  │            │   .4.3   │          │  .4.4  │
    └────────┘            └──────────┘          └────────┘
     DHCP割当              DHCP割当              DHCP割当
  Web UI アクセス可能   Web UI アクセス可能
```

## 2. IP アドレス割り当て

### 2.1 STA モード (上流ネットワーク)

| 項目         | 値                 | 取得方法      |
| ------------ | ------------------ | ------------- |
| IP アドレス  | 192.168.1.100 (例) | DHCP 自動取得 |
| サブネット   | 255.255.255.0      | DHCP 自動取得 |
| ゲートウェイ | 192.168.1.1 (例)   | DHCP 自動取得 |
| DNS サーバー | 192.168.1.1 (例)   | DHCP 自動取得 |

### 2.2 AP モード (下流ネットワーク)

| 項目         | 値                        | 設定方法 |
| ------------ | ------------------------- | -------- |
| IP アドレス  | 192.168.4.1               | 固定設定 |
| サブネット   | 255.255.255.0             | 固定設定 |
| ゲートウェイ | 192.168.4.1               | 自身     |
| DHCP 範囲    | 192.168.4.2 - 192.168.4.4 | 固定設定 |
| DNS サーバー | STA から取得したもの      | 動的設定 |

## 3. データフロー

### 3.1 パケット送信フロー (クライアント → インターネット)

```
┌──────────┐
│ Client   │ 送信元: 192.168.4.2:12345
│ (PC)     │ 宛先: 8.8.8.8:53 (DNS)
└────┬─────┘
     │ ① パケット送信
     ▼
┌────────────────────────────────────────┐
│  XIAO ESP32C6 - AP Interface           │
│  受信: 192.168.4.2:12345 → 8.8.8.8:53  │
└────┬───────────────────────────────────┘
     │ ② NAT テーブル記録
     │   192.168.4.2:12345 ⇔ 192.168.1.100:34567
     ▼
┌────────────────────────────────────────┐
│  NAT Engine (lwIP)                     │
│  変換: 192.168.1.100:34567 → 8.8.8.8:53│
└────┬───────────────────────────────────┘
     │ ③ 変換後パケット送信
     ▼
┌─────────────────────────────────────────┐
│  XIAO ESP32C6 - STA Interface           │
│  送信: 192.168.10.10x:34567 → 8.8.8.8:53│
└────┬────────────────────────────────────┘
     │ ④ インターネットへ
     ▼
┌──────────┐
│ Internet │
└──────────┘
```

### 3.2 パケット受信フロー (インターネット → クライアント)

```
┌──────────┐
│ Internet │ 送信元: 8.8.8.8:53
└────┬─────┘ 宛先: 192.168.1.100:34567
     │ ① レスポンス受信
     ▼
┌────────────────────────────────────────┐
│  XIAO ESP32C6 - STA Interface          │
│  受信: 8.8.8.8:53 → 192.168.1.100:34567│
└────┬───────────────────────────────────┘
     │ ② NAT テーブル参照
     │   192.168.1.100:34567 ⇔ 192.168.4.2:12345
     ▼
┌────────────────────────────────────────┐
│  NAT Engine (lwIP)                     │
│  変換: 8.8.8.8:53 → 192.168.4.2:12345  │
└────┬───────────────────────────────────┘
     │ ③ 変換後パケット送信
     ▼
┌────────────────────────────────────────┐
│  XIAO ESP32C6 - AP Interface           │
│  送信: 8.8.8.8:53 → 192.168.4.2:12345  │
└────┬───────────────────────────────────┘
     │ ④ クライアントへ配送
     ▼
┌──────────┐
│ Client   │
│ (PC)     │
└──────────┘
```

### 3.3 Web UI 設定フロー

```
┌──────────┐
│ User     │
│ (Phone)  │
└────┬─────┘
     │ ① micro-router-esp32c6 に接続
     ▼
┌────────────────────────────────────────┐
│  XIAO ESP32C6 - AP Mode                │
│  Assign IP: 192.168.4.2                │
└────┬───────────────────────────────────┘
     │ ② http://192.168.4.1 にアクセス
     ▼
┌────────────────────────────────────────┐
│  Web Server (Port 80)                  │
│  GET / → Return setting HTML           │
└────┬───────────────────────────────────┘
     │ ③ 設定画面表示
     ▼
┌──────────┐
│ Browser  │ SSID: "MyHomeWiFi"
│          │ Password: "**********"
└────┬─────┘ [保存して再起動]
     │ ④ POST /save (SSID + Password)
     ▼
┌────────────────────────────────────────┐
│  Web Server                            │
│  設定データを受信                      │
└────┬───────────────────────────────────┘
     │ ⑤ Preferences に保存
     ▼
┌────────────────────────────────────────┐
│  NVS (Preferences)                     │
│  wifi-config/sta_ssid = "MyHomeWiFi"   │
│  wifi-config/sta_password = "********" │
│  wifi-config/configured = true         │
└────┬───────────────────────────────────┘
     │ ⑥ 保存完了 → 再起動
     ▼
┌────────────────────────────────────────┐
│  ESP.restart()                         │
└────────────────────────────────────────┘
```

## 4. メモリ構造

### 4.1 ESP32C6 メモリマップ

```
┌─────────────────────────────────┐
│ Flash (4MB)                     │
│  - Arduino Sketch               │
│  - WiFi ファームウェア          │
│  - lwIP ライブラリ              │
│  - Web UI HTML (埋め込み)       │
└─────────────────────────────────┘

┌─────────────────────────────────┐
│ NVS Partition (~20KB)           │
│  - WiFi 設定 (SSID/Password)    │
│  - その他の永続データ           │
└─────────────────────────────────┘

┌─────────────────────────────────┐
│ SRAM (512KB)                    │
│                                 │
│  ┌───────────────────────────┐  │
│  │ Heap (~200KB 利用可能)    │  │
│  │  - WiFi バッファ          │  │
│  │  - NAT テーブル           │  │
│  │  - DHCP リース情報        │  │
│  │  - TCP/IP スタック        │  │
│  │  - Web サーバーバッファ   │  │
│  └───────────────────────────┘  │
│                                 │
│  ┌───────────────────────────┐  │
│  │ Stack (~30KB)             │  │
│  │  - ローカル変数           │  │
│  │  - 関数呼び出し           │  │
│  └───────────────────────────┘  │
│                                 │
│  ┌───────────────────────────┐  │
│  │ グローバル変数 / BSS      │  │
│  └───────────────────────────┘  │
└─────────────────────────────────┘
```

### 4.2 主要なデータ構造

#### 4.2.1 NAT テーブル (lwIP 内部管理)

```cpp
// lwIP が内部的に管理
struct napt_table {
    uint32_t src_ip;      // 元の送信元 IP (192.168.4.x)
    uint16_t src_port;    // 元の送信元ポート
    uint32_t dest_ip;     // 宛先 IP
    uint16_t dest_port;   // 宛先ポート
    uint32_t nat_ip;      // NAT 後の IP (192.168.1.100)
    uint16_t nat_port;    // NAT 後のポート
    uint32_t timestamp;   // タイムスタンプ
    uint8_t  protocol;    // TCP or UDP
};
// 最大エントリ数: 512 (lwIP デフォルト設定)
```

#### 4.2.2 DHCP リース情報

```cpp
// DHCPServer が内部的に管理
struct dhcp_lease {
    uint8_t  mac[6];      // クライアント MAC アドレス
    uint32_t ip;          // 割り当てた IP アドレス
    uint32_t lease_time;  // リース期限 (Unix タイムスタンプ)
};
// 最大リース数: 3
```

#### 4.2.3 EEPROM 保存データ (Preferences)

```cpp
// Preferences を使った保存データ構造
// 名前空間: "wifi-config"

struct WifiConfigData {
    char sta_ssid[33];       // 最大 32 文字 + null 終端
    char sta_password[65];   // 最大 64 文字 + null 終端
    char ap_password[65];    // 最大 64 文字 + null 終端 (Phase 6)
    bool configured;         // 設定済みフラグ
    bool ap_password_set;    // AP パスワード設定済みフラグ (Phase 6)
};

// 使用例:
Preferences preferences;
preferences.begin("wifi-config", false);  // Read/Write モード

// 読み込み
String ssid = preferences.getString("sta_ssid", "");
String password = preferences.getString("sta_password", "");
String apPassword = preferences.getString("ap_password", "esp32c6router");  // Phase 6
bool isConfigured = preferences.getBool("configured", false);
bool apPasswordSet = preferences.getBool("ap_pw_set", false);  // Phase 6

// 書き込み
preferences.putString("sta_ssid", "MyHomeWiFi");
preferences.putString("sta_password", "mypassword");
preferences.putString("ap_password", "newAPpassword123");  // Phase 6
preferences.putBool("configured", true);
preferences.putBool("ap_pw_set", true);  // Phase 6

preferences.end();
```

**NVS ストレージレイアウト**:

```
┌─────────────────────────────────────┐
│ NVS Partition                       │
│                                     │
│  Namespace: "wifi-config"           │
│  ┌───────────────────────────────┐  │
│  │ Key: "sta_ssid"               │  │
│  │ Type: String                  │  │
│  │ Value: "MyHomeWiFi"           │  │
│  │ Size: 10 bytes                │  │
│  └───────────────────────────────┘  │
│                                     │
│  ┌───────────────────────────────┐  │
│  │ Key: "sta_password"           │  │
│  │ Type: String                  │  │
│  │ Value: "mypassword123"        │  │
│  │ Size: 13 bytes                │  │
│  └───────────────────────────────┘  │
│                                     │
│  ┌───────────────────────────────┐  │
│  │ Key: "ap_password" (Phase 6)  │  │
│  │ Type: String                  │  │
│  │ Value: "newAPpassword123"     │  │
│  │ Size: 16 bytes                │  │
│  └───────────────────────────────┘  │
│                                     │
│  ┌───────────────────────────────┐  │
│  │ Key: "configured"             │  │
│  │ Type: Bool                    │  │
│  │ Value: true                   │  │
│  │ Size: 1 byte                  │  │
│  └───────────────────────────────┘  │
│                                     │
│  ┌───────────────────────────────┐  │
│  │ Key: "ap_pw_set" (Phase 6)    │  │
│  │ Type: Bool                    │  │
│  │ Value: true                   │  │
│  │ Size: 1 byte                  │  │
│  └───────────────────────────────┘  │
│                                     │
│  Total: ~41 bytes (+ metadata)      │
└─────────────────────────────────────┘
```

#### 4.2.4 Wi-Fi 設定情報

```cpp
// ソースコードで定義する構造体
struct wifi_config {
    // STA モード設定 (EEPROM から読み込み)
    char sta_ssid[33];
    char sta_password[65];
    bool sta_configured;

    // AP モード設定
    char ap_ssid[32];         // ハードコード: "micro-router-esp32c6"
    char ap_password[65];     // EEPROM から読み込み (Phase 6)、デフォルト: "esp32c6router"
    bool ap_password_set;     // AP パスワード設定済みフラグ (Phase 6)
    uint8_t ap_channel;
    uint8_t ap_max_connections;

    // IP 設定
    IPAddress ap_ip;
    IPAddress ap_gateway;
    IPAddress ap_subnet;
    IPAddress dhcp_start;
};
```

#### 4.2.5 ステータス情報

```cpp
struct router_status {
    // STA 状態
    bool sta_connected;
    IPAddress sta_ip;
    IPAddress sta_gateway;
    IPAddress sta_dns;
    int8_t sta_rssi;

    // AP 状態
    bool ap_started;
    uint8_t ap_client_count;

    // メモリ情報
    uint32_t free_heap;
    uint32_t min_free_heap;

    // 統計情報
    uint32_t packets_forwarded;
    uint32_t packets_dropped;

    // Web サーバー情報
    bool webserver_running;
    uint32_t web_requests_count;
};
```

#### 4.2.6 Web サーバハンドラ構造

```cpp
// WebServer のエンドポイント定義
struct WebEndpoint {
    const char* path;           // URL パス
    HTTPMethod method;          // GET, POST など
    void (*handler)(void);      // ハンドラー関数
};

// 例:
WebServer server(80);

WebEndpoint endpoints[] = {
    {"/", HTTP_GET, handleRoot},
    {"/save", HTTP_POST, handleSave},
    {"/status", HTTP_GET, handleStatus},
    {"/restart", HTTP_POST, handleRestart}
};
```

## 5. プロトコルスタック

### 5.1 レイヤー構成

```
┌─────────────────────────────────────┐
│  Application Layer                  │
│  - HTTP (Web Server)                │
│  - DNS                              │
└────────────┬────────────────────────┘
             │
┌────────────▼────────────────────────┐
│  Transport Layer (lwIP)             │
│  - TCP (Web Server, HTTP)           │
│  - UDP (DNS)                        │
└────────────┬────────────────────────┘
             │
┌────────────▼────────────────────────┐
│  Network Layer (lwIP)               │
│  - IP (IPv4)                        │
│  - NAT / Routing                    │
│  - ICMP                             │
└────────────┬────────────────────────┘
             │
┌────────────▼────────────────────────┐
│  Link Layer                         │
│  - WiFi (IEEE 802.11)               │
│  - ARP                              │
└─────────────────────────────────────┘
```

### 5.2 lwIP 設定パラメータ (推奨値)

```cpp
// メモリプール設定
#define MEMP_NUM_TCP_PCB        8      // TCP コネクション数
#define MEMP_NUM_UDP_PCB        8      // UDP ソケット数
#define MEMP_NUM_NETBUF         16     // ネットワークバッファ数
#define MEMP_NUM_NETCONN        10     // 同時接続数

// TCP 設定
#define TCP_MSS                 1460   // TCP 最大セグメントサイズ
#define TCP_SND_BUF            (4 * TCP_MSS)  // 送信バッファ
#define TCP_WND                (4 * TCP_MSS)  // 受信ウィンドウ

// NAT 設定
#define IP_NAPT                 1      // NAT 有効化
#define IP_NAPT_MAX             512    // NAT テーブルサイズ
#define IP_PORTMAP_MAX          32     // ポートマップ数
```

## 6. 状態遷移図

### 6.1 起動シーケンス (EEPROM 未設定)

```
[電源投入]
    │
    ▼
[初期化]
 - シリアル初期化
 - WiFi 初期化
 - Preferences 初期化
    │
    ▼
[EEPROM から設定読み込み]
 - configured = false
    │
    ▼
[AP モードのみ起動]
 - SSID: micro-router-esp32c6
 - IP: 192.168.4.1
 - DHCP サーバ起動
 - Web サーバ起動
    │
    ▼
[待機状態]
 - シリアルログ: "STA 未設定。http://192.168.4.1 で設定してください"
 - Web UI で設定を待つ
    │
    │ ◄── ユーザが Web UI で SSID/パスワード入力
    │
    ▼
[設定を EEPROM に保存]
 - sta_ssid = "MyHomeWiFi"
 - sta_password = "********"
 - configured = true
    │
    ▼
[再起動]
 - ESP.restart()
```

### 6.2 起動シーケンス (EEPROM 設定済み)

```
[電源投入]
    │
    ▼
[初期化]
 - シリアル初期化
 - WiFi 初期化
 - Preferences 初期化
    │
    ▼
[EEPROM から設定読み込み]
 - configured = true
 - sta_ssid = "MyHomeWiFi"
 - sta_password = "********"
    │
    ▼
[STA モード接続開始]
 - WiFi.begin(ssid, password)
 - 接続試行 (30秒タイムアウト)
    │
    ├─→ [接続失敗] → [re-try]
    │                    │
    ▼                    │
[STA 接続成功] ◄──────────┘
 - IP アドレス取得
 - DNS サーバー取得
    │
    ▼
[AP モード起動]
 - SSID: micro-router-esp32c6
 - IP: 192.168.4.1
 - DHCP サーバー起動
 - Web サーバー起動
    │
    ▼
[NAT 有効化]
 - lwIP NAT 設定
 - ルーティング設定
    │
    ▼
[運用状態]
 - パケット転送
 - 接続監視
 - Web UI でステータス表示
```

### 6.3 Web UI リクエスト処理フロー

```
[クライアント: GET /]
    │
    ▼
[Web Server: handleRoot()]
 - ステータス情報取得
   - STA 接続状態
   - STA IP
   - AP クライアント数
   - 空きメモリ
    │
    ▼
[HTML 生成]
 - ステータス表示部分
 - SSID/Password フォーム
 - 保存ボタン
    │
    ▼
[HTTP 200 OK + HTML]
    │
    ▼
[クライアントに送信]


[クライアント: POST /save]
 - ssid=MyHomeWiFi
 - password=mypassword
    │
    ▼
[Web Server: handleSave()]
 - フォームデータ解析
 - 入力検証
   - SSID 長さ <= 32
   - Password 長さ <= 64
    │
    ├─→ [検証失敗] → [エラーページ]
    │
    ▼
[Preferences に保存]
 - putString("sta_ssid", ssid)
 - putString("sta_password", password)
 - putBool("configured", true)
    │
    ▼
[成功ページ返送]
 - "設定を保存しました"
 - "3秒後に再起動します"
    │
    ▼
[3秒待機]
    │
    ▼
[再起動]
 - ESP.restart()
```

## 7. パフォーマンス考慮事項

### 7.1 メモリ使用量の目安

| 項目             | サイズ     |
| ---------------- | ---------- |
| WiFi スタック    | ~50KB      |
| lwIP スタック    | ~30KB      |
| NAT テーブル     | ~25KB      |
| DHCP サーバー    | ~5KB       |
| Web サーバー     | ~8KB       |
| アプリケーション | ~10KB      |
| **合計**         | **~128KB** |
| **空きヒープ**   | **~72KB**  |

### 7.2 NVS (EEPROM) 使用量

| 項目              | サイズ         |
| ----------------- | -------------- |
| sta_ssid          | 最大 33 bytes  |
| sta_password      | 最大 65 bytes  |
| ap_password (Ph6) | 最大 65 bytes  |
| configured        | 1 byte         |
| ap_pw_set (Ph6)   | 1 byte         |
| NVS メタデータ    | ~100 bytes     |
| **合計**          | **~265 bytes** |

### 7.3 スループット制限要因

1. **WiFi 無線性能**: 実測 10-30 Mbps (環境依存)
2. **CPU 処理能力**: NAT/ルーティング処理による制限
3. **メモリ帯域**: パケットバッファのコピー処理
4. **バッファサイズ**: TCP ウィンドウサイズによる制限

### 7.4 レイテンシ要因

| 処理             | 遅延       |
| ---------------- | ---------- |
| NAT ルックアップ | < 1ms      |
| パケット転送     | < 2ms      |
| WiFi 送受信      | 2-5ms      |
| **合計**         | **< 10ms** |

### 7.5 Web UI レスポンスタイム

| 処理                  | 時間    |
| --------------------- | ------- |
| GET / (トップページ)  | < 200ms |
| POST /save (設定保存) | < 100ms |
| Preferences 読み書き  | < 10ms  |
| 再起動                | ~3 秒   |
