# XIAO ESP32-C6 マイクロ Wi-Fi ルータ - システム仕様書

## 1. プロジェクト概要

**Project Name:** XIAO ESP32-C6 Micro Router
**Version:** Phase 8 (DNS Filtering Implemented)
**Target Hardware:** Seeed Studio XIAO ESP32C6 (RISC-V)

本プロジェクトは、ESP32-C6 上で動作する最小限の Wi-Fi ルータ実装です。上流の Wi-Fi ネットワーク (STA モード) に接続しながら、独自のアクセスポイント (AP モード) を提供し、接続されたクライアントに対して NAT ルーティングを行います。さらに、DNS ベースの広告ブロック機能 (DNS フィルタリング) を搭載しています。

## 2. ハードウェア仕様

| Component | Specification            | Notes                            |
| :-------- | :----------------------- | :------------------------------- |
| **SoC**   | ESP32-C6 (RISC-V 32-bit) | 160 MHz, Wi-Fi 6 (802.11ax)      |
| **Flash** | 4 MB                     | Used for code, NVS, and LittleFS |
| **SRAM**  | 512 KB                   | ~200 KB available for heap       |
| **Power** | USB-C (5V)               |                                  |

**制約事項:**

- **非互換ボード:** XIAO ESP32-C3, S3, および従来の ESP32 (アーキテクチャや SDK の違いによる)。
- **Wi-Fi 範囲:** オンボードアンテナによる制限あり。

## 3. ソフトウェアアーキテクチャ

本ソフトウェアは、ESP32 Arduino Core (v3.0.0+) を使用した Arduino フレームワーク上で構築されています。モジュール単位の「Manager」パターンを採用しています。

| Module               | Description                                                                                        | Source Files               |
| :------------------- | :------------------------------------------------------------------------------------------------- | :------------------------- |
| **WiFiManager**      | Manages dual-mode Wi-Fi (AP+STA) and auto-reconnection logic.                                      | `WiFiManager.h/.cpp`       |
| **NATManager**       | Enables NAPT (Network Address Port Translation) using ESP-IDF `esp_netif` APIs.                    | `NATManager.h/.cpp`        |
| **DNSFilterManager** | Intercepts DNS queries on UDP:53, filters domains against a blocklist, and forwards valid queries. | `DNSFilterManager.h/.cpp`  |
| **WebUIManager**     | Provides the HTTP interface (port 80) for configuration, status monitoring, and file uploads.      | `WebUIManager.h/.cpp`      |
| **ConfigManager**    | Handles persistent storage of settings using NVS (Preferences).                                    | `ConfigManager.h/.cpp`     |
| **Main Loop**        | Handles task scheduling and periodic status updates.                                               | `micro-router-esp32c6.ino` |

## 4. ネットワーク仕様

### 4.1 インターフェース設定

| Interface         | Role         | IP Assignment | Default Config                            |
| :---------------- | :----------- | :------------ | :---------------------------------------- |
| **STA (Station)** | Upstream WAN | DHCP Client   | Connects to existing home router          |
| **AP (SoftAP)**   | LAN Gateway  | Static IP     | IP: `192.168.4.1` Subnet: `255.255.255.0` |

### 4.2 DHCP サーバ (LAN)

ESP32-C6 は AP インターフェース上で DHCP サーバを実行し、クライアントに IP を割り当てます。

| Parameter       | Value                                         |
| :-------------- | :-------------------------------------------- |
| **IP Range**    | `192.168.4.2` - `192.168.4.4`                 |
| **Max Clients** | 3 (Hardcoded limit for memory safety)         |
| **Lease Time**  | 7200 seconds (2 hours)                        |
| **DNS Server**  | Advertises `192.168.4.1` (Self) for filtering |

### 4.3 NAT / ルーティング

- **技術:** ESP-IDF `esp_netif` NAPT を使用。
- **メカニズム:** LAN トラフィック (192.168.4.x) を STA インターフェースの IP にマスカレードします。
- **有効化:** STA が IP アドレスを取得した時点で自動的に有効化されます。

## 5. DNS フィルタリング (広告ブロック)

UDP ポート 53 で動作するカスタム DNS プロキシです。

| Feature             | Description                                                                            |
| :------------------ | :------------------------------------------------------------------------------------- |
| **Filtering Logic** | Matches query domains against a loaded blocklist.                                      |
| **Block Action**    | Returns `0.0.0.0` (A record) for blocked domains.                                      |
| **Pass Action**     | Forwards queries to the upstream DNS (e.g., 8.8.8.8) and relays the response.          |
| **Storage**         | Blocklist stored in `LittleFS` as `/blocklist.txt`.                                    |
| **Capacity**        | ~5,000 domains (Limited by RAM buffer size of 128KB).                                  |
| **Captive Portal**  | Redirects all DNS queries to `192.168.4.1` if internet is unavailable or unconfigured. |

## 6. Web インターフェース (Web UI)

`http://192.168.4.1` (または `http://micro-router.local`) でアクセス可能です。

- AP パスワードが初期値 `esp32c6router` の間は、通常の Web UI ではなく AP パスワード変更専用画面を表示します。
- AP パスワード変更後は、フォームログインとセッション cookie による認証を要求します。
- ログインに使う秘密情報は現在の AP パスワードです。

### 6.1 エンドポイント

| Method | Path                  | Description                                             |
| :----- | :-------------------- | :------------------------------------------------------ |
| `GET`  | `/`                   | **Dashboard**: Status, Wi-Fi setup, AP password change. |
| `GET`  | `/login`              | **Login**: Web UI ログイン画面。                        |
| `POST` | `/login`              | Creates a session after AP password verification.       |
| `POST` | `/logout`             | Clears the current Web UI session.                      |
| `POST` | `/save`               | Saves STA credentials and reboots.                      |
| `POST` | `/save_ap_password`   | Saves new AP password and reboots.                      |
| `GET`  | `/dns-filter`         | **DNS Filter**: Stats, toggle switch, upload form.      |
| `POST` | `/dns-filter-toggle`  | Enables/Disables DNS filtering.                         |
| `POST` | `/upload-blocklist`   | Uploads `domain.txt` (multipart/form-data).             |
| `GET`  | `/download-blocklist` | Downloads the current blocklist.                        |

- 状態変更系エンドポイントでは、セッション確認に加えて CSRF トークン検証と `Origin` / `Referer` 検証を行います。
- AP パスワード変更後の既存セッションは破棄し、再ログインを要求します。
- ブロックリスト upload は AP サブネットからの要求に限定し、サイズ上限は `128KB` とします。

## 7. 設定の永続化 (NVS)

設定は `wifi-config` 名前空間に保存されます。

| Key             | Type   | Description                                     |
| :-------------- | :----- | :---------------------------------------------- |
| `sta_ssid_enc`  | String | 端末固有鍵で暗号化した Upstream Wi-Fi SSID.     |
| `sta_pwd_enc`   | String | 端末固有鍵で暗号化した Upstream Wi-Fi Password. |
| `ap_pwd_enc`    | String | 端末固有鍵で暗号化した Custom AP Password.      |
| `configured`    | Bool   | `true` if STA credentials are set.              |
| `ap_pw_set`     | Bool   | `true` if custom AP password is set.            |
| `dns_filter_en` | Bool   | `true` if DNS filtering is enabled.             |

旧バージョンの `sta_ssid`, `sta_password`, `ap_password` は互換目的で読み込みのみ対応し、起動時に暗号化形式へ自動移行されます。

## 8. 起動シーケンス

1. **初期化:** シリアル、LittleFS、Preferences の初期化。
2. **設定確認:** NVS から設定をロード。
3. **モード選択:**
   - **設定済み:** STA 接続開始 + AP 起動。
   - **未設定:** AP 起動のみ。
4. **サービス:** Web サーバ、DNS フィルタ (プロキシ)、mDNS の開始。
5. **イベントループ:**
   - STA が IP 取得 -> NAT 有効化。
   - STA 切断 -> 再試行ループ (5 秒間隔)。
   - DNS パケット処理。
   - Web リクエスト処理。

### 7.1 保存保護

- `sta_ssid_enc`、`sta_pwd_enc`、`ap_pwd_enc` は端末固有鍵で暗号化して保存されます。
- 暗号化鍵はデバイス固有情報から導出されるため、他個体へ保存領域を複製してもそのまま復号できません。
- 旧平文キーは後方互換のため読み込みのみ許可し、起動時に暗号化形式へ自動移行されます。

## 9. パフォーマンスと制限

| Metric               | Limit / Typical Value                          |
| :------------------- | :--------------------------------------------- |
| **Wi-Fi Throughput** | ~5-10 Mbps (NAT overhead + antenna limit)      |
| **Latency**          | < 10ms added latency                           |
| **Free Heap**        | Maintenance target > 50 KB (Typically ~200 KB) |
| **Boot Time**        | < 5 seconds                                    |

- 上流 DNS 応答待ちは最大 `500ms` に制限します。

## 10. 開発環境

- **IDE:** Arduino IDE 2.x または PlatformIO。
- **Core:** ESP32 Arduino Core v3.0.0+。
- **Partition Scheme:** デフォルト 4MB (SPIFFS/LittleFS 含む)。
