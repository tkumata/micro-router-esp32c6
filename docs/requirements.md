# Seeed Studio XIAO ESP32C6 マイクロ Wi-Fi ルーター - 要件定義書

## 1. プロジェクト概要

### 1.1 目的

Seeed Studio XIAO ESP32C6 を使用した極小 Wi-Fi ルーターの実装。既存の Wi-Fi ネットワークに接続しながら、独自のアクセスポイントを提供し、NAT とルーティング機能により接続デバイスをインターネットに接続する。

### 1.2 想定用途

- PC、スマートフォン、タブレットなど最大 3 台のデバイスを、新規 Wi-Fi ネットワークに接続 (XIAO の AP)
- モバイル環境での簡易ルータ
- IoT デバイスのネットワーク分離

## 2. 開発環境

### 2.1 ハードウェア

- **デバイス**: Seeed Studio XIAO ESP32C6
- **チップセット**: ESP32-C6 (RISC-V シングルコア 160MHz)
- **メモリ**: 512KB SRAM, 4MB Flash
- **無線**: Wi-Fi 6 (802.11ax), Bluetooth 5.3
- **不揮発性メモリ**: NVS (Non-Volatile Storage) - EEPROM として利用

### 2.2 ソフトウェア

- **開発環境**: Arduino IDE 2.0 以降推奨
- **ボードパッケージ**: ESP32 by Espressif Systems (バージョン 3.0.0 以降)
  - **重要**: XIAO ESP32C6 は ESP32-C6 チップ (RISC-V) を使用しているため、ESP32 Arduino Core 3.0.0 以降が必須
  - ボードマネージャー URL: `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
  - ボード選択: XIAO_ESP32C6
- **必要ライブラリ**:
  - WiFi.h (ESP32 標準)
  - WebServer.h (ESP32 標準)
  - Preferences.h (ESP32 標準)
  - esp_netif.h (ESP-IDF netif API - NAT 機能用)
  - **重要**: lwIP NAT の低レベル関数（`ip_napt_enable`等）は ESP32C6 環境でロックエラーが発生するため使用不可。ESP-IDF の高レベル API（`esp_netif_napt_enable()`）を使用する必要がある。

## 3. 機能要件

### 3.1 Wi-Fi STA (Station) モード

- **接続先**: 既存の自宅 Wi-Fi アクセスポイント
- **SSID/パスワード設定**: Web UI から入力、EEPROM に保存
- **IP アドレス取得**: DHCP クライアント
- **再接続機能**: 接続が切れた場合の自動再接続
- **接続タイムアウト**: 30 秒

### 3.2 Wi-Fi AP (Access Point) モード

- **SSID**: `micro-router-esp32c6`
- **パスワード**: 設定可能（デフォルト: ユーザー指定）
- **セキュリティ**: WPA2-PSK
- **IP アドレス**: 192.168.4.1
- **サブネットマスク**: 255.255.255.0 (/24)
- **最大接続数**: 3 台

### 3.3 DHCP サーバー

- **IP 範囲**: 192.168.4.2 ~ 192.168.4.4 (3 台分)
- **リース時間**: 7200 秒 (2 時間)
- **ゲートウェイ**: 192.168.4.1 (本デバイス)
- **DNS サーバー**: STA モードで取得した DNS サーバーを配布

### 3.4 NAT (Network Address Translation)

- **機能**: AP 側からの通信を STA 側の IP アドレスに変換
- **プロトコル**: TCP/UDP 対応
- **ポートマッピング**: 動的 NAT (NAPT/PAT)
- **実装方法**: ESP-IDF の `esp_netif_napt_enable()` API を使用
  - **重要な制約**: ESP32C6 + Arduino Core 3.0 環境では、lwIP の低レベル関数（`ip_napt_enable()`, `ip_napt_enable_no()`）を直接呼び出すとアサーションエラーが発生
  - **解決方法**: ESP-IDF の高レベル netif API を使用することで、内部で適切なロック管理が行われる
  - **有効化タイミング**: WiFi イベント `ARDUINO_EVENT_WIFI_STA_GOT_IP` を受信後、loop() 関数から呼び出す
  - **netif ハンドル取得**: 複数のキー名（"WIFI_AP_DEF", "ESP_NETIF_WIFI_AP", "ap", "AP"）を試行して AP インターフェースを取得

### 3.5 ルーティング

- **デフォルトゲートウェイ**: STA モードで取得したゲートウェイ
- **パケット転送**: AP ↔ STA 間の双方向転送
- **ファイアウォール**: 基本的なステートフルパケット検査

### 3.6 DNS 機能

- **DNS フォワーディング**: AP 接続クライアントの DNS クエリを STA 側の DNS サーバーに転送
- **DNS サーバーアドレス**: STA モードで DHCP から取得

### 3.7 Web UI (設定画面)

- **アクセス方法**: AP 接続後、http://192.168.4.1 にアクセス
- **設定項目**:
  - 既存 Wi-Fi の SSID 入力
  - 既存 Wi-Fi のパスワード入力
  - AP モードのパスワード変更（Phase 6 で追加）
  - 保存ボタン
  - 再起動ボタン
- **ステータス表示**:
  - STA 接続状態（接続済み/未接続）
  - STA IP アドレス
  - AP 接続クライアント数
  - 現在の AP パスワード（マスク表示）
  - メモリ使用量
- **UI デザイン**: シンプルな HTML フォーム（JavaScript 不要）

### 3.8 EEPROM 保存機能

- **保存先**: NVS (Preferences ライブラリを使用)
- **保存データ**:
  - STA モード SSID (最大 32 文字)
  - STA モード パスワード (最大 64 文字)
  - AP モード パスワード (最大 64 文字) - Phase 6 で追加
  - 設定済みフラグ
  - AP パスワード設定済みフラグ - Phase 6 で追加
- **データ永続化**: 電源オフ後も設定を保持
- **初期状態**: 未設定の場合は AP モードのみで起動、AP パスワードはデフォルト値 "esp32c6router" を使用

## 4. 非機能要件

### 4.1 パフォーマンス

- **スループット**: 最低 5 Mbps (Wi-Fi 条件による)
- **レイテンシ**: 追加遅延 < 10ms
- **メモリ使用量**: 空きヒープ > 50KB を維持

### 4.2 可用性

- **起動時間**: 電源投入から AP 起動まで < 10 秒
- **安定性**: 24 時間連続動作可能
- **エラーハンドリング**: 接続エラー時の自動復旧

### 4.3 保守性

- **シリアルログ**: 115200 bps でデバッグ情報出力
- **ステータス LED**: 動作状態の視覚的表示（可能であれば）
- **設定変更**: Web UI から設定変更可能

### 4.4 セキュリティ

- **Web UI**: 認証なし（AP 接続済みのデバイスのみアクセス可能）
- **パスワード保存**: NVS に暗号化されて保存（ESP32 標準機能）
- **入力検証**: SSID/パスワードの長さチェック

## 5. 制約事項

### 5.1 ハードウェア制約

- ESP32C6 のメモリ制限により、大量の同時接続は不可
- 無線性能は周囲の電波環境に依存

### 5.2 ソフトウェア制約

- Arduino フレームワークの制限により、高度なルーティング機能は実装困難
- NAT 機能の実装には ESP-IDF の netif API が必須
  - lwIP の低レベル NAT 関数は ESP32C6 環境では使用不可（TCP/IP コアロックエラーが発生）
  - `esp_netif_napt_enable()` を使用することで回避可能
  - Arduino Core 3.0.0 以降であれば `esp_netif.h` は利用可能
- Web UI は基本的な HTML のみ（高度な UI は不可）

### 5.3 セキュリティ制約

- WPA3 は ESP32C6 でサポート可能だが、互換性のため WPA2-PSK を使用
- ファイアウォール機能は基本的なもののみ
- Web UI は認証なし（AP 接続が必要なため一定のセキュリティは確保）

## 6. 設定項目

### 6.1 STA モード設定（Web UI で設定、EEPROM に保存）

- **SSID**: Web UI から入力（最大 32 文字）
- **パスワード**: Web UI から入力（最大 64 文字）
- **保存先**: Preferences (NVS)

**EEPROM データ構造**:

```cpp
// Preferences 名前空間: "wifi-config"
struct WifiConfig {
  char sta_ssid[33];      // 最大 32 文字 + null終端
  char sta_password[65];  // 最大 64 文字 + null終端
  char ap_password[65];   // 最大 64 文字 + null終端 (Phase 6)
  bool configured;        // 設定済みフラグ
  bool ap_password_set;   // AP パスワード設定済みフラグ (Phase 6)
};
```

### 6.2 AP モード設定（ソースコードで定義）

```cpp
#define AP_SSID "micro-router-esp32c6"
#define AP_PASSWORD "your_ap_password"  // 最低 8 文字
#define AP_CHANNEL 6
#define AP_MAX_CONNECTIONS 3
#define AP_IP "192.168.4.1"
#define AP_GATEWAY "192.168.4.1"
#define AP_SUBNET "255.255.255.0"
```

### 6.3 DHCP 設定

```cpp
#define DHCP_START_IP "192.168.4.2"
#define DHCP_END_IP "192.168.4.4"
#define DHCP_LEASE_TIME 7200
```

### 6.4 Web サーバー設定

```cpp
#define WEB_SERVER_PORT 80
#define WEB_UI_PATH "/"
```

## 7. 想定デバイス

| デバイス       | OS                | 想定用途                                         |
| -------------- | ----------------- | ------------------------------------------------ |
| PC             | Windows/Mac/Linux | 通常のインターネット利用、設定変更（Web UI）     |
| スマートフォン | iOS/Android       | モバイルアプリ、ブラウジング、設定変更（Web UI） |
| タブレット     | iOS/Android       | メディア視聴、ブラウジング                       |

## 8. 開発フェーズ

### Phase 1: 基本実装

- STA + AP 同時動作
- 基本的な接続性確認
- EEPROM 読み書き機能

### Phase 2: Web UI 実装

- Web サーバー起動
- 設定画面の実装
- ステータス表示

### Phase 3: ルーティング実装

- NAT 機能の有効化（ESP-IDF netif API を使用）
- パケット転送の実装
- **実装詳細**:
  - WiFi イベントハンドラで `ARDUINO_EVENT_WIFI_STA_GOT_IP` イベントを検知
  - loop() 関数から `esp_netif_napt_enable(ap_netif)` を呼び出し
  - 複数のキー名を試行して AP netif ハンドルを取得
  - エラーハンドリングと詳細なログ出力を実装

### Phase 4: 安定化

- エラーハンドリング
- 再接続ロジック
- メモリ管理

### Phase 5: 最適化

- パフォーマンスチューニング
- ログ機能の追加

### Phase 6: AP パスワード永続化

- AP パスワードの NVS 保存機能
- Web UI での AP パスワード変更機能
- デフォルトパスワード "esp32c6router" からの移行

## 9. 起動シーケンス

### 9.1 初回起動（EEPROM 未設定）

1. 電源投入
2. EEPROM から設定読み込み → 未設定
3. AP モードのみで起動
4. シリアルログ: "STA 未設定。Web UI で設定してください: http://192.168.4.1"
5. ユーザーが AP に接続
6. Web UI で SSID/パスワード入力
7. EEPROM に保存
8. 再起動

### 9.2 通常起動（EEPROM 設定済み）

1. 電源投入
2. EEPROM から設定読み込み → 設定済み
3. WiFi イベントハンドラを登録
4. STA モードで接続開始
5. AP モード起動
6. `ARDUINO_EVENT_WIFI_STA_GOT_IP` イベント発生
7. loop() から `esp_netif_napt_enable()` で NAT 有効化
8. 運用開始

### 9.3 STA 接続失敗時

1. STA 接続タイムアウト（30 秒）
2. AP モードは継続
3. Web UI で「STA 接続失敗」を表示
4. 再接続を自動リトライ（5 分間隔）

## 10. Web UI 画面仕様

### 10.1 トップページ (/)

```
=================================
  XIAO ESP32C6 マイクロルーター
=================================

【ステータス】
STA 接続: 接続済み / 未接続
STA IP: 192.168.10.100
AP クライアント数: 2 / 3
空きメモリ: 85KB

【Wi-Fi 設定】
既存 Wi-Fi SSID: [入力欄]
パスワード:      [入力欄(type=password)]

[保存して再起動]

=================================
```

### 10.2 保存成功ページ (/save)

```
=================================
設定を保存しました。
3 秒後に再起動します...
=================================
```

## 11. 成功基準

- [ ] 初回起動時、AP モードのみで起動する
- [ ] Web UI (http://192.168.4.1) にアクセスできる
- [ ] Web UI で SSID/パスワードを入力・保存できる
- [ ] 再起動後、保存した設定で STA モードに接続できる
- [ ] PC、スマホ、タブレットが AP に接続できる
- [ ] AP 経由でインターネットにアクセスできる
- [ ] DNS 解決が正常に動作する
- [ ] 24 時間の連続動作テストをクリア
- [ ] ping レイテンシが許容範囲内（+10ms 以下）
- [ ] Web UI でリアルタイムステータスが表示される
