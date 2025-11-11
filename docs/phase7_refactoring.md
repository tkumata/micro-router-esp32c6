# Phase 7: リファクタリング - コードの整理と構造化

## 概要

Phase 6 までで、マイクロ Wi-Fi ルーターの全機能が実装されました。Phase 7 では、単一ファイル（765 行）に実装されたコードを、機能ごとに分割・整理し、メンテナンス性と可読性を向上させます。

**重要**: このフェーズは**機能追加なし**のリファクタリングです。既存の動作を完全に保持したまま、コード構造のみを改善します。

## 現状の課題

### 1. 単一ファイルの肥大化

- `micro-router-esp32c6.ino` が 765 行に達している
- 全ての機能が一つのファイルに混在
- スクロールが多く、目的のコードを見つけにくい

### 2. 責任の分離不足

- 設定管理、Wi-Fi 制御、NAT 機能、Web UI が同じファイル内に存在
- 機能間の依存関係が不明瞭
- 単体テストやデバッグが困難

### 3. 定数とロジックの混在

- グローバル定数がファイル冒頭に羅列
- ロジックとの境界が不明確

## リファクタリングの目標

### 1. 機能ごとのファイル分割

各機能を独立したヘッダー/ソースファイルに分離し、責任を明確化します。

### 2. Arduino IDE の制約を守る

- 同一ディレクトリ内の `.h`/`.cpp` ファイルを自動認識
- `.ino` ファイルはプロジェクトのエントリーポイント
- Arduino IDE のビルドシステムと互換性を保つ

### 3. 既存動作の完全保持

- 機能変更は一切行わない
- リファクタリング後も同じ動作を保証
- ユーザーから見た挙動は変わらない

## 新しいファイル構成

```
micro-router-esp32c6/
├── micro-router-esp32c6.ino   # メインファイル（setup/loop のみ、約 100 行）
├── Config.h                    # 定数定義・設定値（約 60 行）
├── Types.h                     # 構造体・型定義（約 30 行）
├── ConfigManager.h             # Preferences 管理（宣言、約 20 行）
├── ConfigManager.cpp           # Preferences 管理（実装、約 100 行）
├── WiFiManager.h               # Wi-Fi AP/STA 管理（宣言、約 30 行）
├── WiFiManager.cpp             # Wi-Fi AP/STA 管理（実装、約 200 行）
├── NATManager.h                # NAT 機能（宣言、約 15 行）
├── NATManager.cpp              # NAT 機能（実装、約 120 行）
├── WebUIManager.h              # Web UI ハンドラ（宣言、約 20 行）
├── WebUIManager.cpp            # Web UI ハンドラ（実装、約 250 行）
└── Utils.h                     # ユーティリティ関数（約 50 行）
```

- **合計行数**: 約 995 行（コメント・空行含む）
- **増加理由**: ヘッダーファイルの追加、インクルードガード、ファイル間の境界明確化

## 各ファイルの役割と責任

### 1. Config.h

**役割**: アプリケーション全体の定数定義

**内容**:

- AP モード設定（SSID, パスワード, チャンネル, 最大接続数）
- IP アドレス設定（AP の固定 IP）
- タイミング設定（再接続間隔、タイムアウト、遅延時間）
- メモリ管理設定（警告閾値）
- Preferences のキー名定義
- デフォルト値

**依存関係**: なし（純粋な定数定義）

**例**:

```cpp
#ifndef CONFIG_H
#define CONFIG_H

#include <IPAddress.h>

// ===== AP モード設定 =====
extern const char* AP_SSID;
extern const char* AP_PASSWORD;
extern const uint8_t AP_CHANNEL;
extern const uint8_t AP_MAX_CONNECTIONS;

// ===== IP アドレス設定 =====
extern const IPAddress AP_IP;
extern const IPAddress AP_GATEWAY;
extern const IPAddress AP_SUBNET;

// ... その他の定数

#endif // CONFIG_H
```

### 2. Types.h

**役割**: 共通データ構造の定義

**内容**:

- `WifiConfig` 構造体（SSID, パスワード, 設定済みフラグ）
- その他の共通型定義

**依存関係**: なし

**例**:

```cpp
#ifndef TYPES_H
#define TYPES_H

struct WifiConfig {
  char sta_ssid[33];      // SSID 最大 32 文字 + NULL
  char sta_password[65];  // パスワード最大 64 文字 + NULL
  char ap_password[65];   // AP パスワード最大 64 文字 + NULL
  bool configured;        // 設定済みフラグ
  bool ap_password_set;   // AP パスワード設定済みフラグ
};

#endif // TYPES_H
```

### 3. ConfigManager.h / ConfigManager.cpp

**役割**: NVS (Non-Volatile Storage) を使った設定の永続化

**内容**:

- `void loadConfig()` - Preferences から設定を読み込む
- `void saveConfig(const char* ssid, const char* password)` - STA 設定を保存
- `void saveAPPassword(const char* password)` - AP パスワードを保存
- グローバル変数 `config` (WifiConfig 構造体)
- グローバル変数 `preferences` (Preferences オブジェクト)

**依存関係**:

- `Preferences.h` (Arduino ライブラリ)
- `Config.h` (キー名定義)
- `Types.h` (WifiConfig 構造体)

**設計方針**:

- Preferences API をカプセル化
- 設定の読み書きをこのモジュールに集約
- シリアル出力による設定内容のログ

### 4. WiFiManager.h / WiFiManager.cpp

**役割**: Wi-Fi の AP モードと STA モードの管理

**内容**:

- `void setupAP()` - AP モードの起動
- `void setupSTA()` - STA モードの接続
- `void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info)` - Wi-Fi イベントハンドラ
- `void checkAndReconnectSTA()` - STA 自動再接続ロジック
- グローバル変数: `lastReconnectAttempt`, `lastSTAConnected`

**依存関係**:

- `WiFi.h` (ESP32 標準)
- `Config.h` (AP/STA 設定値)
- `Types.h` (WifiConfig)
- `ConfigManager.h` (設定データへのアクセス)
- `NATManager.h` (NAT 有効化リクエスト)

**設計方針**:

- Wi-Fi の状態管理を一元化
- イベント駆動の再接続処理
- NAT Manager と疎結合（フラグ経由で連携）

### 5. NATManager.h / NATManager.cpp

**役割**: NAT/NAPT 機能の有効化と管理

**内容**:

- `void enableNAT()` - ESP-IDF の esp_netif API を使った NAT 有効化
- `void processNATEnableRequest()` - loop() から呼ばれる NAT 有効化処理
- グローバル変数: `natEnabled`, `needEnableNAT`

**依存関係**:

- `esp_netif.h` (ESP-IDF API)
- `Config.h` (NAT 有効化遅延時間)

**設計方針**:

- NAT 有効化はイベントハンドラではなく loop() から実行（ネットワークスタックの安定性確保）
- フラグベースの非同期処理
- エラーハンドリングと詳細なログ出力
- ESP32C6 では Arduino Core の lwIP NAT API が使えないため、ESP-IDF の esp_netif API を使用

### 6. WebUIManager.h / WebUIManager.cpp

**役割**: HTTP サーバーと Web UI の提供

**内容**:

- `void setupWebServer()` - Web サーバーの初期化とルート登録
- `void handleRoot()` - ルートページ（ステータス表示 + 設定フォーム）
- `void handleSave()` - STA 設定保存エンドポイント
- `void handleSaveAPPassword()` - AP パスワード保存エンドポイント
- グローバル変数: `server` (WebServer オブジェクト)

**依存関係**:

- `WebServer.h` (ESP32 標準)
- `WiFi.h` (ステータス取得)
- `Config.h` (設定値の参照)
- `ConfigManager.h` (設定保存)

**設計方針**:

- HTML 生成ロジックをこのモジュールに集約
- レスポンシブデザインの CSS 埋め込み
- エラー処理とバリデーション
- 保存後の自動再起動

### 7. Utils.h

**役割**: 汎用ユーティリティ関数

**内容**:

- `void printMemoryInfo()` - メモリ使用状況の表示
- `void printSeparator(const char* title)` - 区切り線の表示
- `void printPeriodicStatus()` - 定期的なステータス表示（10 秒間隔）
- グローバル変数: `lastStatusPrint`

**依存関係**:

- `WiFi.h` (ステータス取得)
- `Config.h` (警告閾値)

**設計方針**:

- デバッグとログ出力に特化
- インライン実装（ヘッダーファイルのみ）

### 8. micro-router-esp32c6.ino (メインファイル)

**役割**: アプリケーションのエントリーポイント

**内容**:

- `setup()` - 初期化処理の統括
- `loop()` - メインループ処理の統括
- 各マネージャーのインクルード

**依存関係**: すべてのヘッダーファイル

**設計方針**:

- ロジックを極力書かない（マネージャーに委譲）
- 起動シーケンスの可視化
- シンプルで読みやすい構造

**例**:

```cpp
#include "Config.h"
#include "Types.h"
#include "ConfigManager.h"
#include "WiFiManager.h"
#include "NATManager.h"
#include "WebUIManager.h"
#include "Utils.h"

void setup() {
  Serial.begin(115200);
  delay(1000);

  printSeparator("XIAO ESP32C6 マイクロ Wi-Fi ルーター");

  WiFi.onEvent(onWiFiEvent);
  loadConfig();

  if (config.configured) {
    WiFi.mode(WIFI_AP_STA);
    setupSTA();
    setupAP();
  } else {
    WiFi.mode(WIFI_AP);
    setupAP();
  }

  setupWebServer();
  printSeparator("セットアップ完了");
}

void loop() {
  processNATEnableRequest();
  server.handleClient();
  checkAndReconnectSTA();
  printPeriodicStatus();
}
```

## リファクタリングの実施手順

### ステップ 1: 定数と型の分離

**作業内容**:

1. `Config.h` を作成し、全ての定数を移動
2. `Types.h` を作成し、`WifiConfig` 構造体を移動
3. メインファイルで `#include` を追加
4. ビルドして動作確認

**検証方法**:

- コンパイルが通ること
- シリアル出力で正常に起動すること

### ステップ 2: ConfigManager の分離

**作業内容**:

1. `ConfigManager.h` / `ConfigManager.cpp` を作成
2. `loadConfig()`, `saveConfig()`, `saveAPPassword()` を移動
3. `preferences`, `config` 変数を extern 宣言
4. メインファイルで `#include` を追加
5. ビルドして動作確認

**検証方法**:

- Web UI で STA 設定を保存して再起動後に反映されること
- AP パスワード変更が動作すること

### ステップ 3: WiFiManager の分離

**作業内容**:

1. `WiFiManager.h` / `WiFiManager.cpp` を作成
2. `setupAP()`, `setupSTA()`, `onWiFiEvent()`, `checkAndReconnectSTA()` を移動
3. 状態管理変数（`lastReconnectAttempt` 等）を移動
4. ビルドして動作確認

**検証方法**:

- AP モードで接続できること
- STA モードで既存 Wi-Fi に接続できること
- 切断時の自動再接続が動作すること

### ステップ 4: NATManager の分離

**作業内容**:

1. `NATManager.h` / `NATManager.cpp` を作成
2. `enableNAT()`, `processNATEnableRequest()` を移動
3. `natEnabled`, `needEnableNAT` 変数を移動
4. ビルドして動作確認

**検証方法**:

- AP に接続したデバイスからインターネットにアクセスできること
- `ping 8.8.8.8` が通ること
- `curl http://google.com` が動作すること

### ステップ 5: WebUIManager の分離

**作業内容**:

1. `WebUIManager.h` / `WebUIManager.cpp` を作成
2. `setupWebServer()`, `handleRoot()`, `handleSave()`, `handleSaveAPPassword()` を移動
3. `server` 変数を extern 宣言
4. ビルドして動作確認

**検証方法**:

- http://192.168.4.1 にアクセスできること
- ステータス表示が正常に動作すること
- 設定フォームが動作すること

### ステップ 6: Utils の分離

**作業内容**:

1. `Utils.h` を作成
2. `printMemoryInfo()`, `printSeparator()`, `printPeriodicStatus()` を移動
3. ビルドして動作確認

**検証方法**:

- シリアルモニタで定期的なステータス表示が出力されること
- メモリ警告が適切に表示されること

### ステップ 7: メインファイルの簡素化

**作業内容**:

1. `micro-router-esp32c6.ino` を整理
2. `setup()` と `loop()` のみを残す
3. 全てのヘッダーをインクルード
4. 最終ビルドと動作確認

**検証方法**:

- 全機能が Phase 6 と同じように動作すること
- メモリ使用量が大きく変わらないこと

## リファクタリング後の期待効果

### 1. 可読性の向上

- 各ファイルが 250 行以下に収まる
- 機能ごとにファイルが分かれ、目的のコードを見つけやすい
- コメントとコードの比率が改善

### 2. メンテナンス性の向上

- 機能追加時の影響範囲が明確
- バグ修正が該当ファイルのみで完結
- 責任の分離により変更が安全

### 3. テスト容易性の向上

- 各マネージャーを独立してテスト可能
- モック作成が容易（依存関係が明確）
- デバッグ時の切り分けが簡単

### 4. 拡張性の向上

- 新機能の追加がモジュールとして実装可能
- 既存コードへの影響を最小化
- 将来的なライブラリ化の基盤

## Arduino IDE での注意点

### 1. ファイル配置

- **すべてのファイルは `micro-router-esp32c6/` ディレクトリ直下に配置**
- サブディレクトリは使用しない（Arduino IDE の自動認識の制約）

### 2. ビルド順序

- `.ino` ファイルは最後にコンパイルされる
- `.cpp` ファイルは `.ino` より先にコンパイルされる
- グローバル変数の初期化順序に注意

### 3. インクルード順序

- 依存関係を考慮した順序でインクルード
- Arduino ライブラリ → 自作ヘッダー の順

**推奨順序**:

```cpp
// Arduino 標準ライブラリ
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>

// 自作ヘッダー（依存関係の少ない順）
#include "Config.h"
#include "Types.h"
#include "ConfigManager.h"
#include "WiFiManager.h"
#include "NATManager.h"
#include "WebUIManager.h"
#include "Utils.h"
```

### 4. extern 宣言の使い方

- ヘッダーファイルで `extern` 宣言
- 実装ファイル (`.cpp`) または `.ino` で実体を定義
- 複数ファイルから同じ変数にアクセス可能

**例**:

```cpp
// Config.h
extern const char* AP_SSID;

// Config.cpp または .ino
const char* AP_SSID = "micro-router-esp32c6";
```

## リスクと対策

### リスク 1: ビルドエラー

**原因**: インクルード順序の誤り、extern 宣言の不一致

**対策**:

- 段階的にリファクタリングし、各ステップでビルド確認
- コンパイルエラーが出たら直前のステップに戻る

### リスク 2: リンクエラー

**原因**: 関数や変数の定義が見つからない

**対策**:

- extern 宣言と実体定義の対応を確認
- ヘッダーガードの記述漏れをチェック

### リスク 3: 実行時エラー

**原因**: 初期化順序の誤り、グローバル変数の競合

**対策**:

- setup() での初期化順序を Phase 6 と同じに保つ
- グローバル変数の初期化タイミングを確認

### リスク 4: メモリ不足

**原因**: ファイル分割によるオーバーヘッド

**対策**:

- リファクタリング前後でメモリ使用量を比較
- inline 関数を活用してコードサイズを削減

## 成功基準

リファクタリング完了の判断基準:

- ✅ **コンパイル**: エラーなくビルドが完了する
- ✅ **起動**: Phase 6 と同じシリアル出力で起動する
- ✅ **AP モード**: デバイスから接続できる
- ✅ **STA モード**: 既存 Wi-Fi に接続できる
- ✅ **NAT**: AP 経由でインターネットにアクセスできる
- ✅ **Web UI**: 設定画面が表示され、設定保存が動作する
- ✅ **再接続**: STA 切断時の自動再接続が動作する
- ✅ **メモリ**: Phase 6 と比べて使用量が ±10% 以内
- ✅ **ファイルサイズ**: 各ファイルが 250 行以下に収まっている

## Phase 8 以降への展望

Phase 7 のリファクタリングにより、モジュール構造が確立され、機能拡張が容易になりました。

### 今後の候補機能

#### ファイアウォール機能（将来検討）

Phase 7 のリファクタリングにより、ファイアウォール機能の追加が容易になっています：

**実装方針**:

- 新しいモジュール: `FirewallManager.h/.cpp` を作成
- ESP-IDF の `esp_netif` フィルタリング API を使用
- 既存の NAT 機能との統合

**想定機能**:

- IP アドレスベースのブロック/許可リスト
- ポートベースのフィルタリング
- 簡易的な侵入検知（異常なトラフィックの検出）
- Web UI からのルール設定

**実装の容易さ**:

- `FirewallManager.cpp` を追加するだけで、他のモジュールへの影響最小
- `NATManager.cpp` と連携してパケットフィルタリング
- `WebUIManager.cpp` に設定画面を追加

---

## まとめ

Phase 7 のリファクタリングは、コードの品質向上に不可欠なステップでした。機能追加はありませんでしたが、将来の拡張性と保守性を大きく改善しました。

### 実施結果

**所要時間**: 約 2 時間

**達成内容**:

- ✅ メインファイルを 765 行 → 138 行に削減（82% 削減）
- ✅ 11 ファイルへの機能分割完了
- ✅ 全機能が Phase 6 と同じく動作確認済み
- ✅ NAT/ルーティング機能の動作確認済み（google.com, fast.com へのアクセス成功）

**難易度**: 中級（Arduino IDE の仕組みを理解していれば可能）

**優先度**: 高（将来の機能追加の基盤として重要）

**ステータス**: ✅ 完了
