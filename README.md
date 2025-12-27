# XIAO ESP32-C6 によるマイクロルーター

## 1. 概要

Seeed Studio XIAO ESP32C6 (RISC-V) を使用したミニマルな Wi-Fi ルータの実装です。

![image](./docs/IMG_0517.jpeg)

本プロジェクトは、Seeed Studio XIAO ESP32C6 を使って小型の Wi-Fi ルータを構築します。任意の Wi-Fi ネットワーク (e,g, 自宅のルータなど) に接続しながら、独自のアクセスポイントを提供し、最大 3 台のデバイスに対して NAT ルーティングを実現します。

マイコンボードのスペック不足から以下の制限があります。どうしても低速なネットワーク環境が必要な場合しか用途が思いつきません。しかし、DNS フィルタリングを実装したことで、低速でも広告ブロック機能を持つハードウェアセキュリティルータとして活用できます。

- 通信速度は 7 Mbps 前後
- 同時接続は 3 台程度

### 主な機能

- **デュアル Wi-Fi モード**: STA (Station) モードと AP (Access Point) モードの同時動作
- **Web ベース設定画面**: 初回起動時に Web UI から上位 Wi-Fi AP の認証情報を設定可能 (ハードコード不要でセキュア)
- **NAT ルーティング**: AP 配下のデバイスから既存ネットワーク経由でインターネットへアクセス可能
- **DHCP サーバー**: 192.168.4.0/24 セグメントで自動的に IP アドレスを割り当て
- **不揮発性ストレージ**: 設定情報をマイコンボードの NVS に保存し、再起動後も保持
- **DNS フィルタリング**: ドメインレベルでの広告・トラッキングブロック機能
  - カスタマイズ可能なブロックリスト (最大 5,000 ドメイン)
  - Web UI からのブロックリストアップロード機能
  - 統計情報の表示 (ブロック数、許可数)
  - HTTP/HTTPS 両方に対応
- **キャプティブポータル機能**: XIAO ESP32C6 の STA モードが Home Router に未接続の場合はキャプティポータル機能が有効状態になり、接続状態の場合は通常のルータになる
- **mDNS Responder**: mDNS Responder の実装で `micro-router.local` でアクセス可能

```text
┌────────────────────────────┐
│ Home Router                │
│ (Wi-Fi AP)                 │
└────────────────────────────┘
      ▲              ▲
┌─────┴───────┐┌─────┴───────┐
│ (Wi-Fi STA) ││ (Wi-Fi STA) │
│ XIAO        ││ PC          │
│ (Wi-Fi AP)  │└─────────────┘
└─────────────┘
      ▲
┌─────┴───────┐
│ (Wi-Fi STA) │
│ Any Devices │
└─────────────┘
```

## 2. ハードウェア

- Seeed Studio XIAO ESP32C6 (RISC-V CPU)
- USB-C to A 変換器 (場合によっては)

### 注意点

- XIAO ESP32C3 は ARM CPU なので、本ソースコードでは動かない
- 同様に XIAO ESP32S2, S3, H4 は Xtensa CPU なので、本ソースコードでは動かない

## 3. 使用方法

[usage.md 参照](./docs/USAGE.md)

### DNS フィルタリング機能の使用方法

#### ブロックリストの準備

1. **豆腐フィルタをダウンロード （推奨）**

```bash
curl -O https://raw.githubusercontent.com/tofukko/filter/master/Adblock_Plus_list.txt
```

2. **ドメインリストに変換**

```bash
python3 tools/convert_adblock_to_domains.py Adblock_Plus_list.txt domain.txt
```

このスクリプトは Adblock Plus 形式のフィルタリストから、本機が扱えるシンプルなドメインリスト (1 行 1 ドメイン形式) に変換します。

3. **Web UI からアップロード**

- ブラウザで `http://micro-router.local/dns-filter` にアクセス
- 「ブロックリスト管理」セクションで `domain.txt` をアップロード
- 自動的にブロックリストが更新されます

#### DNS フィルタの有効化

1. `http://micro-router.local/dns-filter` にアクセス
2. 「DNS フィルタを有効にする」にチェック
3. 「保存」ボタンをクリック

#### 統計情報の確認

DNS フィルタページで以下の統計情報を確認できます：

- 総クエリ数
- ブロック数 (パーセンテージ表示)
- 許可数 (パーセンテージ表示)
- ブロックリスト登録数

#### 動作確認

AP に接続したデバイスから以下のコマンドで動作を確認できます：

```bash
# ブロック対象ドメインのクエリ（0.0.0.0 が返る）
nslookup ads.google.com

# 許可対象ドメインのクエリ（正しい IP が返る）
nslookup www.google.com
```

#### トラブルシューティング

広告ブロックが機能しない場合は、[DNS フィルタリングトラブルシューティングガイド](./docs/dns_filtering_troubleshooting.md) を参照してください。
